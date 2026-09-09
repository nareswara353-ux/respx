#ifndef KAVE_INTSET_H
#define KAVE_INTSET_H

#include <stddef.h>
#include <stdint.h>

typedef struct intset {
    uint32_t length;
    uint32_t alloc;
    uint8_t encoding;
    uint8_t *contents;
} intset;

#define INTSET_ENC_INT16 0
#define INTSET_ENC_INT32 1
#define INTSET_ENC_INT64 2

intset *intset_new(void);
void intset_free(intset *is);
int intset_add(intset *is, int64_t value);
int intset_remove(intset *is, int64_t value);
int intset_contains(const intset *is, int64_t value);
size_t intset_size(const intset *is);
int64_t intset_get(const intset *is, size_t index);
void intset_foreach(const intset *is, void (*callback)(int64_t value, void *user), void *user);

#endif
