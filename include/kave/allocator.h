#ifndef KAVE_ALLOCATOR_H
#define KAVE_ALLOCATOR_H

#include <stddef.h>

typedef struct kave_alloc_stats {
    size_t total_allocated;
    size_t total_freed;
    size_t active_bytes;
    size_t active_blocks;
} kave_alloc_stats;

void *kave_malloc(size_t size);
void *kave_calloc(size_t nmemb, size_t size);
void *kave_realloc(void *ptr, size_t new_size);
void kave_free(void *ptr);

kave_alloc_stats kave_get_global_stats(void);
void kave_reset_stats(void);

#endif
