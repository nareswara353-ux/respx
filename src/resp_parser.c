#include "kave/resp_parser.h"
#include "kave/allocator.h"
#include "kave/sds.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define RESP_PARSE_INITIAL 4096
#define RESP_PARSE_MAX 1048576

static int resp_parser_grow(resp_parser *p, size_t needed)
{
    if (needed <= p->buf_cap) return 0;
    size_t new_cap = p->buf_cap * 2;
    if (new_cap < needed) new_cap = needed;
    if (new_cap > RESP_PARSE_MAX) return -1;
    char *new_buf = kave_realloc(p->buffer, new_cap);
    if (!new_buf) return -1;
    p->buffer = new_buf;
    p->buf_cap = new_cap;
    return 0;
}

resp_parser *resp_parser_new(void)
{
    resp_parser *p = kave_malloc(sizeof(resp_parser));
    if (!p) return NULL;
    p->buffer = kave_malloc(RESP_PARSE_INITIAL);
    if (!p->buffer) {
        kave_free(p);
        return NULL;
    }
    p->buf_len = 0;
    p->buf_cap = RESP_PARSE_INITIAL;
    p->pos = 0;
    p->state = 0;
    return p;
}

void resp_parser_free(resp_parser *p)
{
    if (!p) return;
    if (p->buffer) kave_free(p->buffer);
    kave_free(p);
}

int resp_parser_feed(resp_parser *p, const char *data, size_t len)
{
    if (!p || !data || len == 0) return -1;
    if (p->buf_len + len > RESP_PARSE_MAX) return -1;
    if (resp_parser_grow(p, p->buf_len + len + 1) < 0) return -1;
    memcpy(p->buffer + p->buf_len, data, len);
    p->buf_len += len;
    p->buffer[p->buf_len] = '\0';
    return 0;
}

static int resp_read_line(resp_parser *p, char **line, size_t *line_len)
{
    size_t start = p->pos;
    while (p->pos < p->buf_len && p->buffer[p->pos] != '\r') p->pos++;
    if (p->pos >= p->buf_len || p->buffer[p->pos] != '\r') return -1;
    if (p->pos + 1 >= p->buf_len || p->buffer[p->pos + 1] != '\n') return -1;
    *line = p->buffer + start;
    *line_len = p->pos - start;
    p->pos += 2;
    return 0;
}

static long long resp_parse_int(const char *s, size_t len)
{
    long long n = 0;
    int sign = 1;
    size_t i = 0;
    if (len > 0 && s[0] == '-') {
        sign = -1;
        i++;
    }
    for (; i < len; i++) {
        if (!isdigit(s[i])) return 0;
        n = n * 10 + (s[i] - '0');
    }
    return n * sign;
}

static resp_value *resp_parse_bulk(resp_parser *p, size_t len)
{
    if (len > RESP_PARSE_MAX) return NULL;
    if (p->pos + len + 2 > p->buf_len) return NULL;
    resp_value *v = kave_malloc(sizeof(resp_value));
    if (!v) return NULL;
    v->type = RESP_BULK_STRING;
    v->bulk.ptr = kave_malloc(len + 1);
    if (!v->bulk.ptr) {
        kave_free(v);
        return NULL;
    }
    memcpy(v->bulk.ptr, p->buffer + p->pos, len);
    v->bulk.ptr[len] = '\0';
    v->bulk.len = len;
    p->pos += len;
    if (p->pos + 2 > p->buf_len || p->buffer[p->pos] != '\r' || p->buffer[p->pos+1] != '\n') {
        kave_free(v->bulk.ptr);
        kave_free(v);
        return NULL;
    }
    p->pos += 2;
    return v;
}

static resp_value *resp_parse_array(resp_parser *p, size_t count)
{
    if (count > 1024) return NULL;
    resp_value *v = kave_malloc(sizeof(resp_value));
    if (!v) return NULL;
    v->type = RESP_ARRAY;
    v->array.items = kave_calloc(count, sizeof(resp_value *));
    if (!v->array.items) {
        kave_free(v);
        return NULL;
    }
    v->array.count = 0;
    for (size_t i = 0; i < count; i++) {
        resp_value *item = resp_parser_parse(p);
        if (!item) {
            for (size_t j = 0; j < v->array.count; j++) {
                resp_value_free(v->array.items[j]);
            }
            kave_free(v->array.items);
            kave_free(v);
            return NULL;
        }
        v->array.items[i] = item;
        v->array.count++;
    }
    return v;
}

