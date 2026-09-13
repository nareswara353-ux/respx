#include "kave/list.h"
#include "kave/allocator.h"
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

static int *make_int(int v)
{
    int *p = kave_malloc(sizeof(int));
    if (!p) return NULL;
    *p = v;
    return p;
}

int main(void)
{
    list *l = list_new();
    ASSERT(l != NULL, "list_new returns non-null");
    ASSERT(list_length(l) == 0, "new list empty");
    ASSERT(list_head(l) == NULL, "head is NULL on empty");
    ASSERT(list_tail(l) == NULL, "tail is NULL on empty");

    list_append(l, make_int(1));
    list_append(l, make_int(2));
    list_append(l, make_int(3));
    ASSERT(list_length(l) == 3, "length 3 after appends");
    ASSERT(*(int *)list_head(l)->data == 1, "head is 1");
    ASSERT(*(int *)list_tail(l)->data == 3, "tail is 3");
    ASSERT(*(int *)list_head(l)->next->data == 2, "second is 2");

    list_prepend(l, make_int(0));
    ASSERT(list_length(l) == 4, "length 4 after prepend");
    ASSERT(*(int *)list_head(l)->data == 0, "head is 0 after prepend");

    list_node *second = list_head(l)->next;
    list_insert_before(l, second, make_int(99));
    ASSERT(list_length(l) == 5, "length 5 after insert_before");
    ASSERT(*(int *)list_head(l)->next->data == 99, "inserted before second");

    list_insert_after(l, list_tail(l), make_int(100));
    ASSERT(list_length(l) == 6, "length 6 after insert_after");
    ASSERT(*(int *)list_tail(l)->data == 100, "tail is 100");

    int *head_val = (int *)list_pop_head(l);
    ASSERT(head_val != NULL && *head_val == 0, "pop_head returns 0");
    kave_free(head_val);
    ASSERT(list_length(l) == 5, "length 5 after pop_head");

    int *tail_val = (int *)list_pop_tail(l);
    ASSERT(tail_val != NULL && *tail_val == 100, "pop_tail returns 100");
    kave_free(tail_val);
    ASSERT(list_length(l) == 4, "length 4 after pop_tail");

    list_node *third = list_head(l)->next->next;
    int *third_val = (int *)third->data;
    ASSERT(list_delete_node(l, third) == 0, "delete_node succeeds");
    kave_free(third_val);
    ASSERT(list_length(l) == 3, "length 3 after delete_node");

    int counter = 0;
    list_node *node = list_head(l);
    while (node) {
        counter++;
        node = node->next;
    }
    ASSERT(counter == 3, "forward iteration count is 3");

    counter = 0;
    node = list_tail(l);
    while (node) {
        counter++;
        node = node->prev;
    }
    ASSERT(counter == 3, "backward iteration count is 3");

    while (list_head(l) != NULL) {
        int *v = (int *)list_pop_head(l);
        kave_free(v);
    }
    ASSERT(list_length(l) == 0, "empty after draining");
    ASSERT(list_pop_head(l) == NULL, "pop_head on empty returns NULL");
    ASSERT(list_pop_tail(l) == NULL, "pop_tail on empty returns NULL");

    list_free(l);

    kave_alloc_stats stats = kave_get_global_stats();
    ASSERT(stats.active_bytes == 0, "no leaked bytes");
    ASSERT(stats.active_blocks == 0, "no leaked blocks");

    if (failures == 0) {
        printf("test_list: OK\n");
        return 0;
    }
    fprintf(stderr, "test_list: %d failures\n", failures);
    return 1;
}
