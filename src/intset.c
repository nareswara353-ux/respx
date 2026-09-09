#include "kave/intset.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define INTSET_ENC_INT16 0
#define INTSET_ENC_INT32 1
#define INTSET_ENC_INT64 2

static uint8_t intset_encoding_for_value(int64_t v)
{
    if (v >= INT16_MIN && v <= INT16_MAX) return INTSET_ENC_INT16;
    if (v >= INT32_MIN && v <= INT32_MAX) return INTSET_ENC_INT32;
    return INTSET_ENC_INT64;
}

static size_t intset_encoding_size(uint8_t enc)
{
    switch (enc) {
        case INTSET_ENC_INT16: return 2;
        case INTSET_ENC_INT32: return 4;
        case INTSET_ENC_INT64: return 8;
        default: return 0;
    }
}

static void intset_set_value(void *ptr, uint8_t enc, size_t idx, int64_t val)
{
    size_t sz = intset_encoding_size(enc);
    switch (enc) {
        case INTSET_ENC_INT16: ((int16_t *)ptr)[idx] = (int16_t)val; break;
        case INTSET_ENC_INT32: ((int32_t *)ptr)[idx] = (int32_t)val; break;
        case INTSET_ENC_INT64: ((int64_t *)ptr)[idx] = val; break;
    }
}

static int64_t intset_get_value(const void *ptr, uint8_t enc, size_t idx)
{
    size_t sz = intset_encoding_size(enc);
    switch (enc) {
        case INTSET_ENC_INT16: return ((int16_t *)ptr)[idx];
        case INTSET_ENC_INT32: return ((int32_t *)ptr)[idx];
        case INTSET_ENC_INT64: return ((int64_t *)ptr)[idx];
        default: return 0;
    }
}

static int intset_value_compare(const void *ptr, uint8_t enc, size_t idx, int64_t val)
{
    int64_t cur = intset_get_value(ptr, enc, idx);
    if (cur < val) return -1;
    if (cur > val) return 1;
    return 0;
}

intset *intset_new(void)
{
    intset *is = kave_malloc(sizeof(intset));
    if (!is) return NULL;
    is->encoding = INTSET_ENC_INT16;
    is->length = 0;
    is->contents = NULL;
    return is;
}

void intset_free(intset *is)
{
    if (!is) return;
    if (is->contents) kave_free(is->contents);
    kave_free(is);
}

static intset *intset_upgrade_and_add(intset *is, int64_t value)
{
    uint8_t new_enc = intset_encoding_for_value(value);
    uint8_t old_enc = is->encoding;
    size_t old_sz = intset_encoding_size(old_enc);
    size_t new_sz = intset_encoding_size(new_enc);
    size_t len = is->length;
    void *new_contents = kave_malloc((len + 1) * new_sz);
    if (!new_contents) return NULL;
    size_t i = 0, j = 0;
    while (i < len && intset_value_compare(is->contents, old_enc, i, value) < 0) {
        intset_set_value(new_contents, new_enc, j, intset_get_value(is->contents, old_enc, i));
        i++; j++;
    }
    intset_set_value(new_contents, new_enc, j, value);
    j++;
    while (i < len) {
        intset_set_value(new_contents, new_enc, j, intset_get_value(is->contents, old_enc, i));
        i++; j++;
    }
    kave_free(is->contents);
    is->contents = new_contents;
    is->encoding = new_enc;
    is->length = len + 1;
    return is;
}

intset *intset_add(intset *is, int64_t value)
{
    if (!is) return NULL;
    uint8_t enc = is->encoding;
    size_t len = is->length;
    void *ptr = is->contents;
    size_t lo = 0, hi = len;
    while (lo < hi) {
        size_t mid = (lo + hi) >> 1;
        int cmp = intset_value_compare(ptr, enc, mid, value);
        if (cmp < 0) lo = mid + 1;
        else if (cmp > 0) hi = mid;
        else return is;
    }
    if (intset_encoding_for_value(value) > enc) {
        return intset_upgrade_and_add(is, value);
    }
    size_t sz = intset_encoding_size(enc);
    void *new_contents = kave_malloc((len + 1) * sz);
    if (!new_contents) return NULL;
    if (lo > 0) {
        memcpy(new_contents, ptr, lo * sz);
    }
    intset_set_value(new_contents, enc, lo, value);
    if (lo < len) {
        memcpy((char *)new_contents + (lo + 1) * sz, (char *)ptr + lo * sz, (len - lo) * sz);
    }
    kave_free(is->contents);
    is->contents = new_contents;
    is->length = len + 1;
    return is;
}

