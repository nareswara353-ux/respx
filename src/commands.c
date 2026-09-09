#include "kave/commands.h"
#include "kave/allocator.h"
#include "kave/sds.h"
#include "kave/skiplist.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static command_result *result_new(int success, resp_value *response, const char *error)
{
    command_result *res = kave_malloc(sizeof(command_result));
    if (!res) return NULL;
    res->success = success;
    res->response = response;
    if (error) {
        res->error_msg = sds_new(error);
    } else {
        res->error_msg = NULL;
    }
    return res;
}

static resp_value *resp_bulk_string(const char *s)
{
    resp_value *v = kave_malloc(sizeof(resp_value));
    if (!v) return NULL;
    v->type = RESP_BULK_STRING;
    v->bulk.ptr = sds_new(s);
    v->bulk.len = s ? strlen(s) : 0;
    return v;
}

static resp_value *resp_integer(long long n)
{
    resp_value *v = kave_malloc(sizeof(resp_value));
    if (!v) return NULL;
    v->type = RESP_INTEGER;
    v->integer = n;
    return v;
}

static resp_value *resp_null(void)
{
    resp_value *v = kave_malloc(sizeof(resp_value));
    if (!v) return NULL;
    v->type = RESP_NULL;
    return v;
}

static resp_value *resp_simple_string(const char *s)
{
    resp_value *v = kave_malloc(sizeof(resp_value));
    if (!v) return NULL;
    v->type = RESP_STRING;
    v->string = sds_new(s);
    return v;
}

command_result *cmd_get(ht *storage, const resp_value *args, void *ctx)
{
    (void)ctx;
    if (!args || args->array.count < 1) {
        return result_new(0, resp_simple_string("ERR wrong number of arguments"), NULL);
    }
    resp_value *key_arg = args->array.items[0];
    if (key_arg->type != RESP_BULK_STRING && key_arg->type != RESP_STRING) {
        return result_new(0, resp_simple_string("ERR invalid key type"), NULL);
    }
    const char *key = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.ptr : key_arg->string;
    size_t key_len = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.len : strlen(key);
    void *val = ht_find(storage, key, key_len);
    if (!val) {
        return result_new(1, resp_null(), NULL);
    }
    return result_new(1, resp_bulk_string((char *)val), NULL);
}

command_result *cmd_set(ht *storage, const resp_value *args, void *ctx)
{
    (void)ctx;
    if (!args || args->array.count < 2) {
        return result_new(0, resp_simple_string("ERR wrong number of arguments"), NULL);
    }
    resp_value *key_arg = args->array.items[0];
    resp_value *val_arg = args->array.items[1];
    if ((key_arg->type != RESP_BULK_STRING && key_arg->type != RESP_STRING) ||
        (val_arg->type != RESP_BULK_STRING && val_arg->type != RESP_STRING)) {
        return result_new(0, resp_simple_string("ERR invalid argument type"), NULL);
    }
    const char *key = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.ptr : key_arg->string;
    size_t key_len = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.len : strlen(key);
    const char *val = val_arg->type == RESP_BULK_STRING ? val_arg->bulk.ptr : val_arg->string;
    size_t val_len = val_arg->type == RESP_BULK_STRING ? val_arg->bulk.len : strlen(val);
    char *val_copy = sds_new_len(val, val_len);
    if (!val_copy) {
        return result_new(0, resp_simple_string("ERR memory allocation failed"), NULL);
    }
    if (ht_insert(storage, key, key_len, val_copy) < 0) {
        sds_free(val_copy);
        return result_new(0, resp_simple_string("ERR failed to insert"), NULL);
    }
    return result_new(1, resp_simple_string("OK"), NULL);
}

