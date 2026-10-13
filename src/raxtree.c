#include "kave/raxtree.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>

#define RAX_MAX_CHILDREN 256

struct raxnode {
    unsigned char *label;
    size_t label_len;
    void *value;
    int has_value;
    struct raxnode *children[RAX_MAX_CHILDREN];
};

struct raxtree {
    raxnode *root;
    size_t num_entries;
};

static raxnode *raxnode_new(const unsigned char *label, size_t label_len)
{
    raxnode *n = kave_malloc(sizeof(raxnode));
    if (!n) return NULL;
    memset(n->children, 0, sizeof(n->children));
    n->label = kave_malloc(label_len);
    if (!n->label) {
        kave_free(n);
        return NULL;
    }
    memcpy(n->label, label, label_len);
    n->label_len = label_len;
    n->value = NULL;
    n->has_value = 0;
    return n;
}

static void raxnode_free(raxnode *n)
{
    if (!n) return;
    for (int i = 0; i < RAX_MAX_CHILDREN; i++) {
        if (n->children[i]) raxnode_free(n->children[i]);
    }
    if (n->label) kave_free(n->label);
    kave_free(n);
}

raxtree *rax_new(void)
{
    raxtree *rax = kave_malloc(sizeof(raxtree));
    if (!rax) return NULL;
    rax->root = raxnode_new((const unsigned char *)"", 0);
    if (!rax->root) {
        kave_free(rax);
        return NULL;
    }
    rax->num_entries = 0;
    return rax;
}

void rax_free(raxtree *rax)
{
    if (!rax) return;
    if (rax->root) raxnode_free(rax->root);
    kave_free(rax);
}

static size_t rax_common_prefix(const unsigned char *a, size_t a_len,
                                const unsigned char *b, size_t b_len)
{
    size_t i = 0;
    while (i < a_len && i < b_len && a[i] == b[i]) i++;
    return i;
}

static raxnode *rax_find_child(raxnode *n, unsigned char c)
{
    return n->children[c];
}

static raxnode *rax_node_with_label(raxtree *rax, const unsigned char *label, size_t label_len, int create)
{
    (void)rax;
    if (!label_len) return rax->root;
    unsigned char first = label[0];
    if (create && !rax->root->children[first]) {
        rax->root->children[first] = raxnode_new(label, label_len);
    }
    raxnode *cur = rax->root->children[first];
    size_t matched = 0;
    while (cur && matched < label_len) {
        size_t common = rax_common_prefix(cur->label, cur->label_len, label + matched, label_len - matched);
        if (common == cur->label_len) {
            matched += common;
            if (matched == label_len) return cur;
            unsigned char next = label[matched];
            if (!cur->children[next]) {
                if (!create) return NULL;
                cur->children[next] = raxnode_new(label + matched, label_len - matched);
            }
            cur = cur->children[next];
        } else {
            return NULL;
        }
    }
    return cur;
}

int rax_insert(raxtree *rax, const unsigned char *key, size_t key_len, void *value)
{
    if (!rax || !key) return -1;
    if (key_len == 0) {
        if (!rax->root->has_value) rax->num_entries++;
        rax->root->value = value;
        rax->root->has_value = 1;
        return 0;
    }
    unsigned char first = key[0];
    if (!rax->root->children[first]) {
        rax->root->children[first] = raxnode_new(key, key_len);
        if (!rax->root->children[first]) return -1;
        rax->root->children[first]->value = value;
        rax->root->children[first]->has_value = 1;
        rax->num_entries++;
        return 0;
    }
    raxnode *cur = rax->root->children[first];
    size_t matched = 0;
    while (matched < key_len) {
        size_t common = rax_common_prefix(cur->label, cur->label_len, key + matched, key_len - matched);
        if (common < cur->label_len) {
            raxnode *split = raxnode_new(cur->label, common);
            if (!split) return -1;
            memmove(cur->label, cur->label + common, cur->label_len - common);
            cur->label_len -= common;
            split->children[cur->label[0]] = cur;
            rax->root->children[first] = split;
            cur = split;
            matched += common;
            break;
        }
        matched += common;
        if (matched == key_len) {
            if (!cur->has_value) rax->num_entries++;
            cur->value = value;
            cur->has_value = 1;
            return 0;
        }
        unsigned char next = key[matched];
        if (!cur->children[next]) {
            cur->children[next] = raxnode_new(key + matched, key_len - matched);
            if (!cur->children[next]) return -1;
            cur->children[next]->value = value;
            cur->children[next]->has_value = 1;
            rax->num_entries++;
            return 0;
        }
        cur = cur->children[next];
    }
    if (!cur->has_value) rax->num_entries++;
    cur->value = value;
    cur->has_value = 1;
    return 0;
}

