#ifndef KAVE_DICT_H
#define KAVE_DICT_H

#include <stddef.h>
#include <stdint.h>

#define DICT_OK 0
#define DICT_ERR 1

typedef struct dict_entry {
    void *key;
    void *value;
    struct dict_entry *next;
    uint64_t hash;
} dict_entry;

typedef struct dict_type {
    uint64_t (*hash_function)(const void *key);
    int (*key_compare)(const void *a, const void *b);
    void (*key_destructor)(void *key);
    void (*value_destructor)(void *value);
} dict_type;

typedef struct dict {
    dict_entry **buckets;
    size_t num_buckets;
    size_t num_entries;
    dict_type type;
    size_t iter_index;
    dict_entry *iter_next;
} dict;

typedef struct dict_iterator {
    dict *target;
    size_t bucket_index;
    dict_entry *next_entry;
} dict_iterator;

dict *dict_new(const dict_type *type);
void dict_free(dict *d);
int dict_insert(dict *d, void *key, void *value);
void *dict_find(const dict *d, const void *key);
int dict_delete(dict *d, const void *key);
size_t dict_size(const dict *d);
void dict_clear(dict *d);
void dict_resize(dict *d, size_t new_size);
dict_iterator *dict_iterator_new(dict *d);
dict_entry *dict_iterator_next(dict_iterator *it);
void dict_iterator_free(dict_iterator *it);
void dict_scan(dict *d, void (*callback)(void *key, void *value, void *user), void *user);

#endif
