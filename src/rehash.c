#include "kave/rehash.h"
#include "kave/hash_table.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>

#define REHASH_STEP 16

rehash_ctx *rehash_ctx_new(ht *old_table, size_t new_size)
{
    if (!old_table || new_size <= old_table->size) return NULL;
    rehash_ctx *ctx = kave_malloc(sizeof(rehash_ctx));
    if (!ctx) return NULL;
    ctx->old_table = old_table;
    ctx->new_table = ht_new(new_size);
    if (!ctx->new_table) {
        kave_free(ctx);
        return NULL;
    }
    ctx->rehash_index = 0;
    ctx->in_progress = 1;
    return ctx;
}

void rehash_ctx_free(rehash_ctx *ctx)
{
    if (!ctx) return;
    if (ctx->new_table) {
        ht_free(ctx->new_table);
    }
    kave_free(ctx);
}

static void rehash_move_entry(rehash_ctx *ctx, size_t idx)
{
    ht_entry *e = &ctx->old_table->entries[idx];
    if (!e->key || e->deleted) return;
    ht_insert(ctx->new_table, e->key, e->key_len, e->value);
    e->deleted = 1;
    kave_free(e->key);
    e->key = NULL;
    e->value = NULL;
    ctx->old_table->used--;
}

int rehash_step(rehash_ctx *ctx, size_t steps)
{
    if (!ctx || !ctx->in_progress) return 0;
    size_t moved = 0;
    while (moved < steps && ctx->rehash_index < ctx->old_table->size) {
        rehash_move_entry(ctx, ctx->rehash_index);
        ctx->rehash_index++;
        moved++;
    }
    if (ctx->rehash_index >= ctx->old_table->size) {
        ctx->in_progress = 0;
        ht_free(ctx->old_table);
        ctx->old_table = ctx->new_table;
        ctx->new_table = NULL;
        return 1;
    }
    return 0;
}

int rehash_complete(const rehash_ctx *ctx)
{
    return ctx ? !ctx->in_progress : 1;
}

void *rehash_find(const rehash_ctx *ctx, const char *key, size_t key_len)
{
    if (!ctx || !key) return NULL;
    void *val = ht_find(ctx->new_table, key, key_len);
    if (val) return val;
    return ht_find(ctx->old_table, key, key_len);
}

int rehash_insert(rehash_ctx *ctx, const char *key, size_t key_len, void *value)
{
    if (!ctx || !key) return -1;
    int r = ht_insert(ctx->new_table, key, key_len, value);
    if (r == 0) {
        ht_delete(ctx->old_table, key, key_len);
    }
    return r;
}

int rehash_delete(rehash_ctx *ctx, const char *key, size_t key_len)
{
    if (!ctx || !key) return -1;
    int r = ht_delete(ctx->new_table, key, key_len);
    if (r != 0) {
        r = ht_delete(ctx->old_table, key, key_len);
    }
    return r;
}
