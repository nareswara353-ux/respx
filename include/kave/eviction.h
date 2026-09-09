#ifndef KAVE_EVICTION_H
#define KAVE_EVICTION_H

#include <stddef.h>
#include <stdint.h>

typedef enum evict_policy {
    EVICT_NONE,
    EVICT_LRU,
    EVICT_LFU
} evict_policy;

typedef struct evict_ctx evict_ctx;

evict_ctx *evict_new(evict_policy policy, size_t max_memory_bytes);
void evict_free(evict_ctx *ctx);
void evict_set_policy(evict_ctx *ctx, evict_policy policy);
void evict_set_max_memory(evict_ctx *ctx, size_t max_bytes);
int evict_record_access(evict_ctx *ctx, const char *key, size_t key_len);
int evict_remove_key(evict_ctx *ctx, const char *key, size_t key_len);
const char *evict_select_victim(evict_ctx *ctx, size_t *key_len);
size_t evict_get_current_memory(const evict_ctx *ctx);
int evict_should_evict(const evict_ctx *ctx);
void evict_perform_eviction(evict_ctx *ctx, void (*on_evict)(const char *key, size_t key_len, void *user), void *user);

#endif
