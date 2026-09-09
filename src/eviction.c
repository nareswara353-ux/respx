#include "kave/eviction.h"
#include "kave/allocator.h"
#include "kave/list.h"
#include "kave/sds.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

typedef struct evict_entry {
    char *key;
    size_t key_len;
    size_t memory_usage;
    time_t last_access;
    unsigned int access_count;
} evict_entry;

struct evict_ctx {
    evict_policy policy;
    size_t max_memory_bytes;
    size_t current_memory_bytes;
    list *entries;
    unsigned int access_counter;
};

static uint64_t evict_hash(const char *key, size_t len)
{
    uint64_t h = 0x811c9dc5;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)key[i];
        h *= 0x01000193;
    }
    return h;
}

static evict_entry *evict_entry_new(const char *key, size_t key_len, size_t memory)
{
    evict_entry *e = kave_malloc(sizeof(evict_entry));
    if (!e) return NULL;
    e->key = kave_malloc(key_len + 1);
    if (!e->key) {
        kave_free(e);
        return NULL;
    }
    memcpy(e->key, key, key_len);
    e->key[key_len] = '\0';
    e->key_len = key_len;
    e->memory_usage = memory;
    e->last_access = time(NULL);
    e->access_count = 1;
    return e;
}

static void evict_entry_free(evict_entry *e)
{
    if (!e) return;
    if (e->key) kave_free(e->key);
    kave_free(e);
}

static int evict_entry_cmp(const void *a, const void *b)
{
    const evict_entry *ea = (const evict_entry *)a;
    const evict_entry *eb = (const evict_entry *)b;
    if (ea->key_len != eb->key_len) return -1;
    return memcmp(ea->key, eb->key, ea->key_len);
}

evict_ctx *evict_new(evict_policy policy, size_t max_memory_bytes)
{
    evict_ctx *ctx = kave_malloc(sizeof(evict_ctx));
    if (!ctx) return NULL;
    ctx->policy = policy;
    ctx->max_memory_bytes = max_memory_bytes;
    ctx->current_memory_bytes = 0;
    ctx->entries = list_new();
    if (!ctx->entries) {
        kave_free(ctx);
        return NULL;
    }
    ctx->access_counter = 0;
    return ctx;
}

void evict_free(evict_ctx *ctx)
{
    if (!ctx) return;
    list_foreach(ctx->entries, (void (*)(void *, void *))evict_entry_free, NULL);
    list_free(ctx->entries);
    kave_free(ctx);
}

void evict_set_policy(evict_ctx *ctx, evict_policy policy)
{
    if (!ctx) return;
    ctx->policy = policy;
}

void evict_set_max_memory(evict_ctx *ctx, size_t max_bytes)
{
    if (!ctx) return;
    ctx->max_memory_bytes = max_bytes;
}

static evict_entry *evict_find_entry(evict_ctx *ctx, const char *key, size_t key_len)
{
    list_node *node = list_head(ctx->entries);
    while (node) {
        evict_entry *e = (evict_entry *)node->data;
        if (e->key_len == key_len && memcmp(e->key, key, key_len) == 0) {
            return e;
        }
        node = node->next;
    }
    return NULL;
}

int evict_record_access(evict_ctx *ctx, const char *key, size_t key_len)
{
    if (!ctx || !key) return -1;
    evict_entry *e = evict_find_entry(ctx, key, key_len);
    if (!e) {
        e = evict_entry_new(key, key_len, 0);
        if (!e) return -1;
        list_append(ctx->entries, e);
        return 0;
    }
    e->last_access = time(NULL);
    e->access_count++;
    return 0;
}

int evict_remove_key(evict_ctx *ctx, const char *key, size_t key_len)
{
    if (!ctx || !key) return -1;
    list_node *node = list_head(ctx->entries);
    while (node) {
        evict_entry *e = (evict_entry *)node->data;
        if (e->key_len == key_len && memcmp(e->key, key, key_len) == 0) {
            ctx->current_memory_bytes -= e->memory_usage;
            list_delete_node(ctx->entries, node);
            evict_entry_free(e);
            return 0;
        }
        node = node->next;
    }
    return -1;
}

const char *evict_select_victim(evict_ctx *ctx, size_t *key_len)
{
    if (!ctx || list_length(ctx->entries) == 0) {
        if (key_len) *key_len = 0;
        return NULL;
    }
    list_node *node = list_head(ctx->entries);
    evict_entry *victim_entry = (evict_entry *)node->data;
    node = node->next;
    while (node) {
        evict_entry *e = (evict_entry *)node->data;
        if (ctx->policy == EVICT_LRU) {
            if (e->last_access < victim_entry->last_access) {
                victim_entry = e;
            }
        } else if (ctx->policy == EVICT_LFU) {
            if (e->access_count < victim_entry->access_count) {
                victim_entry = e;
            }
        }
        node = node->next;
    }
    if (key_len) *key_len = victim_entry->key_len;
    return victim_entry->key;
}

size_t evict_get_current_memory(const evict_ctx *ctx)
{
    return ctx ? ctx->current_memory_bytes : 0;
}

int evict_should_evict(const evict_ctx *ctx)
{
    if (!ctx) return 0;
    return ctx->current_memory_bytes > ctx->max_memory_bytes;
}

void evict_perform_eviction(evict_ctx *ctx, void (*on_evict)(const char *key, size_t key_len, void *user), void *user)
{
    if (!ctx || !on_evict) return;
    while (evict_should_evict(ctx) && list_length(ctx->entries) > 0) {
        size_t key_len;
        const char *key = evict_select_victim(ctx, &key_len);
        if (!key) break;
        on_evict(key, key_len, user);
        evict_remove_key(ctx, key, key_len);
    }
}

void evict_update_memory(evict_ctx *ctx, const char *key, size_t key_len, size_t memory_delta)
{
    if (!ctx) return;
    evict_entry *e = evict_find_entry(ctx, key, key_len);
    if (e) {
        e->memory_usage += memory_delta;
    } else {
        e = evict_entry_new(key, key_len, memory_delta);
        if (e) {
            list_append(ctx->entries, e);
        }
    }
    ctx->current_memory_bytes += memory_delta;
}
