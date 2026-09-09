#ifndef KAVE_SKIPLIST_H
#define KAVE_SKIPLIST_H

#include <stddef.h>

typedef struct skiplist_node {
    char *key;
    void *value;
    double score;
    struct skiplist_node **forward;
    struct skiplist_node *backward;
    int level;
} skiplist_node;

typedef struct skiplist {
    skiplist_node *header;
    skiplist_node *tail;
    size_t length;
    int max_level;
} skiplist;

skiplist *sl_new(void);
void sl_free(skiplist *list);
int sl_insert(skiplist *list, double score, const char *key, void *value);
int sl_delete(skiplist *list, double score, const char *key);
void *sl_find(const skiplist *list, double score, const char *key);
skiplist_node *sl_first(const skiplist *list);
skiplist_node *sl_last(const skiplist *list);
skiplist_node *sl_next(const skiplist_node *node);
skiplist_node *sl_prev(const skiplist_node *node);
size_t sl_count(const skiplist *list);
void sl_foreach(const skiplist *list, void (*callback)(const char *key, double score, void *value, void *userdata), void *userdata);

#endif
