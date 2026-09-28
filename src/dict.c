#include "kave/dict.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>

#define DICT_INITIAL_SIZE 16
#define DICT_LOAD_FACTOR 1.0
#define DICT_GROW_MULT 2

static size_t dict_next_power_of_two(size_t n)
{
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

dict *dict_new(const dict_type *type)
{
    if (!type || !type->hash_function || !type->key_compare) return NULL;
    dict *d = kave_malloc(sizeof(dict));
    if (!d) return NULL;
    d->num_buckets = DICT_INITIAL_SIZE;
    d->buckets = kave_calloc(d->num_buckets, sizeof(dict_entry *));
    if (!d->buckets) {
        kave_free(d);
        return NULL;
    }
    d->num_entries = 0;
    d->type = *type;
    d->iter_index = 0;
    d->iter_next = NULL;
    return d;
}

void dict_clear(dict *d)
{
    if (!d) return;
    for (size_t i = 0; i < d->num_buckets; i++) {
        dict_entry *e = d->buckets[i];
        while (e) {
            dict_entry *next = e->next;
            if (d->type.key_destructor) d->type.key_destructor(e->key);
            if (d->type.value_destructor) d->type.value_destructor(e->value);
            kave_free(e);
            e = next;
        }
        d->buckets[i] = NULL;
    }
    d->num_entries = 0;
}

void dict_free(dict *d)
{
    if (!d) return;
    dict_clear(d);
    kave_free(d->buckets);
    kave_free(d);
}

static int dict_expand_if_needed(dict *d)
{
    if ((double)d->num_entries / (double)d->num_buckets <= DICT_LOAD_FACTOR) return DICT_OK;
    size_t new_size = d->num_buckets * DICT_GROW_MULT;
    dict_entry **new_buckets = kave_calloc(new_size, sizeof(dict_entry *));
    if (!new_buckets) return DICT_ERR;
    for (size_t i = 0; i < d->num_buckets; i++) {
        dict_entry *e = d->buckets[i];
        while (e) {
            dict_entry *next = e->next;
            size_t idx = e->hash & (new_size - 1);
            e->next = new_buckets[idx];
            new_buckets[idx] = e;
            e = next;
        }
    }
    kave_free(d->buckets);
    d->buckets = new_buckets;
    d->num_buckets = new_size;
    return DICT_OK;
}

static dict_entry *dict_find_entry(const dict *d, const void *key, uint64_t hash)
{
    size_t idx = hash & (d->num_buckets - 1);
    dict_entry *e = d->buckets[idx];
    while (e) {
        if (e->hash == hash && d->type.key_compare(e->key, key) == 0) return e;
        e = e->next;
    }
    return NULL;
}

int dict_insert(dict *d, void *key, void *value)
{
    if (!d || !key) return DICT_ERR;
    uint64_t hash = d->type.hash_function(key);
    dict_entry *existing = dict_find_entry(d, key, hash);
    if (existing) {
        if (d->type.value_destructor) d->type.value_destructor(existing->value);
        existing->value = value;
        return DICT_OK;
    }
    if (dict_expand_if_needed(d) != DICT_OK) return DICT_ERR;
    dict_entry *e = kave_malloc(sizeof(dict_entry));
    if (!e) return DICT_ERR;
    e->key = key;
    e->value = value;
    e->hash = hash;
    size_t idx = hash & (d->num_buckets - 1);
    e->next = d->buckets[idx];
    d->buckets[idx] = e;
    d->num_entries++;
    return DICT_OK;
}

void *dict_find(const dict *d, const void *key)
{
    if (!d || !key) return NULL;
    uint64_t hash = d->type.hash_function(key);
    dict_entry *e = dict_find_entry(d, key, hash);
    return e ? e->value : NULL;
}

int dict_delete(dict *d, const void *key)
{
    if (!d || !key) return DICT_ERR;
    uint64_t hash = d->type.hash_function(key);
    size_t idx = hash & (d->num_buckets - 1);
    dict_entry *e = d->buckets[idx];
    dict_entry *prev = NULL;
    while (e) {
        if (e->hash == hash && d->type.key_compare(e->key, key) == 0) {
            if (prev) prev->next = e->next;
            else d->buckets[idx] = e->next;
            if (d->type.key_destructor) d->type.key_destructor(e->key);
            if (d->type.value_destructor) d->type.value_destructor(e->value);
            kave_free(e);
            d->num_entries--;
            return DICT_OK;
        }
        prev = e;
        e = e->next;
    }
    return DICT_ERR;
}

size_t dict_size(const dict *d)
{
    return d ? d->num_entries : 0;
}

void dict_resize(dict *d, size_t new_size)
{
    if (!d || new_size == 0) return;
    size_t target = dict_next_power_of_two(new_size);
    if (target == d->num_buckets) return;
    dict_entry **new_buckets = kave_calloc(target, sizeof(dict_entry *));
    if (!new_buckets) return;
    for (size_t i = 0; i < d->num_buckets; i++) {
        dict_entry *e = d->buckets[i];
        while (e) {
            dict_entry *next = e->next;
            size_t idx = e->hash & (target - 1);
            e->next = new_buckets[idx];
            new_buckets[idx] = e;
            e = next;
        }
    }
    kave_free(d->buckets);
    d->buckets = new_buckets;
    d->num_buckets = target;
}

dict_iterator *dict_iterator_new(dict *d)
{
    if (!d) return NULL;
    dict_iterator *it = kave_malloc(sizeof(dict_iterator));
    if (!it) return NULL;
    it->target = d;
    it->bucket_index = 0;
    it->next_entry = NULL;
    return it;
}

dict_entry *dict_iterator_next(dict_iterator *it)
{
    if (!it || !it->target) return NULL;
    dict *d = it->target;
    while (!it->next_entry) {
        if (it->bucket_index >= d->num_buckets) return NULL;
        it->next_entry = d->buckets[it->bucket_index++];
    }
    dict_entry *result = it->next_entry;
    it->next_entry = result->next;
    return result;
}

void dict_iterator_free(dict_iterator *it)
{
    if (!it) return;
    kave_free(it);
}

void dict_scan(dict *d, void (*callback)(void *key, void *value, void *user), void *user)
{
    if (!d || !callback) return;
    for (size_t i = 0; i < d->num_buckets; i++) {
        dict_entry *e = d->buckets[i];
        while (e) {
            callback(e->key, e->value, user);
            e = e->next;
        }
    }
}
