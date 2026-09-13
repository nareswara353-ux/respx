#include "kave/hash_table.h"
#include "kave/allocator.h"
#include "kave/sds.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        failures++; \
    } \
} while (0)

int main(void)
{
    ht *table = ht_new(16);
    ASSERT(table != NULL, "ht_new returns non-null");
    ASSERT(ht_count(table) == 0, "new table empty");

    char *v1 = sds_new("value1");
    ASSERT(ht_insert(table, "key1", 4, v1) == 0, "insert key1");
    ASSERT(ht_count(table) == 1, "count is 1 after insert");

    void *found = ht_find(table, "key1", 4);
    ASSERT(found == v1, "find returns inserted value");
    ASSERT(strcmp((char *)found, "value1") == 0, "value content correct");

    ASSERT(ht_find(table, "missing", 7) == NULL, "missing key returns NULL");

    char *v1b = sds_new("value1_updated");
    ASSERT(ht_insert(table, "key1", 4, v1b) == 0, "update existing key");
    ASSERT(ht_count(table) == 1, "count still 1 after update");
    found = ht_find(table, "key1", 4);
    ASSERT(strcmp((char *)found, "value1_updated") == 0, "value updated");

    ASSERT(ht_delete(table, "key1", 4) == 0, "delete existing key");
    ASSERT(ht_count(table) == 0, "count 0 after delete");
    ASSERT(ht_find(table, "key1", 4) == NULL, "find after delete returns NULL");
    ASSERT(ht_delete(table, "key1", 4) == -1, "delete missing returns -1");

    char keys[100][16];
    char *values[100];
    for (int i = 0; i < 100; i++) {
        snprintf(keys[i], sizeof(keys[i]), "key_%d", i);
        values[i] = sds_new(keys[i]);
        ASSERT(ht_insert(table, keys[i], strlen(keys[i]), values[i]) == 0,
               "bulk insert succeeds");
    }
    ASSERT(ht_count(table) == 100, "count is 100 after bulk insert");

    for (int i = 0; i < 100; i++) {
        void *v = ht_find(table, keys[i], strlen(keys[i]));
        ASSERT(v == values[i], "bulk find returns correct value");
    }

    for (int i = 0; i < 50; i++) {
        ASSERT(ht_delete(table, keys[i], strlen(keys[i])) == 0,
               "bulk delete succeeds");
    }
    ASSERT(ht_count(table) == 50, "count 50 after deleting half");

    for (int i = 0; i < 50; i++) {
        ASSERT(ht_find(table, keys[i], strlen(keys[i])) == NULL,
               "deleted key not found");
    }
    for (int i = 50; i < 100; i++) {
        ASSERT(ht_find(table, keys[i], strlen(keys[i])) != NULL,
               "surviving key still found");
    }

    ht_free(table);

    kave_alloc_stats stats = kave_get_global_stats();
    ASSERT(stats.active_bytes == 0, "no leaked bytes");
    ASSERT(stats.active_blocks == 0, "no leaked blocks");

    if (failures == 0) {
        printf("test_hash: OK\n");
        return 0;
    }
    fprintf(stderr, "test_hash: %d failures\n", failures);
    return 1;
}
