#ifndef KAVE_STREAM_H
#define KAVE_STREAM_H

#include <stddef.h>
#include <stdint.h>

typedef struct stream_id {
    uint64_t ms;
    uint64_t seq;
} stream_id;

typedef struct stream_entry {
    stream_id id;
    char **fields;
    char **values;
    size_t num_fields;
    struct stream_entry *next;
} stream_entry;

typedef struct stream {
    stream_entry *head;
    stream_entry *tail;
    size_t length;
    uint64_t last_ms;
    uint64_t last_seq;
} stream;

stream *stream_new(void);
void stream_free(stream *s);
int stream_id_parse(const char *str, stream_id *out);
int stream_id_compare(const stream_id *a, const stream_id *b);
int stream_add(stream *s, const stream_id *id, char **fields, char **values, size_t num_fields);
stream_entry *stream_range(const stream *s, const stream_id *start, const stream_id *end, size_t count);
size_t stream_length(const stream *s);

#endif
