#ifndef KAVE_HASH_TABLE_H
#define KAVE_HASH_TABLE_H

#include <stddef.h>
#include <stdint.h>

typedef struct ht_entry {
    char *key;
    void *value;
    size_t key_len;
    uint64_t hash;
    int deleted;
} ht_entry;

typedef struct ht {
    ht_entry *entries;
    size_t size;
    size_t used;
    size_t mask;
    uint64_t (*hash_fn)(const char *key, size_t len);
    int (*key_eq)(const char *a, size_t alen, const char *b, size_t blen);
} ht;

ht *ht_new(size_t initial_size);
void ht_free(ht *table);
int ht_insert(ht *table, const char *key, size_t key_len, void *value);
void *ht_find(const ht *table, const char *key, size_t key_len);
int ht_delete(ht *table, const char *key, size_t key_len);
void ht_rehash(ht *table, size_t new_size);
size_t ht_count(const ht *table);
void ht_foreach(const ht *table, void (*callback)(const char *key, size_t key_len, void *value, void *userdata), void *userdata);

#endif
