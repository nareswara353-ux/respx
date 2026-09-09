#include "kave/skiplist.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define SL_MAX_LEVEL 32
#define SL_PROBABILITY 0.25

static int sl_random_level(void)
{
    int level = 1;
    while ((rand() & 0xFFFF) < (SL_PROBABILITY * 0xFFFF) && level < SL_MAX_LEVEL) {
        level++;
    }
    return level;
}

skiplist *sl_new(void)
{
    skiplist *list = kave_malloc(sizeof(skiplist));
    if (!list) return NULL;
    list->max_level = SL_MAX_LEVEL;
    list->length = 0;
    list->tail = NULL;
    list->header = kave_malloc(sizeof(skiplist_node));
    if (!list->header) {
        kave_free(list);
        return NULL;
    }
    list->header->forward = kave_calloc(list->max_level, sizeof(skiplist_node *));
    if (!list->header->forward) {
        kave_free(list->header);
        kave_free(list);
        return NULL;
    }
    list->header->key = NULL;
    list->header->value = NULL;
    list->header->score = 0.0;
    list->header->backward = NULL;
    list->header->level = 0;
    for (int i = 0; i < list->max_level; i++) {
        list->header->forward[i] = NULL;
    }
    return list;
}

static void sl_free_node(skiplist_node *node)
{
    if (!node) return;
    if (node->key) kave_free(node->key);
    if (node->value) kave_free(node->value);
    if (node->forward) kave_free(node->forward);
    kave_free(node);
}

void sl_free(skiplist *list)
{
    if (!list) return;
    skiplist_node *cur = list->header->forward[0];
    while (cur) {
        skiplist_node *next = cur->forward[0];
        sl_free_node(cur);
        cur = next;
    }
    kave_free(list->header->forward);
    kave_free(list->header);
    kave_free(list);
}

static skiplist_node *sl_create_node(int level, double score, const char *key, void *value)
{
    skiplist_node *node = kave_malloc(sizeof(skiplist_node));
    if (!node) return NULL;
    node->forward = kave_calloc(level, sizeof(skiplist_node *));
    if (!node->forward) {
        kave_free(node);
        return NULL;
    }
    node->key = kave_malloc(strlen(key) + 1);
    if (!node->key) {
        kave_free(node->forward);
        kave_free(node);
        return NULL;
    }
    strcpy(node->key, key);
    node->value = value;
    node->score = score;
    node->backward = NULL;
    node->level = level;
    for (int i = 0; i < level; i++) {
        node->forward[i] = NULL;
    }
    return node;
}

int sl_insert(skiplist *list, double score, const char *key, void *value)
{
    if (!list || !key) return -1;
    skiplist_node *update[SL_MAX_LEVEL];
    skiplist_node *cur = list->header;
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (cur->forward[i] && 
               (cur->forward[i]->score < score || 
                (cur->forward[i]->score == score && 
                 strcmp(cur->forward[i]->key, key) < 0))) {
            cur = cur->forward[i];
        }
        update[i] = cur;
    }
    cur = cur->forward[0];
    if (cur && cur->score == score && strcmp(cur->key, key) == 0) {
        if (cur->value) kave_free(cur->value);
        cur->value = value;
        return 0;
    }
    int level = sl_random_level();
    skiplist_node *new_node = sl_create_node(level, score, key, value);
    if (!new_node) return -1;
    if (level > list->max_level) {
        for (int i = list->max_level; i < level; i++) {
            update[i] = list->header;
        }
        list->max_level = level;
    }
    for (int i = 0; i < level; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }
    new_node->backward = (update[0] == list->header) ? NULL : update[0];
    if (new_node->forward[0]) {
        new_node->forward[0]->backward = new_node;
    } else {
        list->tail = new_node;
    }
    list->length++;
    return 0;
}

int sl_delete(skiplist *list, double score, const char *key)
{
    if (!list || !key) return -1;
    skiplist_node *update[SL_MAX_LEVEL];
    skiplist_node *cur = list->header;
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (cur->forward[i] && 
               (cur->forward[i]->score < score || 
                (cur->forward[i]->score == score && 
                 strcmp(cur->forward[i]->key, key) < 0))) {
            cur = cur->forward[i];
        }
        update[i] = cur;
    }
    cur = cur->forward[0];
    if (!cur || cur->score != score || strcmp(cur->key, key) != 0) {
        return -1;
    }
    for (int i = 0; i < list->max_level; i++) {
        if (update[i]->forward[i] == cur) {
            update[i]->forward[i] = cur->forward[i];
        }
    }
    if (cur->forward[0]) {
        cur->forward[0]->backward = cur->backward;
    } else {
        list->tail = cur->backward;
    }
    sl_free_node(cur);
    list->length--;
    return 0;
}

void *sl_find(const skiplist *list, double score, const char *key)
{
    if (!list || !key) return NULL;
    skiplist_node *cur = list->header;
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (cur->forward[i] && 
               (cur->forward[i]->score < score || 
                (cur->forward[i]->score == score && 
                 strcmp(cur->forward[i]->key, key) < 0))) {
            cur = cur->forward[i];
        }
    }
    cur = cur->forward[0];
    if (cur && cur->score == score && strcmp(cur->key, key) == 0) {
        return cur->value;
    }
    return NULL;
}

skiplist_node *sl_first(const skiplist *list)
{
    return list ? list->header->forward[0] : NULL;
}

skiplist_node *sl_last(const skiplist *list)
{
    return list ? list->tail : NULL;
}

skiplist_node *sl_next(const skiplist_node *node)
{
    return node ? node->forward[0] : NULL;
}

skiplist_node *sl_prev(const skiplist_node *node)
{
    return node ? node->backward : NULL;
}

size_t sl_count(const skiplist *list)
{
    return list ? list->length : 0;
}

void sl_foreach(const skiplist *list, void (*callback)(const char *key, double score, void *value, void *userdata), void *userdata)
{
    if (!list || !callback) return;
    skiplist_node *cur = list->header->forward[0];
    while (cur) {
        callback(cur->key, cur->score, cur->value, userdata);
        cur = cur->forward[0];
    }
}
