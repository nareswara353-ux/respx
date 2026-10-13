#ifndef KAVE_RAXTREE_H
#define KAVE_RAXTREE_H

#include <stddef.h>
#include <stdint.h>

typedef struct raxtree raxtree;
typedef struct raxnode raxnode;

raxtree *rax_new(void);
void rax_free(raxtree *rax);
int rax_insert(raxtree *rax, const unsigned char *key, size_t key_len, void *value);
void *rax_find(const raxtree *rax, const unsigned char *key, size_t key_len);
int rax_delete(raxtree *rax, const unsigned char *key, size_t key_len);
size_t rax_size(const raxtree *rax);
void rax_foreach_prefix(const raxtree *rax, const unsigned char *prefix, size_t prefix_len,
                        void (*callback)(const unsigned char *key, size_t key_len, void *value, void *user),
                        void *user);

#endif
