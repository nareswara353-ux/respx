#include "kave/allocator.h"
#include <stdlib.h>
#include <pthread.h>

static pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER;
static kave_alloc_stats stats = {0, 0, 0, 0};

void *kave_malloc(size_t size)
{
    if (size == 0) return NULL;
    void *ptr = malloc(size);
    if (!ptr) return NULL;
    pthread_mutex_lock(&alloc_mutex);
    stats.total_allocated += size;
    stats.active_bytes += size;
    stats.active_blocks++;
    pthread_mutex_unlock(&alloc_mutex);
    return ptr;
}

void *kave_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    if (total == 0) return NULL;
    void *ptr = calloc(nmemb, size);
    if (!ptr) return NULL;
    pthread_mutex_lock(&alloc_mutex);
    stats.total_allocated += total;
    stats.active_bytes += total;
    stats.active_blocks++;
    pthread_mutex_unlock(&alloc_mutex);
    return ptr;
}

void *kave_realloc(void *ptr, size_t new_size)
{
    if (!ptr) return kave_malloc(new_size);
    if (new_size == 0) {
        kave_free(ptr);
        return NULL;
    }
    size_t old_size = malloc_usable_size(ptr);
    void *new_ptr = realloc(ptr, new_size);
    if (!new_ptr) return NULL;
    pthread_mutex_lock(&alloc_mutex);
    stats.active_bytes -= old_size;
    stats.active_bytes += new_size;
    pthread_mutex_unlock(&alloc_mutex);
    return new_ptr;
}

void kave_free(void *ptr)
{
    if (!ptr) return;
    size_t size = malloc_usable_size(ptr);
    pthread_mutex_lock(&alloc_mutex);
    stats.total_freed += size;
    stats.active_bytes -= size;
    stats.active_blocks--;
    pthread_mutex_unlock(&alloc_mutex);
    free(ptr);
}

kave_alloc_stats kave_get_global_stats(void)
{
    pthread_mutex_lock(&alloc_mutex);
    kave_alloc_stats copy = stats;
    pthread_mutex_unlock(&alloc_mutex);
    return copy;
}

void kave_reset_stats(void)
{
    pthread_mutex_lock(&alloc_mutex);
    stats.total_allocated = 0;
    stats.total_freed = 0;
    stats.active_bytes = 0;
    stats.active_blocks = 0;
    pthread_mutex_unlock(&alloc_mutex);
}
