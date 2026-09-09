#include "kave/list.h"
#include "kave/allocator.h"
#include <stdlib.h>

list *list_new(void)
{
    list *l = kave_malloc(sizeof(list));
    if (!l) return NULL;
    l->head = NULL;
    l->tail = NULL;
    l->length = 0;
    return l;
}

void list_free(list *l)
{
    if (!l) return;
    list_node *cur = l->head;
    while (cur) {
        list_node *next = cur->next;
        if (cur->data) kave_free(cur->data);
        kave_free(cur);
        cur = next;
    }
    kave_free(l);
}

list_node *list_node_new(void *data)
{
    list_node *node = kave_malloc(sizeof(list_node));
    if (!node) return NULL;
    node->prev = NULL;
    node->next = NULL;
    node->data = data;
    return node;
}

list *list_append(list *l, void *data)
{
    if (!l) return NULL;
    list_node *node = list_node_new(data);
    if (!node) return NULL;
    if (l->tail) {
        l->tail->next = node;
        node->prev = l->tail;
        l->tail = node;
    } else {
        l->head = l->tail = node;
    }
    l->length++;
    return l;
}

list *list_prepend(list *l, void *data)
{
    if (!l) return NULL;
    list_node *node = list_node_new(data);
    if (!node) return NULL;
    if (l->head) {
        l->head->prev = node;
        node->next = l->head;
        l->head = node;
    } else {
        l->head = l->tail = node;
    }
    l->length++;
    return l;
}

list *list_insert_before(list *l, list_node *node, void *data)
{
    if (!l || !node) return NULL;
    list_node *new_node = list_node_new(data);
    if (!new_node) return NULL;
    new_node->prev = node->prev;
    new_node->next = node;
    if (node->prev) {
        node->prev->next = new_node;
    } else {
        l->head = new_node;
    }
    node->prev = new_node;
    l->length++;
    return l;
}

list *list_insert_after(list *l, list_node *node, void *data)
{
    if (!l || !node) return NULL;
    list_node *new_node = list_node_new(data);
    if (!new_node) return NULL;
    new_node->prev = node;
    new_node->next = node->next;
    if (node->next) {
        node->next->prev = new_node;
    } else {
        l->tail = new_node;
    }
    node->next = new_node;
    l->length++;
    return l;
}

void *list_pop_head(list *l)
{
    if (!l || !l->head) return NULL;
    list_node *node = l->head;
    void *data = node->data;
    l->head = node->next;
    if (l->head) {
        l->head->prev = NULL;
    } else {
        l->tail = NULL;
    }
    kave_free(node);
    l->length--;
    return data;
}

void *list_pop_tail(list *l)
{
    if (!l || !l->tail) return NULL;
    list_node *node = l->tail;
    void *data = node->data;
    l->tail = node->prev;
    if (l->tail) {
        l->tail->next = NULL;
    } else {
        l->head = NULL;
    }
    kave_free(node);
    l->length--;
    return data;
}

int list_delete_node(list *l, list_node *node)
{
    if (!l || !node) return -1;
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        l->head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        l->tail = node->prev;
    }
    if (node->data) kave_free(node->data);
    kave_free(node);
    l->length--;
    return 0;
}

list_node *list_find(const list *l, const void *data, int (*cmp)(const void *, const void *))
{
    if (!l || !cmp) return NULL;
    list_node *cur = l->head;
    while (cur) {
        if (cmp(data, cur->data) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

void list_foreach(const list *l, void (*fn)(void *data, void *user), void *user)
{
    if (!l || !fn) return;
    list_node *cur = l->head;
    while (cur) {
        fn(cur->data, user);
        cur = cur->next;
    }
}

size_t list_length(const list *l)
{
    return l ? l->length : 0;
}

list_node *list_head(const list *l)
{
    return l ? l->head : NULL;
}

list_node *list_tail(const list *l)
{
    return l ? l->tail : NULL;
}
