#include "kave/expire.h"
#include "kave/allocator.h"
#include "kave/list.h"
#include "kave/sds.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EXPIRE_BUCKETS 1024

typedef struct expire_entry {
    char *key;
    size_t key_len;
    time_t expire_at;
} expire_entry;

struct expire_ctx {
    list *buckets[EXPIRE_BUCKETS];
    time_t last_active_check;
};

static uint64_t expire_hash(const char *key, size_t len)
{
    uint64_t h = 0x811c9dc5;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)key[i];
        h *= 0x01000193;
    }
    return h;
}

static int expire_entry_cmp(const void *a, const void *b)
{
    const expire_entry *ea = (const expire_entry *)a;
    const expire_entry *eb = (const expire_entry *)b;
    if (ea->key_len != eb->key_len) return -1;
    return memcmp(ea->key, eb->key, ea->key_len);
}

static expire_entry *expire_entry_new(const char *key, size_t key_len, time_t ttl)
{
    expire_entry *e = kave_malloc(sizeof(expire_entry));
    if (!e) return NULL;
    e->key = kave_malloc(key_len + 1);
    if (!e->key) {
        kave_free(e);
        return NULL;
    }
    memcpy(e->key, key, key_len);
    e->key[key_len] = '\0';
    e->key_len = key_len;
    e->expire_at = time(NULL) + ttl;
    return e;
}

static void expire_entry_free(expire_entry *e)
{
    if (!e) return;
    if (e->key) kave_free(e->key);
    kave_free(e);
}

expire_ctx *expire_new(void)
{
    expire_ctx *ctx = kave_malloc(sizeof(expire_ctx));
    if (!ctx) return NULL;
    for (int i = 0; i < EXPIRE_BUCKETS; i++) {
        ctx->buckets[i] = list_new();
        if (!ctx->buckets[i]) {
            for (int j = 0; j < i; j++) {
                list_free(ctx->buckets[j]);
            }
            kave_free(ctx);
            return NULL;
        }
    }
    ctx->last_active_check = time(NULL);
    return ctx;
}

void expire_free(expire_ctx *ctx)
{
    if (!ctx) return;
    for (int i = 0; i < EXPIRE_BUCKETS; i++) {
        if (ctx->buckets[i]) {
            list_foreach(ctx->buckets[i], (void (*)(void *, void *))expire_entry_free, NULL);
            list_free(ctx->buckets[i]);
        }
    }
    kave_free(ctx);
}

static int expire_find_entry(expire_ctx *ctx, const char *key, size_t key_len, list_node **out_node, expire_entry **out_entry)
{
    uint64_t h = expire_hash(key, key_len);
    int bucket = h % EXPIRE_BUCKETS;
    list *l = ctx->buckets[bucket];
    list_node *node = list_head(l);
    while (node) {
        expire_entry *e = (expire_entry *)node->data;
        if (e->key_len == key_len && memcmp(e->key, key, key_len) == 0) {
            if (out_node) *out_node = node;
            if (out_entry) *out_entry = e;
            return 1;
        }
        node = node->next;
    }
    return 0;
}

int expire_set(expire_ctx *ctx, const char *key, size_t key_len, time_t ttl_seconds)
{
    if (!ctx || !key || ttl_seconds <= 0) return -1;
    expire_entry *existing = NULL;
    list_node *node = NULL;
    if (expire_find_entry(ctx, key, key_len, &node, &existing)) {
        existing->expire_at = time(NULL) + ttl_seconds;
        return 0;
    }
    expire_entry *e = expire_entry_new(key, key_len, ttl_seconds);
    if (!e) return -1;
    uint64_t h = expire_hash(key, key_len);
    int bucket = h % EXPIRE_BUCKETS;
    list_append(ctx->buckets[bucket], e);
    return 0;
}

int expire_remove(expire_ctx *ctx, const char *key, size_t key_len)
{
    if (!ctx || !key) return -1;
    list_node *node = NULL;
    expire_entry *e = NULL;
    if (!expire_find_entry(ctx, key, key_len, &node, &e)) return -1;
    uint64_t h = expire_hash(key, key_len);
    int bucket = h % EXPIRE_BUCKETS;
    list_delete_node(ctx->buckets[bucket], node);
    expire_entry_free(e);
    return 0;
}

time_t expire_get_ttl(const expire_ctx *ctx, const char *key, size_t key_len)
{
    if (!ctx || !key) return -1;
    expire_entry *e = NULL;
    if (!expire_find_entry((expire_ctx *)ctx, key, key_len, NULL, &e)) return -1;
    time_t now = time(NULL);
    if (e->expire_at <= now) return 0;
    return e->expire_at - now;
}

int expire_is_expired(const expire_ctx *ctx, const char *key, size_t key_len)
{
    if (!ctx || !key) return 1;
    expire_entry *e = NULL;
    if (!expire_find_entry((expire_ctx *)ctx, key, key_len, NULL, &e)) return 1;
    return (time(NULL) >= e->expire_at);
}

void expire_check_active(expire_ctx *ctx, void (*on_expire)(const char *key, size_t key_len, void *user), void *user)
{
    if (!ctx || !on_expire) return;
    time_t now = time(NULL);
    if (now - ctx->last_active_check < 1) return;
    ctx->last_active_check = now;
    for (int i = 0; i < EXPIRE_BUCKETS; i++) {
        list *l = ctx->buckets[i];
        list_node *node = list_head(l);
        while (node) {
            list_node *next = node->next;
            expire_entry *e = (expire_entry *)node->data;
            if (e->expire_at <= now) {
                on_expire(e->key, e->key_len, user);
                list_delete_node(l, node);
                expire_entry_free(e);
            }
            node = next;
        }
    }
}

void expire_passive_cleanup(expire_ctx *ctx, size_t max_checks)
{
    if (!ctx) return;
    time_t now = time(NULL);
    size_t checked = 0;
    for (int i = 0; i < EXPIRE_BUCKETS && checked < max_checks; i++) {
        list *l = ctx->buckets[i];
        list_node *node = list_head(l);
        while (node && checked < max_checks) {
            list_node *next = node->next;
            expire_entry *e = (expire_entry *)node->data;
            if (e->expire_at <= now) {
                list_delete_node(l, node);
                expire_entry_free(e);
            }
            checked++;
            node = next;
        }
    }
}
