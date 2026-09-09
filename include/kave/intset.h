#ifndef KAVE_INTSET_H
#define KAVE_INTSET_H

#include <stddef.h>
#include <stdint.h>

typedef struct intset {
    uint32_t encoding;
    uint32_t length;
    void *contents;
} intset;

intset *intset_new(void);
void intset_free(intset *is);
intset *intset_add(intset *is, int64_t value);
intset *intset_remove(intset *is, int64_t value);
int intset_find(const intset *is, int64_t value);
int64_t intset_random(const intset *is);
uint32_t intset_len(const intset *is);
intset *intset_union(const intset *a, const intset *b);
intset *intset_intersection(const intset *a, const intset *b);
intset *intset_difference(const intset *a, const intset *b);
void intset_foreach(const intset *is, void (*fn)(int64_t value, void *user), void *user);

#endif
