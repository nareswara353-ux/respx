#include "kave/resp_parser.h"
#include "kave/allocator.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        failures++; \
    } \
} while (0)

static resp_value *parse_single(const char *input)
{
    resp_parser *p = resp_parser_new();
    if (!p) return NULL;
    if (resp_parser_feed(p, input, strlen(input)) < 0) {
        resp_parser_free(p);
        return NULL;
    }
    resp_value *v = resp_parser_parse(p);
    resp_parser_free(p);
    return v;
}

int main(void)
{
    resp_value *v = parse_single("+OK\r\n");
    ASSERT(v != NULL, "simple string parsed");
    ASSERT(v->type == RESP_STRING, "type is STRING");
    ASSERT(strcmp(v->string, "OK") == 0, "content is OK");
    resp_value_free(v);

    v = parse_single("-ERR unknown\r\n");
    ASSERT(v != NULL, "error parsed");
    ASSERT(v->type == RESP_ERROR, "type is ERROR");
    ASSERT(strcmp(v->string, "ERR unknown") == 0, "error content correct");
    resp_value_free(v);

    v = parse_single(":42\r\n");
    ASSERT(v != NULL, "integer parsed");
    ASSERT(v->type == RESP_INTEGER, "type is INTEGER");
    ASSERT(v->integer == 42, "value is 42");
    resp_value_free(v);

    v = parse_single(":-100\r\n");
    ASSERT(v != NULL, "negative integer parsed");
    ASSERT(v->integer == -100, "value is -100");
    resp_value_free(v);

    v = parse_single("$5\r\nhello\r\n");
    ASSERT(v != NULL, "bulk string parsed");
    ASSERT(v->type == RESP_BULK_STRING, "type is BULK_STRING");
    ASSERT(v->bulk.len == 5, "bulk length is 5");
    ASSERT(memcmp(v->bulk.ptr, "hello", 5) == 0, "bulk content correct");
    resp_value_free(v);

    v = parse_single("$-1\r\n");
    ASSERT(v != NULL, "null bulk parsed");
    ASSERT(v->type == RESP_NULL, "type is NULL");
    resp_value_free(v);

    v = parse_single("*3\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$3\r\nbaz\r\n");
    ASSERT(v != NULL, "array parsed");
    ASSERT(v->type == RESP_ARRAY, "type is ARRAY");
    ASSERT(v->array.count == 3, "array has 3 items");
    ASSERT(v->array.items[0]->type == RESP_BULK_STRING, "item0 is bulk");
    ASSERT(memcmp(v->array.items[0]->bulk.ptr, "foo", 3) == 0, "item0 is foo");
    ASSERT(memcmp(v->array.items[1]->bulk.ptr, "bar", 3) == 0, "item1 is bar");
    ASSERT(memcmp(v->array.items[2]->bulk.ptr, "baz", 3) == 0, "item2 is baz");
    resp_value_free(v);

    v = parse_single("*-1\r\n");
    ASSERT(v != NULL, "null array parsed");
    ASSERT(v->type == RESP_NULL, "type is NULL");
    resp_value_free(v);

    v = parse_single("#t\r\n");
    ASSERT(v != NULL, "boolean true parsed");
    ASSERT(v->type == RESP_BOOLEAN, "type is BOOLEAN");
    ASSERT(v->integer == 1, "value is true");
    resp_value_free(v);

    v = parse_single("#f\r\n");
    ASSERT(v != NULL, "boolean false parsed");
    ASSERT(v->integer == 0, "value is false");
    resp_value_free(v);

    v = parse_single(",3.14\r\n");
    ASSERT(v != NULL, "double parsed");
    ASSERT(v->type == RESP_DOUBLE, "type is DOUBLE");
    ASSERT(v->real > 3.13 && v->real < 3.15, "value is ~3.14");
    resp_value_free(v);

    resp_parser *p = resp_parser_new();
    ASSERT(p != NULL, "parser created");
    ASSERT(resp_parser_feed(p, "*2\r\n$3\r\nGE", 9) == 0, "partial feed 1");
    ASSERT(resp_parser_parse(p) == NULL, "partial parse returns NULL");
    ASSERT(resp_parser_feed(p, "T\r\n$3\r\nkey\r\n", 12) == 0, "partial feed 2");
    v = resp_parser_parse(p);
    ASSERT(v != NULL, "complete parse after partial feeds");
    ASSERT(v->type == RESP_ARRAY, "type is ARRAY");
    ASSERT(v->array.count == 2, "has 2 items");
    ASSERT(memcmp(v->array.items[0]->bulk.ptr, "GET", 3) == 0, "cmd GET");
    ASSERT(memcmp(v->array.items[1]->bulk.ptr, "key", 3) == 0, "arg key");
    resp_value_free(v);
    resp_parser_free(p);

    char *str = resp_value_to_string((resp_value *)&(resp_value){
        .type = RESP_INTEGER,
        .integer = 123
    });
    ASSERT(str != NULL, "to_string for integer");
    ASSERT(strcmp(str, "123") == 0, "integer to_string correct");
    resp_value_free((resp_value *)&(resp_value){.type = RESP_NULL});

    resp_value *temp = resp_parser_new() ? NULL : NULL;
    (void)temp;

    kave_alloc_stats stats = kave_get_global_stats();
    ASSERT(stats.active_bytes == 0, "no leaked bytes");
    ASSERT(stats.active_blocks == 0, "no leaked blocks");

    if (failures == 0) {
        printf("test_resp: OK\n");
        return 0;
    }
    fprintf(stderr, "test_resp: %d failures\n", failures);
    return 1;
}