resp_value *resp_parser_parse(resp_parser *p)
{
    if (!p || p->pos >= p->buf_len) return NULL;
    char type = p->buffer[p->pos];
    p->pos++;
    if (p->pos >= p->buf_len) return NULL;
    char *line;
    size_t line_len;
    long long n;
    resp_value *v = NULL;
    switch (type) {
        case '+':
            if (resp_read_line(p, &line, &line_len) < 0) return NULL;
            v = kave_malloc(sizeof(resp_value));
            if (!v) return NULL;
            v->type = RESP_STRING;
            v->string = kave_malloc(line_len + 1);
            if (!v->string) {
                kave_free(v);
                return NULL;
            }
            memcpy(v->string, line, line_len);
            v->string[line_len] = '\0';
            return v;
        case '-':
            if (resp_read_line(p, &line, &line_len) < 0) return NULL;
            v = kave_malloc(sizeof(resp_value));
            if (!v) return NULL;
            v->type = RESP_ERROR;
            v->string = kave_malloc(line_len + 1);
            if (!v->string) {
                kave_free(v);
                return NULL;
            }
            memcpy(v->string, line, line_len);
            v->string[line_len] = '\0';
            return v;
        case ':':
            if (resp_read_line(p, &line, &line_len) < 0) return NULL;
            v = kave_malloc(sizeof(resp_value));
            if (!v) return NULL;
            v->type = RESP_INTEGER;
            v->integer = resp_parse_int(line, line_len);
            return v;
        case '$':
            if (resp_read_line(p, &line, &line_len) < 0) return NULL;
            n = resp_parse_int(line, line_len);
            if (n < 0) {
                v = kave_malloc(sizeof(resp_value));
                if (!v) return NULL;
                v->type = RESP_NULL;
                return v;
            }
            return resp_parse_bulk(p, (size_t)n);
        case '*':
            if (resp_read_line(p, &line, &line_len) < 0) return NULL;
            n = resp_parse_int(line, line_len);
            if (n < 0) {
                v = kave_malloc(sizeof(resp_value));
                if (!v) return NULL;
                v->type = RESP_NULL;
                return v;
            }
            return resp_parse_array(p, (size_t)n);
        case '#':
            if (resp_read_line(p, &line, &line_len) < 0) return NULL;
            v = kave_malloc(sizeof(resp_value));
            if (!v) return NULL;
            v->type = RESP_BOOLEAN;
            v->integer = (line_len == 1 && line[0] == 't') ? 1 : 0;
            return v;
        case ',':
            if (resp_read_line(p, &line, &line_len) < 0) return NULL;
            v = kave_malloc(sizeof(resp_value));
            if (!v) return NULL;
            v->type = RESP_DOUBLE;
            v->real = strtod(line, NULL);
            return v;
        case '|':
            if (resp_read_line(p, &line, &line_len) < 0) return NULL;
            n = resp_parse_int(line, line_len);
            if (n < 0 || n > 1024) return NULL;
            v = kave_malloc(sizeof(resp_value));
            if (!v) return NULL;
            v->type = RESP_MAP;
            v->map.keys = kave_calloc((size_t)n, sizeof(resp_value *));
            v->map.values = kave_calloc((size_t)n, sizeof(resp_value *));
            if (!v->map.keys || !v->map.values) {
                if (v->map.keys) kave_free(v->map.keys);
                if (v->map.values) kave_free(v->map.values);
                kave_free(v);
                return NULL;
            }
            v->map.count = 0;
            for (size_t i = 0; i < (size_t)n; i++) {
                resp_value *key = resp_parser_parse(p);
                if (!key) {
                    for (size_t j = 0; j < v->map.count; j++) {
                        resp_value_free(v->map.keys[j]);
                        resp_value_free(v->map.values[j]);
                    }
                    kave_free(v->map.keys);
                    kave_free(v->map.values);
                    kave_free(v);
                    return NULL;
                }
                resp_value *val = resp_parser_parse(p);
                if (!val) {
                    resp_value_free(key);
                    for (size_t j = 0; j < v->map.count; j++) {
                        resp_value_free(v->map.keys[j]);
                        resp_value_free(v->map.values[j]);
                    }
                    kave_free(v->map.keys);
                    kave_free(v->map.values);
                    kave_free(v);
                    return NULL;
                }
                v->map.keys[i] = key;
                v->map.values[i] = val;
                v->map.count++;
            }
            return v;
        default:
            return NULL;
    }
}

void resp_value_free(resp_value *v)
{
    if (!v) return;
    switch (v->type) {
        case RESP_STRING:
        case RESP_ERROR:
            if (v->string) kave_free(v->string);
            break;
        case RESP_BULK_STRING:
            if (v->bulk.ptr) kave_free(v->bulk.ptr);
            break;
        case RESP_ARRAY:
            for (size_t i = 0; i < v->array.count; i++) {
                resp_value_free(v->array.items[i]);
            }
            if (v->array.items) kave_free(v->array.items);
            break;
        case RESP_MAP:
            for (size_t i = 0; i < v->map.count; i++) {
                resp_value_free(v->map.keys[i]);
                resp_value_free(v->map.values[i]);
            }
            if (v->map.keys) kave_free(v->map.keys);
            if (v->map.values) kave_free(v->map.values);
            break;
        default:
            break;
    }
    kave_free(v);
}

char *resp_value_to_string(const resp_value *v)
{
    if (!v) return NULL;
    char buf[128];
    switch (v->type) {
        case RESP_STRING:
        case RESP_ERROR:
            return sds_new(v->string);
        case RESP_INTEGER:
            snprintf(buf, sizeof(buf), "%lld", v->integer);
            return sds_new(buf);
        case RESP_BULK_STRING:
            return sds_new_len(v->bulk.ptr, v->bulk.len);
        case RESP_NULL:
            return sds_new("(null)");
        case RESP_BOOLEAN:
            return sds_new(v->integer ? "true" : "false");
        case RESP_DOUBLE:
            snprintf(buf, sizeof(buf), "%f", v->real);
            return sds_new(buf);
        case RESP_ARRAY:
            return sds_new("[array]");
        case RESP_MAP:
            return sds_new("{map}");
        default:
            return sds_new("(unknown)");
    }
}
