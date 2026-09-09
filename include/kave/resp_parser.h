#ifndef KAVE_RESP_PARSER_H
#define KAVE_RESP_PARSER_H

#include <stddef.h>
#include <stdint.h>

typedef enum resp_type {
    RESP_STRING,
    RESP_ERROR,
    RESP_INTEGER,
    RESP_BULK_STRING,
    RESP_ARRAY,
    RESP_NULL,
    RESP_BOOLEAN,
    RESP_DOUBLE,
    RESP_MAP,
    RESP_SET,
    RESP_PUSH
} resp_type;

typedef struct resp_value {
    resp_type type;
    union {
        char *string;
        long long integer;
        double real;
        struct {
            char *ptr;
            size_t len;
        } bulk;
        struct {
            struct resp_value **items;
            size_t count;
        } array;
        struct {
            struct resp_value **keys;
            struct resp_value **values;
            size_t count;
        } map;
    };
} resp_value;

typedef struct resp_parser {
    char *buffer;
    size_t buf_len;
    size_t buf_cap;
    size_t pos;
    int state;
} resp_parser;

resp_parser *resp_parser_new(void);
void resp_parser_free(resp_parser *p);
int resp_parser_feed(resp_parser *p, const char *data, size_t len);
resp_value *resp_parser_parse(resp_parser *p);
void resp_value_free(resp_value *v);
char *resp_value_to_string(const resp_value *v);

#endif
