#ifndef KAVE_LIST_H
#define KAVE_LIST_H

#include <stddef.h>

typedef struct list_node {
    struct list_node *prev;
    struct list_node *next;
    void *data;
} list_node;

typedef struct list {
    list_node *head;
    list_node *tail;
    size_t length;
} list;

list *list_new(void);
void list_free(list *l);
list_node *list_node_new(void *data);
list *list_append(list *l, void *data);
list *list_prepend(list *l, void *data);
list *list_insert_before(list *l, list_node *node, void *data);
list *list_insert_after(list *l, list_node *node, void *data);
void *list_pop_head(list *l);
void *list_pop_tail(list *l);
int list_delete_node(list *l, list_node *node);
list_node *list_find(const list *l, const void *data, int (*cmp)(const void *, const void *));
void list_foreach(const list *l, void (*fn)(void *data, void *user), void *user);
size_t list_length(const list *l);
list_node *list_head(const list *l);
list_node *list_tail(const list *l);

#endif
