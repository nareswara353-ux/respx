#include "kave/skiplist.h"
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
    skiplist *sl = sl_new();
    ASSERT(sl != NULL, "sl_new returns non-null");
    ASSERT(sl_count(sl) == 0, "new skiplist empty");
    ASSERT(sl_first(sl) == NULL, "first is NULL on empty");
    ASSERT(sl_last(sl) == NULL, "last is NULL on empty");

    char *m1 = sds_new("alice");
    ASSERT(sl_insert(sl, 1.0, "alice", m1) == 0, "insert alice");
    ASSERT(sl_count(sl) == 1, "count is 1");

    void *found = sl_find(sl, 1.0, "alice");
    ASSERT(found == m1, "find returns inserted value");
    ASSERT(strcmp((char *)found, "alice") == 0, "value content correct");

    ASSERT(sl_find(sl, 1.0, "bob") == NULL, "missing member returns NULL");
    ASSERT(sl_find(sl, 2.0, "alice") == NULL, "wrong score returns NULL");

    char *m1b = sds_new("alice_updated");
    ASSERT(sl_insert(sl, 1.0, "alice", m1b) == 0, "update existing member");
    ASSERT(sl_count(sl) == 1, "count still 1 after update");
    found = sl_find(sl, 1.0, "alice");
    ASSERT(strcmp((char *)found, "alice_updated") == 0, "value updated");

    ASSERT(sl_delete(sl, 1.0, "alice") == 0, "delete existing member");
    ASSERT(sl_count(sl) == 0, "count 0 after delete");
    ASSERT(sl_find(sl, 1.0, "alice") == NULL, "find after delete NULL");
    ASSERT(sl_delete(sl, 1.0, "alice") == -1, "delete missing returns -1");

    const char *names[] = {"zoe", "alice", "bob", "charlie", "dave"};
    double scores[] = {50.0, 10.0, 20.0, 30.0, 40.0};
    for (int i = 0; i < 5; i++) {
        char *m = sds_new(names[i]);
        ASSERT(sl_insert(sl, scores[i], names[i], m) == 0, "bulk insert");
    }
    ASSERT(sl_count(sl) == 5, "count is 5 after bulk");

    skiplist_node *node = sl_first(sl);
    ASSERT(node != NULL, "first is not NULL");
    ASSERT(strcmp(node->key, "alice") == 0, "first is alice (score 10)");
    ASSERT(node->score == 10.0, "first score is 10");

    node = sl_next(node);
    ASSERT(node && strcmp(node->key, "bob") == 0, "second is bob");
    node = sl_next(node);
    ASSERT(node && strcmp(node->key, "charlie") == 0, "third is charlie");
    node = sl_next(node);
    ASSERT(node && strcmp(node->key, "dave") == 0, "fourth is dave");
    node = sl_next(node);
    ASSERT(node && strcmp(node->key, "zoe") == 0, "fifth is zoe");
    node = sl_next(node);
    ASSERT(node == NULL, "after last is NULL");

    node = sl_last(sl);
    ASSERT(node && strcmp(node->key, "zoe") == 0, "last is zoe");
    node = sl_prev(node);
    ASSERT(node && strcmp(node->key, "dave") == 0, "prev of zoe is dave");

    ASSERT(sl_delete(sl, 30.0, "charlie") == 0, "delete middle");
    ASSERT(sl_count(sl) == 4, "count 4 after delete");
    ASSERT(sl_find(sl, 30.0, "charlie") == NULL, "charlie removed");
    ASSERT(sl_find(sl, 20.0, "bob") != NULL, "bob still present");
    ASSERT(sl_find(sl, 40.0, "dave") != NULL, "dave still present");

    sl_free(sl);

    kave_alloc_stats stats = kave_get_global_stats();
    ASSERT(stats.active_bytes == 0, "no leaked bytes");
    ASSERT(stats.active_blocks == 0, "no leaked blocks");

    if (failures == 0) {
        printf("test_skiplist: OK\n");
        return 0;
    }
    fprintf(stderr, "test_skiplist: %d failures\n", failures);
    return 1;
}
