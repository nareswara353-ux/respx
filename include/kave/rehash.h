#ifndef KAVE_REHASH_H
#define KAVE_REHASH_H

#include "kave/hash_table.h"

typedef struct rehash_ctx {
    ht *old_table;
    ht *new_table;
    size_t rehash_index;
    int in_progress;
} rehash_ctx;

void rehash_init(rehash_ctx *ctx, ht *old_ht, size_t new_size);
int rehash_step(rehash_ctx *ctx, int steps);
int rehash_complete(const rehash_ctx *ctx);
void rehash_cleanup(rehash_ctx *ctx);
ht *rehash_get_current_table(const rehash_ctx *ctx);
ht *rehash_get_old_table(const rehash_ctx *ctx);

#endif
