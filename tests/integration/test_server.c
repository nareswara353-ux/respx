#include "kave/commands.h"
#include "kave/hash_table.h"
#include "kave/resp_parser.h"
#include "kave/allocator.h"
#include "kave/sds.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static int failures = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        failures++; \
    } \
} while (0)

static resp_value *make_array(int count, ...)
{
    resp_value *arr = kave_malloc(sizeof(resp_value));
    if (!arr) return NULL;
    arr->type = RESP_ARRAY;
    arr->array.items = kave_malloc(sizeof(resp_value *) * (count > 0 ? count : 1));
    if (!arr->array.items) {
        kave_free(arr);
        return NULL;
    }
    arr->array.count = count;
    va_list ap;
    va_start(ap, count);
    for (int i = 0; i < count; i++) {
        arr->array.items[i] = va_arg(ap, resp_value *);
    }
    va_end(ap);
    return arr;
}

static resp_value *make_bulk(const char *s)
{
    resp_value *v = kave_malloc(sizeof(resp_value));
    if (!v) return NULL;
    v->type = RESP_BULK_STRING;
    size_t len = s ? strlen(s) : 0;
    v->bulk.ptr = kave_malloc(len + 1);
    if (!v->bulk.ptr) {
        kave_free(v);
        return NULL;
    }
    if (s) memcpy(v->bulk.ptr, s, len);
    v->bulk.ptr[len] = '\0';
    v->bulk.len = len;
    return v;
}

int main(void)
{
    ht *storage = ht_new(16);
    ASSERT(storage != NULL, "storage created");

    resp_value *args = make_array(2, make_bulk("mykey"), make_bulk("myvalue"));
    command_result *res = command_dispatch(storage, "SET", args, NULL);
    ASSERT(res != NULL && res->success, "SET succeeds");
    resp_value_free(args);
    command_result_free(res);

    args = make_array(1, make_bulk("mykey"));
    res = command_dispatch(storage, "GET", args, NULL);
    ASSERT(res != NULL && res->success, "GET succeeds");
    ASSERT(res->response->type == RESP_BULK_STRING, "GET returns bulk");
    ASSERT(memcmp(res->response->bulk.ptr, "myvalue", 7) == 0, "GET value correct");
    resp_value_free(args);
    command_result_free(res);

    args = make_array(1, make_bulk("nonexistent"));
    res = command_dispatch(storage, "GET", args, NULL);
    ASSERT(res != NULL && res->success, "GET missing succeeds");
    ASSERT(res->response->type == RESP_NULL, "GET missing returns null");
    resp_value_free(args);
    command_result_free(res);

    args = make_array(1, make_bulk("counter"));
    res = command_dispatch(storage, "INCR", args, NULL);
    ASSERT(res != NULL && res->success, "INCR creates key");
    ASSERT(res->response->integer == 1, "INCR new key is 1");
    resp_value_free(args);
    command_result_free(res);

    args = make_array(1, make_bulk("counter"));
    res = command_dispatch(storage, "INCR", args, NULL);
    ASSERT(res->response->integer == 2, "INCR again is 2");
    resp_value_free(args);
    command_result_free(res);

    args = make_array(1, make_bulk("counter"));
    res = command_dispatch(storage, "INCR", args, NULL);
    ASSERT(res->response->integer == 3, "INCR third time is 3");
    resp_value_free(args);
    command_result_free(res);

    args = make_array(1, make_bulk("mykey"));
    res = command_dispatch(storage, "DEL", args, NULL);
    ASSERT(res != NULL && res->success, "DEL succeeds");
    ASSERT(res->response->integer == 1, "DEL deleted 1 key");
    resp_value_free(args);
    command_result_free(res);

    args = make_array(1, make_bulk("mykey"));
    res = command_dispatch(storage, "GET", args, NULL);
    ASSERT(res->response->type == RESP_NULL, "GET after DEL returns null");
    resp_value_free(args);
    command_result_free(res);

    args = make_array(1, make_bulk("mykey"));
    res = command_dispatch(storage, "DEL", args, NULL);
    ASSERT(res->response->integer == 0, "DEL missing returns 0");
    resp_value_free(args);
    command_result_free(res);

    resp_value *setkey = make_bulk("myzset");
    resp_value *s1 = make_bulk("1.5");
    resp_value *m1 = make_bulk("alice");
    resp_value *s2 = make_bulk("2.5");
    resp_value *m2 = make_bulk("bob");
    args = make_array(5, setkey, s1, m1, s2, m2);
    res = command_dispatch(storage, "ZADD", args, NULL);
    ASSERT(res != NULL && res->success, "ZADD succeeds");
    ASSERT(res->response->integer == 2, "ZADD added 2 members");
    resp_value_free(args);
    command_result_free(res);

    args = make_array(1, make_bulk("myzset"));
    res = command_dispatch(storage, "ZRANGE", args, NULL);
    ASSERT(res != NULL && res->success, "ZRANGE succeeds");
    ASSERT(res->response->type == RESP_ARRAY, "ZRANGE returns array");
    ASSERT(res->response->array.count == 4, "ZRANGE has 4 items");
    resp_value_free(args);
    command_result_free(res);

    args = make_array(0);
    res = command_dispatch(storage, "UNKNOWNCMD", args, NULL);
    ASSERT(res != NULL && !res->success, "unknown command fails");
    resp_value_free(args);
    command_result_free(res);

    ht_free(storage);

    kave_alloc_stats stats = kave_get_global_stats();
    ASSERT(stats.active_bytes == 0, "no leaked bytes");
    ASSERT(stats.active_blocks == 0, "no leaked blocks");

    if (failures == 0) {
        printf("test_server: OK\n");
        return 0;
    }
    fprintf(stderr, "test_server: %d failures\n", failures);
    return 1;
}