command_result *cmd_del(ht *storage, const resp_value *args, void *ctx)
{
    (void)ctx;
    if (!args || args->array.count < 1) {
        return result_new(0, resp_simple_string("ERR wrong number of arguments"), NULL);
    }
    int deleted = 0;
    for (size_t i = 0; i < args->array.count; i++) {
        resp_value *key_arg = args->array.items[i];
        if (key_arg->type != RESP_BULK_STRING && key_arg->type != RESP_STRING) continue;
        const char *key = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.ptr : key_arg->string;
        size_t key_len = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.len : strlen(key);
        if (ht_delete(storage, key, key_len) == 0) {
            deleted++;
        }
    }
    return result_new(1, resp_integer(deleted), NULL);
}

command_result *cmd_mget(ht *storage, const resp_value *args, void *ctx)
{
    (void)ctx;
    if (!args || args->array.count < 1) {
        return result_new(0, resp_simple_string("ERR wrong number of arguments"), NULL);
    }
    resp_value *result = kave_malloc(sizeof(resp_value));
    if (!result) return NULL;
    result->type = RESP_ARRAY;
    result->array.items = kave_malloc(args->array.count * sizeof(resp_value *));
    if (!result->array.items) {
        kave_free(result);
        return NULL;
    }
    result->array.count = 0;
    for (size_t i = 0; i < args->array.count; i++) {
        resp_value *key_arg = args->array.items[i];
        if (key_arg->type != RESP_BULK_STRING && key_arg->type != RESP_STRING) {
            result->array.items[result->array.count++] = resp_null();
            continue;
        }
        const char *key = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.ptr : key_arg->string;
        size_t key_len = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.len : strlen(key);
        void *val = ht_find(storage, key, key_len);
        if (val) {
            result->array.items[result->array.count++] = resp_bulk_string((char *)val);
        } else {
            result->array.items[result->array.count++] = resp_null();
        }
    }
    return result_new(1, result, NULL);
}

command_result *cmd_expire(ht *storage, const resp_value *args, void *ctx)
{
    (void)ctx;
    (void)storage;
    if (!args || args->array.count < 2) {
        return result_new(0, resp_simple_string("ERR wrong number of arguments"), NULL);
    }
    return result_new(1, resp_integer(0), NULL);
}

command_result *cmd_incr(ht *storage, const resp_value *args, void *ctx)
{
    (void)ctx;
    if (!args || args->array.count < 1) {
        return result_new(0, resp_simple_string("ERR wrong number of arguments"), NULL);
    }
    resp_value *key_arg = args->array.items[0];
    if (key_arg->type != RESP_BULK_STRING && key_arg->type != RESP_STRING) {
        return result_new(0, resp_simple_string("ERR invalid key type"), NULL);
    }
    const char *key = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.ptr : key_arg->string;
    size_t key_len = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.len : strlen(key);
    void *val = ht_find(storage, key, key_len);
    long long num;
    if (!val) {
        num = 0;
    } else {
        char *end;
        num = strtoll((char *)val, &end, 10);
        if (*end != '\0') {
            return result_new(0, resp_simple_string("ERR value is not an integer"), NULL);
        }
    }
    num++;
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", num);
    char *new_val = sds_new(buf);
    if (!new_val) {
        return result_new(0, resp_simple_string("ERR memory allocation failed"), NULL);
    }
    if (val) sds_free(val);
    if (ht_insert(storage, key, key_len, new_val) < 0) {
        sds_free(new_val);
        return result_new(0, resp_simple_string("ERR failed to insert"), NULL);
    }
    return result_new(1, resp_integer(num), NULL);
}

