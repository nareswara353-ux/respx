#include "kave/hash_table.h"
#include "kave/allocator.h"
#include <string.h>
#include <stdlib.h>

#define HT_INITIAL_SIZE 16
#define HT_LOAD_FACTOR 0.75

static uint64_t default_hash(const char *key, size_t len)
{
    uint64_t h = 0x811c9dc5;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)key[i];
        h *= 0x01000193;
    }
    return h;
}

static int default_key_eq(const char *a, size_t alen, const char *b, size_t blen)
{
    return alen == blen && memcmp(a, b, alen) == 0;
}

ht *ht_new(size_t initial_size)
{
    if (initial_size < 4) initial_size = 4;
    size_t size = 1;
    while (size < initial_size) size <<= 1;
    ht *table = kave_malloc(sizeof(ht));
    if (!table) return NULL;
    table->entries = kave_calloc(size, sizeof(ht_entry));
    if (!table->entries) {
        kave_free(table);
        return NULL;
    }
    table->size = size;
    table->used = 0;
    table->mask = size - 1;
    table->hash_fn = default_hash;
    table->key_eq = default_key_eq;
    return table;
}

void ht_free(ht *table)
{
    if (!table) return;
    for (size_t i = 0; i < table->size; i++) {
        ht_entry *e = &table->entries[i];
        if (e->key) {
            kave_free(e->key);
            if (e->value) kave_free(e->value);
        }
    }
    kave_free(table->entries);
    kave_free(table);
}

static size_t ht_find_slot(const ht *table, const char *key, size_t key_len, uint64_t hash, int for_insert)
{
    size_t idx = hash & table->mask;
    size_t first_deleted = table->size;
    for (size_t i = 0; i < table->size; i++) {
        size_t pos = (idx + i) & table->mask;
        ht_entry *e = &table->entries[pos];
        if (!e->key) {
            if (for_insert) {
                return (first_deleted < table->size) ? first_deleted : pos;
            }
            return table->size;
        }
        if (e->deleted) {
            if (first_deleted == table->size) first_deleted = pos;
            continue;
        }
        if (e->hash == hash && table->key_eq(e->key, e->key_len, key, key_len)) {
            return pos;
        }
    }
    return first_deleted;
}

int ht_insert(ht *table, const char *key, size_t key_len, void *value)
{
    if (!table || !key) return -1;
    if ((double)(table->used + 1) / table->size > HT_LOAD_FACTOR) {
        ht_rehash(table, table->size * 2);
    }
    uint64_t hash = table->hash_fn(key, key_len);
    size_t slot = ht_find_slot(table, key, key_len, hash, 1);
    if (slot >= table->size) return -1;
    ht_entry *e = &table->entries[slot];
    if (e->key) {
        if (e->value) kave_free(e->value);
        e->value = value;
        return 0;
    }
    e->key = kave_malloc(key_len + 1);
    if (!e->key) return -1;
    memcpy(e->key, key, key_len);
    e->key[key_len] = '\0';
    e->key_len = key_len;
    e->hash = hash;
    e->value = value;
    e->deleted = 0;
    table->used++;
    return 0;
}

void *ht_find(const ht *table, const char *key, size_t key_len)
{
    if (!table || !key) return NULL;
    uint64_t hash = table->hash_fn(key, key_len);
    size_t slot = ht_find_slot(table, key, key_len, hash, 0);
    if (slot >= table->size) return NULL;
    return table->entries[slot].value;
}

int ht_delete(ht *table, const char *key, size_t key_len)
{
    if (!table || !key) return -1;
    uint64_t hash = table->hash_fn(key, key_len);
    size_t slot = ht_find_slot(table, key, key_len, hash, 0);
    if (slot >= table->size) return -1;
    ht_entry *e = &table->entries[slot];
    e->deleted = 1;
    kave_free(e->key);
    e->key = NULL;
    if (e->value) {
        kave_free(e->value);
        e->value = NULL;
    }
    table->used--;
    return 0;
}

void ht_rehash(ht *table, size_t new_size)
{
    if (!table || new_size < table->used * 2) return;
    size_t old_size = table->size;
    ht_entry *old_entries = table->entries;
    table->size = new_size;
    table->mask = new_size - 1;
    table->used = 0;
    table->entries = kave_calloc(new_size, sizeof(ht_entry));
    if (!table->entries) {
        table->entries = old_entries;
        table->size = old_size;
        table->mask = old_size - 1;
        return;
    }
    for (size_t i = 0; i < old_size; i++) {
        ht_entry *e = &old_entries[i];
        if (e->key && !e->deleted) {
            uint64_t hash = e->hash;
            size_t slot = ht_find_slot(table, e->key, e->key_len, hash, 1);
            if (slot < table->size) {
                table->entries[slot] = *e;
                table->used++;
            } else {
                kave_free(e->key);
                if (e->value) kave_free(e->value);
            }
        }
    }
    kave_free(old_entries);
}

size_t ht_count(const ht *table)
{
    return table ? table->used : 0;
}

void ht_foreach(const ht *table, void (*callback)(const char *key, size_t key_len, void *value, void *userdata), void *userdata)
{
    if (!table || !callback) return;
    for (size_t i = 0; i < table->size; i++) {
        ht_entry *e = &table->entries[i];
        if (e->key && !e->deleted) {
            callback(e->key, e->key_len, e->value, userdata);
        }
    }
}
