#ifndef KAVE_REHASH_H
#define KAVE_REHASH_H

#include <stddef.h>

typedef struct ht ht;

typedef struct rehash_ctx {
    ht *old_table;
    ht *new_table;
    size_t rehash_index;
    int in_progress;
} rehash_ctx;

rehash_ctx *rehash_ctx_new(ht *old_table, size_t new_size);
void rehash_ctx_free(rehash_ctx *ctx);
int rehash_step(rehash_ctx *ctx, size_t steps);
int rehash_complete(const rehash_ctx *ctx);
void *rehash_find(const rehash_ctx *ctx, const char *key, size_t key_len);
int rehash_insert(rehash_ctx *ctx, const char *key, size_t key_len, void *value);
int rehash_delete(rehash_ctx *ctx, const char *key, size_t key_len);

#endif
