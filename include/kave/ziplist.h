#ifndef KAVE_ZIPLIST_H
#define KAVE_ZIPLIST_H

#include <stddef.h>
#include <stdint.h>

typedef unsigned char zl_byte;

typedef struct ziplist {
    zl_byte *data;
    size_t length;
    size_t capacity;
    size_t num_entries;
} ziplist;

ziplist *zl_new(void);
void zl_free(ziplist *zl);
size_t zl_length(const ziplist *zl);
size_t zl_entries(const ziplist *zl);
int zl_push_tail(ziplist *zl, const char *value, size_t value_len);
int zl_push_head(ziplist *zl, const char *value, size_t value_len);
int zl_get(const ziplist *zl, size_t index, char **value, size_t *value_len);
int zl_set(ziplist *zl, size_t index, const char *value, size_t value_len);
int zl_delete(ziplist *zl, size_t index);
int zl_insert(ziplist *zl, size_t index, const char *value, size_t value_len);
void zl_foreach(const ziplist *zl, void (*callback)(size_t index, const char *value, size_t value_len, void *user), void *user);
ziplist *zl_dup(const ziplist *zl);

#endif
