#ifndef KAVE_LIST_H
#define KAVE_LIST_H

#include <stddef.h>

typedef struct list_node {
    void *data;
    struct list_node *prev;
    struct list_node *next;
} list_node;

typedef struct list {
    list_node *head;
    list_node *tail;
    size_t length;
    void (*free_data)(void *);
} list;

list *list_new(void (*free_data)(void *));
void list_free(list *l);
int list_push_head(list *l, void *data);
int list_push_tail(list *l, void *data);
void *list_pop_head(list *l);
void *list_pop_tail(list *l);
void *list_remove_node(list *l, list_node *node);
list_node *list_find(const list *l, const void *data, int (*cmp)(const void *, const void *));
void list_foreach(const list *l, void (*callback)(void *data, void *userdata), void *userdata);
size_t list_length(const list *l);
list_node *list_head(const list *l);
list_node *list_tail(const list *l);
list_node *list_next(const list_node *node);
list_node *list_prev(const list_node *node);

#endif
