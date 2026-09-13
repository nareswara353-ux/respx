#include "kave/hash_table.h"
#include "kave/skiplist.h"
#include "kave/sds.h"
#include "kave/allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_ITERATIONS 1000000

static double elapsed_seconds(struct timespec *start, struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1e9;
}

static void benchmark_ht_set(int iterations)
{
    ht *table = ht_new(65536);
    struct timespec start, end;
    char key[32];
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        char *val = sds_new(key);
        ht_insert(table, key, strlen(key), val, HT_VAL_SDS);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double secs = elapsed_seconds(&start, &end);
    printf("HT SET   : %8d ops in %.3f s = %.0f ops/sec\n",
           iterations, secs, iterations / secs);
    ht_free(table);
}

static void benchmark_ht_get(int iterations)
{
    ht *table = ht_new(65536);
    char key[32];
    for (int i = 0; i < iterations; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        char *val = sds_new(key);
        ht_insert(table, key, strlen(key), val, HT_VAL_SDS);
    }
    struct timespec start, end;
    volatile size_t hits = 0;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        void *v = ht_find(table, key, strlen(key));
        if (v) hits++;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double secs = elapsed_seconds(&start, &end);
    printf("HT GET   : %8d ops in %.3f s = %.0f ops/sec (hits=%zu)\n",
           iterations, secs, iterations / secs, hits);
    ht_free(table);
}

static void benchmark_ht_del(int iterations)
{
    ht *table = ht_new(65536);
    char key[32];
    for (int i = 0; i < iterations; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        char *val = sds_new(key);
        ht_insert(table, key, strlen(key), val, HT_VAL_SDS);
    }
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        ht_delete(table, key, strlen(key));
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double secs = elapsed_seconds(&start, &end);
    printf("HT DEL   : %8d ops in %.3f s = %.0f ops/sec\n",
           iterations, secs, iterations / secs);
    ht_free(table);
}

static void benchmark_sl_insert(int iterations)
{
    skiplist *sl = sl_new();
    struct timespec start, end;
    char member[32];
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        snprintf(member, sizeof(member), "member_%d", i);
        char *val = sds_new(member);
        sl_insert(sl, (double)i, member, val);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double secs = elapsed_seconds(&start, &end);
    printf("SL INSERT: %8d ops in %.3f s = %.0f ops/sec\n",
           iterations, secs, iterations / secs);
    sl_free(sl);
}

static void benchmark_sl_find(int iterations)
{
    skiplist *sl = sl_new();
    char member[32];
    for (int i = 0; i < iterations; i++) {
        snprintf(member, sizeof(member), "member_%d", i);
        char *val = sds_new(member);
        sl_insert(sl, (double)i, member, val);
    }
    struct timespec start, end;
    volatile size_t hits = 0;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        snprintf(member, sizeof(member), "member_%d", i);
        void *v = sl_find(sl, (double)i, member);
        if (v) hits++;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double secs = elapsed_seconds(&start, &end);
    printf("SL FIND  : %8d ops in %.3f s = %.0f ops/sec (hits=%zu)\n",
           iterations, secs, iterations / secs, hits);
    sl_free(sl);
}

static void benchmark_sl_traverse(int iterations)
{
    skiplist *sl = sl_new();
    char member[32];
    for (int i = 0; i < iterations; i++) {
        snprintf(member, sizeof(member), "member_%d", i);
        char *val = sds_new(member);
        sl_insert(sl, (double)i, member, val);
    }
    struct timespec start, end;
    volatile size_t count = 0;
    clock_gettime(CLOCK_MONOTONIC, &start);
    skiplist_node *node = sl_first(sl);
    while (node) {
        count++;
        node = sl_next(node);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double secs = elapsed_seconds(&start, &end);
    printf("SL TRAV  : %8zu ops in %.3f s = %.0f ops/sec\n",
           count, secs, count / secs);
    sl_free(sl);
}

int main(int argc, char **argv)
{
    int iterations = DEFAULT_ITERATIONS;
    if (argc > 1) {
        iterations = atoi(argv[1]);
        if (iterations <= 0) iterations = DEFAULT_ITERATIONS;
    }
    printf("kave-server benchmark (iterations=%d)\n", iterations);
    printf("========================================\n");
    benchmark_ht_set(iterations);
    benchmark_ht_get(iterations);
    benchmark_ht_del(iterations);
    benchmark_sl_insert(iterations);
    benchmark_sl_find(iterations);
    benchmark_sl_traverse(iterations);
    printf("========================================\n");
    kave_alloc_stats stats = kave_get_global_stats();
    printf("Memory: %zu bytes in %zu blocks active at exit\n",
           stats.active_bytes, stats.active_blocks);
    return 0;
}
