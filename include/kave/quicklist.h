#ifndef KAVE_QUICKLIST_H
#define KAVE_QUICKLIST_H

#include <stddef.h>
#include "kave/ziplist.h"

typedef struct quicklist_node {
    ziplist *zl;
    struct quicklist_node *prev;
    struct quicklist_node *next;
} quicklist_node;

typedef struct quicklist {
    quicklist_node *head;
    quicklist_node *tail;
    size_t num_nodes;
    size_t num_entries;
    size_t fill_limit;
} quicklist;

quicklist *ql_new(size_t fill_limit);
void ql_free(quicklist *ql);
size_t ql_length(const quicklist *ql);
size_t ql_nodes(const quicklist *ql);
int ql_push_head(quicklist *ql, const char *value, size_t value_len);
int ql_push_tail(quicklist *ql, const char *value, size_t value_len);
int ql_pop_head(quicklist *ql, char **value, size_t *value_len);
int ql_pop_tail(quicklist *ql, char **value, size_t *value_len);
int ql_get(const quicklist *ql, size_t index, char **value, size_t *value_len);
int ql_delete(quicklist *ql, size_t index);
void ql_foreach(const quicklist *ql, void (*callback)(size_t index, const char *value, size_t value_len, void *user), void *user);

#endif