command_result *cmd_zadd(ht *storage, const resp_value *args, void *ctx)
{
    (void)ctx;
    if (!args || args->array.count < 3) {
        return result_new(0, resp_simple_string("ERR wrong number of arguments"), NULL);
    }
    resp_value *key_arg = args->array.items[0];
    if (key_arg->type != RESP_BULK_STRING && key_arg->type != RESP_STRING) {
        return result_new(0, resp_simple_string("ERR invalid key type"), NULL);
    }
    const char *key = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.ptr : key_arg->string;
    size_t key_len = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.len : strlen(key);
    skiplist *zset = (skiplist *)ht_find(storage, key, key_len);
    if (!zset) {
        zset = sl_new();
        if (!zset) {
            return result_new(0, resp_simple_string("ERR memory allocation failed"), NULL);
        }
        if (ht_insert(storage, key, key_len, zset) < 0) {
            sl_free(zset);
            return result_new(0, resp_simple_string("ERR failed to insert"), NULL);
        }
    }
    int added = 0;
    for (size_t i = 1; i + 1 < args->array.count; i += 2) {
        resp_value *score_arg = args->array.items[i];
        resp_value *member_arg = args->array.items[i+1];
        if ((score_arg->type != RESP_BULK_STRING && score_arg->type != RESP_STRING) ||
            (member_arg->type != RESP_BULK_STRING && member_arg->type != RESP_STRING)) {
            continue;
        }
        double score = strtod(score_arg->type == RESP_BULK_STRING ? score_arg->bulk.ptr : score_arg->string, NULL);
        const char *member = member_arg->type == RESP_BULK_STRING ? member_arg->bulk.ptr : member_arg->string;
        char *member_copy = sds_new(member);
        if (!member_copy) continue;
        if (sl_insert(zset, score, member, member_copy) == 0) {
            added++;
        } else {
            sds_free(member_copy);
        }
    }
    return result_new(1, resp_integer(added), NULL);
}

command_result *cmd_zrange(ht *storage, const resp_value *args, void *ctx)
{
    (void)ctx;
    if (!args || args->array.count < 3) {
        return result_new(0, resp_simple_string("ERR wrong number of arguments"), NULL);
    }
    resp_value *key_arg = args->array.items[0];
    if (key_arg->type != RESP_BULK_STRING && key_arg->type != RESP_STRING) {
        return result_new(0, resp_simple_string("ERR invalid key type"), NULL);
    }
    const char *key = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.ptr : key_arg->string;
    size_t key_len = key_arg->type == RESP_BULK_STRING ? key_arg->bulk.len : strlen(key);
    skiplist *zset = (skiplist *)ht_find(storage, key, key_len);
    if (!zset) {
        return result_new(1, resp_simple_string("OK"), NULL);
    }
    resp_value *result = kave_malloc(sizeof(resp_value));
    if (!result) return NULL;
    result->type = RESP_ARRAY;
    size_t count = sl_count(zset);
    result->array.items = kave_malloc(count * 2 * sizeof(resp_value *));
    if (!result->array.items) {
        kave_free(result);
        return NULL;
    }
    result->array.count = 0;
    skiplist_node *node = sl_first(zset);
    while (node) {
        result->array.items[result->array.count++] = resp_bulk_string(node->key);
        char score_buf[64];
        snprintf(score_buf, sizeof(score_buf), "%f", node->score);
        result->array.items[result->array.count++] = resp_bulk_string(score_buf);
        node = sl_next(node);
    }
    return result_new(1, result, NULL);
}

static struct cmd_entry {
    const char *name;
    command_fn fn;
} cmd_table[] = {
    {"GET", cmd_get},
    {"SET", cmd_set},
    {"DEL", cmd_del},
    {"MGET", cmd_mget},
    {"EXPIRE", cmd_expire},
    {"INCR", cmd_incr},
    {"ZADD", cmd_zadd},
    {"ZRANGE", cmd_zrange},
    {NULL, NULL}
};

command_result *command_dispatch(ht *storage, const char *cmd_name, const resp_value *args, void *ctx)
{
    if (!storage || !cmd_name) {
        return result_new(0, resp_simple_string("ERR internal error"), NULL);
    }
    for (int i = 0; cmd_table[i].name; i++) {
        if (strcasecmp(cmd_table[i].name, cmd_name) == 0) {
            return cmd_table[i].fn(storage, args, ctx);
        }
    }
    char err_msg[128];
    snprintf(err_msg, sizeof(err_msg), "ERR unknown command '%s'", cmd_name);
    return result_new(0, resp_simple_string(err_msg), NULL);
}

void command_result_free(command_result *res)
{
    if (!res) return;
    if (res->response) {
        resp_value_free(res->response);
    }
    if (res->error_msg) {
        sds_free(res->error_msg);
    }
    kave_free(res);
}