intset *intset_remove(intset *is, int64_t value)
{
    if (!is || is->length == 0) return is;
    uint8_t enc = is->encoding;
    size_t len = is->length;
    void *ptr = is->contents;
    size_t lo = 0, hi = len;
    while (lo < hi) {
        size_t mid = (lo + hi) >> 1;
        int cmp = intset_value_compare(ptr, enc, mid, value);
        if (cmp < 0) lo = mid + 1;
        else if (cmp > 0) hi = mid;
        else {
            size_t sz = intset_encoding_size(enc);
            void *new_contents = kave_malloc((len - 1) * sz);
            if (!new_contents) return NULL;
            if (mid > 0) {
                memcpy(new_contents, ptr, mid * sz);
            }
            if (mid + 1 < len) {
                memcpy((char *)new_contents + mid * sz, (char *)ptr + (mid + 1) * sz, (len - mid - 1) * sz);
            }
            kave_free(is->contents);
            is->contents = new_contents;
            is->length = len - 1;
            return is;
        }
    }
    return is;
}

int intset_find(const intset *is, int64_t value)
{
    if (!is || is->length == 0) return 0;
    uint8_t enc = is->encoding;
    size_t len = is->length;
    void *ptr = is->contents;
    size_t lo = 0, hi = len;
    while (lo < hi) {
        size_t mid = (lo + hi) >> 1;
        int cmp = intset_value_compare(ptr, enc, mid, value);
        if (cmp < 0) lo = mid + 1;
        else if (cmp > 0) hi = mid;
        else return 1;
    }
    return 0;
}

int64_t intset_random(const intset *is)
{
    if (!is || is->length == 0) return 0;
    size_t idx = rand() % is->length;
    return intset_get_value(is->contents, is->encoding, idx);
}

uint32_t intset_len(const intset *is)
{
    return is ? is->length : 0;
}

static intset *intset_clone(const intset *is)
{
    intset *clone = intset_new();
    if (!clone) return NULL;
    if (is->length == 0) return clone;
    size_t sz = intset_encoding_size(is->encoding) * is->length;
    clone->encoding = is->encoding;
    clone->length = is->length;
    clone->contents = kave_malloc(sz);
    if (!clone->contents) {
        intset_free(clone);
        return NULL;
    }
    memcpy(clone->contents, is->contents, sz);
    return clone;
}

intset *intset_union(const intset *a, const intset *b)
{
    if (!a && !b) return NULL;
    if (!a) return intset_clone(b);
    if (!b) return intset_clone(a);
    intset *result = intset_clone(a);
    if (!result) return NULL;
    uint8_t enc = b->encoding;
    size_t len = b->length;
    for (size_t i = 0; i < len; i++) {
        int64_t val = intset_get_value(b->contents, enc, i);
        result = intset_add(result, val);
        if (!result) return NULL;
    }
    return result;
}

intset *intset_intersection(const intset *a, const intset *b)
{
    if (!a || !b) return intset_new();
    intset *result = intset_new();
    if (!result) return NULL;
    uint8_t enc = a->encoding;
    size_t len = a->length;
    for (size_t i = 0; i < len; i++) {
        int64_t val = intset_get_value(a->contents, enc, i);
        if (intset_find(b, val)) {
            result = intset_add(result, val);
            if (!result) return NULL;
        }
    }
    return result;
}

intset *intset_difference(const intset *a, const intset *b)
{
    if (!a) return intset_new();
    if (!b) return intset_clone(a);
    intset *result = intset_clone(a);
    if (!result) return NULL;
    uint8_t enc = b->encoding;
    size_t len = b->length;
    for (size_t i = 0; i < len; i++) {
        int64_t val = intset_get_value(b->contents, enc, i);
        result = intset_remove(result, val);
        if (!result) return NULL;
    }
    return result;
}

void intset_foreach(const intset *is, void (*fn)(int64_t value, void *user), void *user)
{
    if (!is || !fn) return;
    uint8_t enc = is->encoding;
    size_t len = is->length;
    for (size_t i = 0; i < len; i++) {
        fn(intset_get_value(is->contents, enc, i), user);
    }
}