void *rax_find(const raxtree *rax, const unsigned char *key, size_t key_len)
{
    if (!rax || !key) return NULL;
    if (key_len == 0) return rax->root->has_value ? rax->root->value : NULL;
    raxnode *cur = rax->root->children[key[0]];
    size_t matched = 0;
    while (cur && matched < key_len) {
        if (cur->label_len > key_len - matched) return NULL;
        if (memcmp(cur->label, key + matched, cur->label_len) != 0) return NULL;
        matched += cur->label_len;
        if (matched == key_len) {
            return cur->has_value ? cur->value : NULL;
        }
        cur = cur->children[key[matched]];
    }
    return NULL;
}

int rax_delete(raxtree *rax, const unsigned char *key, size_t key_len)
{
    if (!rax || !key) return -1;
    if (key_len == 0) {
        if (!rax->root->has_value) return -1;
        rax->root->has_value = 0;
        rax->root->value = NULL;
        rax->num_entries--;
        return 0;
    }
    raxnode *cur = rax->root->children[key[0]];
    size_t matched = 0;
    while (cur && matched < key_len) {
        if (cur->label_len > key_len - matched) return -1;
        if (memcmp(cur->label, key + matched, cur->label_len) != 0) return -1;
        matched += cur->label_len;
        if (matched == key_len) {
            if (!cur->has_value) return -1;
            cur->has_value = 0;
            cur->value = NULL;
            rax->num_entries--;
            return 0;
        }
        cur = cur->children[key[matched]];
    }
    return -1;
}

size_t rax_size(const raxtree *rax)
{
    return rax ? rax->num_entries : 0;
}

static void rax_walk(raxnode *n, unsigned char *buf, size_t depth, size_t max_depth,
                     void (*callback)(const unsigned char *key, size_t key_len, void *value, void *user),
                     void *user)
{
    if (depth + n->label_len > max_depth) return;
    memcpy(buf + depth, n->label, n->label_len);
    size_t new_depth = depth + n->label_len;
    if (n->has_value) callback(buf, new_depth, n->value, user);
    for (int i = 0; i < RAX_MAX_CHILDREN; i++) {
        if (n->children[i]) rax_walk(n->children[i], buf, new_depth, max_depth, callback, user);
    }
}

void rax_foreach_prefix(const raxtree *rax, const unsigned char *prefix, size_t prefix_len,
                        void (*callback)(const unsigned char *key, size_t key_len, void *value, void *user),
                        void *user)
{
    if (!rax || !callback) return;
    if (prefix_len == 0) {
        unsigned char buf[1024];
        for (int i = 0; i < RAX_MAX_CHILDREN; i++) {
            if (rax->root->children[i]) rax_walk(rax->root->children[i], buf, 0, sizeof(buf), callback, user);
        }
        return;
    }
    raxnode *cur = rax->root->children[prefix[0]];
    size_t matched = 0;
    while (cur && matched < prefix_len) {
        if (cur->label_len > prefix_len - matched) return;
        if (memcmp(cur->label, prefix + matched, cur->label_len) != 0) return;
        matched += cur->label_len;
        if (matched == prefix_len) {
            unsigned char buf[1024];
            rax_walk(cur, buf, 0, sizeof(buf), callback, user);
            return;
        }
        cur = cur->children[prefix[matched]];
    }
}
