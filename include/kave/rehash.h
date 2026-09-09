#ifndef KAVE_REHASH_H
#define KAVE_REHASH_H

#include <stddef.h>

typedef struct ht ht;

typedef struct rehash_state {
    ht *source;
    ht *target;
    size_t rehash_idx;
    int in_progress;
} rehash_state;

void rehash_init(rehash_state *state, ht *old_table, ht *new_table);
int rehash_step(rehash_state *state, int steps);
void rehash_finish(rehash_state *state);
int rehash_is_done(const rehash_state *state);

#endif
