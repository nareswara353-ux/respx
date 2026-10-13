#include "kave/stream.h"
#include "kave/allocator.h"
#include "kave/sds.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

static void stream_entry_free(stream_entry *e)
{
    if (!e) return;
    for (size_t i = 0; i < e->num_fields; i++) {
        if (e->fields[i]) kave_free(e->fields[i]);
        if (e->values[i]) kave_free(e->values[i]);
    }
    if (e->fields) kave_free(e->fields);
    if (e->values) kave_free(e->values);
    kave_free(e);
}

stream *stream_new(void)
{
    stream *s = kave_malloc(sizeof(stream));
    if (!s) return NULL;
    s->head = NULL;
    s->tail = NULL;
    s->length = 0;
    s->last_ms = 0;
    s->last_seq = 0;
    return s;
}

void stream_free(stream *s)
{
    if (!s) return;
    stream_entry *e = s->head;
    while (e) {
        stream_entry *next = e->next;
        stream_entry_free(e);
        e = next;
    }
    kave_free(s);
}

int stream_id_parse(const char *str, stream_id *out)
{
    if (!str || !out) return -1;
    const char *dash = strchr(str, '-');
    if (!dash) return -1;
    char ms_buf[32];
    size_t ms_len = (size_t)(dash - str);
    if (ms_len == 0 || ms_len >= sizeof(ms_buf)) return -1;
    memcpy(ms_buf, str, ms_len);
    ms_buf[ms_len] = '\0';
    for (size_t i = 0; i < ms_len; i++) if (!isdigit((unsigned char)ms_buf[i])) return -1;
    for (const char *p = dash + 1; *p; p++) if (!isdigit((unsigned char)*p)) return -1;
    out->ms = (uint64_t)strtoull(ms_buf, NULL, 10);
    out->seq = (uint64_t)strtoull(dash + 1, NULL, 10);
    return 0;
}

int stream_id_compare(const stream_id *a, const stream_id *b)
{
    if (a->ms < b->ms) return -1;
    if (a->ms > b->ms) return 1;
    if (a->seq < b->seq) return -1;
    if (a->seq > b->seq) return 1;
    return 0;
}

static void stream_generate_id(stream *s, stream_id *out)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t now_ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
    if (now_ms > s->last_ms) {
        out->ms = now_ms;
        out->seq = 0;
    } else {
        out->ms = s->last_ms;
        out->seq = s->last_seq + 1;
    }
    s->last_ms = out->ms;
    s->last_seq = out->seq;
}

int stream_add(stream *s, const stream_id *id, char **fields, char **values, size_t num_fields)
{
    if (!s || !fields || !values) return -1;
    stream_id generated;
    if (!id) {
        stream_generate_id(s, &generated);
    } else {
        generated = *id;
    }
    stream_entry *e = kave_malloc(sizeof(stream_entry));
    if (!e) return -1;
    e->id = generated;
    e->num_fields = num_fields;
    e->fields = kave_malloc(num_fields * sizeof(char *));
    e->values = kave_malloc(num_fields * sizeof(char *));
    if (!e->fields || !e->values) {
        if (e->fields) kave_free(e->fields);
        if (e->values) kave_free(e->values);
        kave_free(e);
        return -1;
    }
    for (size_t i = 0; i < num_fields; i++) {
        e->fields[i] = sds_new(fields[i]);
        e->values[i] = sds_new(values[i]);
    }
    e->next = NULL;
    if (s->tail) {
        s->tail->next = e;
        s->tail = e;
    } else {
        s->head = s->tail = e;
    }
    s->length++;
    if (generated.ms > s->last_ms || (generated.ms == s->last_ms && generated.seq > s->last_seq)) {
        s->last_ms = generated.ms;
        s->last_seq = generated.seq;
    }
    return 0;
}

stream_entry *stream_range(const stream *s, const stream_id *start, const stream_id *end, size_t count)
{
    if (!s || !s->head) return NULL;
    stream_entry *first = NULL;
    stream_entry *e = s->head;
    while (e) {
        if (stream_id_compare(&e->id, start) >= 0) {
            first = e;
            break;
        }
        e = e->next;
    }
    if (!first) return NULL;
    if (count == 0) return first;
    stream_entry *cur = first;
    size_t n = 1;
    while (cur->next && n < count) {
        if (stream_id_compare(&cur->next->id, end) > 0) break;
        cur = cur->next;
        n++;
    }
    return first;
}

size_t stream_length(const stream *s)
{
    return s ? s->length : 0;
}
