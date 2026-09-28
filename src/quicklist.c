#include "kave/quicklist.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>

#define QL_DEFAULT_FILL 128

static quicklist_node *ql_node_new(void)
{
    quicklist_node *node = kave_malloc(sizeof(quicklist_node));
    if (!node) return NULL;
    node->zl = zl_new();
    if (!node->zl) {
        kave_free(node);
        return NULL;
    }
    node->prev = NULL;
    node->next = NULL;
    return node;
}

static void ql_node_free(quicklist_node *node)
{
    if (!node) return;
    if (node->zl) zl_free(node->zl);
    kave_free(node);
}

quicklist *ql_new(size_t fill_limit)
{
    quicklist *ql = kave_malloc(sizeof(quicklist));
    if (!ql) return NULL;
    ql->head = NULL;
    ql->tail = NULL;
    ql->num_nodes = 0;
    ql->num_entries = 0;
    ql->fill_limit = fill_limit > 0 ? fill_limit : QL_DEFAULT_FILL;
    return ql;
}

void ql_free(quicklist *ql)
{
    if (!ql) return;
    quicklist_node *node = ql->head;
    while (node) {
        quicklist_node *next = node->next;
        ql_node_free(node);
        node = next;
    }
    kave_free(ql);
}

size_t ql_length(const quicklist *ql)
{
    return ql ? ql->num_entries : 0;
}

size_t ql_nodes(const quicklist *ql)
{
    return ql ? ql->num_nodes : 0;
}

int ql_push_tail(quicklist *ql, const char *value, size_t value_len)
{
    if (!ql) return -1;
    if (!ql->tail || zl_entries(ql->tail->zl) >= ql->fill_limit) {
        quicklist_node *node = ql_node_new();
        if (!node) return -1;
        if (ql->tail) {
            ql->tail->next = node;
            node->prev = ql->tail;
            ql->tail = node;
        } else {
            ql->head = ql->tail = node;
        }
        ql->num_nodes++;
    }
    if (zl_push_tail(ql->tail->zl, value, value_len) < 0) return -1;
    ql->num_entries++;
    return 0;
}

int ql_push_head(quicklist *ql, const char *value, size_t value_len)
{
    if (!ql) return -1;
    if (!ql->head || zl_entries(ql->head->zl) >= ql->fill_limit) {
        quicklist_node *node = ql_node_new();
        if (!node) return -1;
        if (ql->head) {
            ql->head->prev = node;
            node->next = ql->head;
            ql->head = node;
        } else {
            ql->head = ql->tail = node;
        }
        ql->num_nodes++;
    }
    if (zl_push_head(ql->head->zl, value, value_len) < 0) return -1;
    ql->num_entries++;
    return 0;
}

int ql_pop_head(quicklist *ql, char **value, size_t *value_len)
{
    if (!ql || !ql->head) return -1;
    quicklist_node *node = ql->head;
    char *v = NULL;
    size_t vlen = 0;
    if (zl_get(node->zl, 0, &v, &vlen) < 0) return -1;
    if (value) {
        char *copy = kave_malloc(vlen + 1);
        if (!copy) return -1;
        memcpy(copy, v, vlen);
        copy[vlen] = '\0';
        *value = copy;
    }
    if (value_len) *value_len = vlen;
    zl_delete(node->zl, 0);
    ql->num_entries--;
    if (zl_entries(node->zl) == 0) {
        ql->head = node->next;
        if (ql->head) ql->head->prev = NULL;
        else ql->tail = NULL;
        ql_node_free(node);
        ql->num_nodes--;
    }
    return 0;
}

int ql_pop_tail(quicklist *ql, char **value, size_t *value_len)
{
    if (!ql || !ql->tail) return -1;
    quicklist_node *node = ql->tail;
    size_t last = zl_entries(node->zl) - 1;
    char *v = NULL;
    size_t vlen = 0;
    if (zl_get(node->zl, last, &v, &vlen) < 0) return -1;
    if (value) {
        char *copy = kave_malloc(vlen + 1);
        if (!copy) return -1;
        memcpy(copy, v, vlen);
        copy[vlen] = '\0';
        *value = copy;
    }
    if (value_len) *value_len = vlen;
    zl_delete(node->zl, last);
    ql->num_entries--;
    if (zl_entries(node->zl) == 0) {
        ql->tail = node->prev;
        if (ql->tail) ql->tail->next = NULL;
        else ql->head = NULL;
        ql_node_free(node);
        ql->num_nodes--;
    }
    return 0;
}

static quicklist_node *ql_find_node(const quicklist *ql, size_t *index)
{
    size_t remaining = *index;
    quicklist_node *node = ql->head;
    while (node) {
        size_t count = zl_entries(node->zl);
        if (remaining < count) {
            *index = remaining;
            return node;
        }
        remaining -= count;
        node = node->next;
    }
    return NULL;
}

int ql_get(const quicklist *ql, size_t index, char **value, size_t *value_len)
{
    if (!ql || index >= ql->num_entries) return -1;
    size_t local = index;
    quicklist_node *node = ql_find_node(ql, &local);
    if (!node) return -1;
    return zl_get(node->zl, local, value, value_len);
}

int ql_delete(quicklist *ql, size_t index)
{
    if (!ql || index >= ql->num_entries) return -1;
    size_t local = index;
    quicklist_node *node = ql_find_node(ql, &local);
    if (!node) return -1;
    if (zl_delete(node->zl, local) < 0) return -1;
    ql->num_entries--;
    if (zl_entries(node->zl) == 0) {
        if (node->prev) node->prev->next = node->next;
        else ql->head = node->next;
        if (node->next) node->next->prev = node->prev;
        else ql->tail = node->prev;
        ql_node_free(node);
        ql->num_nodes--;
    }
    return 0;
}

void ql_foreach(const quicklist *ql, void (*callback)(size_t index, const char *value, size_t value_len, void *user), void *user)
{
    if (!ql || !callback) return;
    size_t global = 0;
    quicklist_node *node = ql->head;
    while (node) {
        size_t count = zl_entries(node->zl);
        for (size_t i = 0; i < count; i++) {
            char *v = NULL;
            size_t vlen = 0;
            zl_get(node->zl, i, &v, &vlen);
            callback(global++, v, vlen, user);
        }
        node = node->next;
    }
}
