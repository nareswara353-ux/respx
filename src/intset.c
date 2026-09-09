#include "kave/intset.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define INTSET_ENC_INT16 0
#define INTSET_ENC_INT32 1
#define INTSET_ENC_INT64 2

static size_t intset_encoding_size(uint32_t enc)
{
    switch (enc) {
        case INTSET_ENC_INT16: return sizeof(int16_t);
        case INTSET_ENC_INT32: return sizeof(int32_t);
        case INTSET_ENC_INT64: return sizeof(int64_t);
        default: return 0;
    }
}

static uint32_t intset_encoding_for_value(int64_t value)
{
    if (value >= INT16_MIN && value <= INT16_MAX) return INTSET_ENC_INT16;
    if (value >= INT32_MIN && value <= INT32_MAX) return INTSET_ENC_INT32;
    return INTSET_ENC_INT64;
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

static int intset_search(const intset *is, int64_t value, uint32_t *pos)
{
    if (is->length == 0) {
        if (pos) *pos = 0;
        return 0;
    }
    size_t sz = intset_encoding_size(is->encoding);
    uint32_t lo = 0, hi = is->length - 1;
    while (lo <= hi) {
        uint32_t mid = (lo + hi) / 2;
        int64_t mid_val;
        switch (is->encoding) {
            case INTSET_ENC_INT16:
                mid_val = (int64_t)((int16_t *)is->contents)[mid];
                break;
            case INTSET_ENC_INT32:
                mid_val = (int64_t)((int32_t *)is->contents)[mid];
                break;
            case INTSET_ENC_INT64:
                mid_val = ((int64_t *)is->contents)[mid];
                break;
            default: return 0;
        }
        if (mid_val == value) {
            if (pos) *pos = mid;
            return 1;
        } else if (mid_val < value) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    if (pos) *pos = lo;
    return 0;
}

static void *intset_realloc_contents(intset *is, size_t old_len, size_t new_len)
{
    size_t sz = intset_encoding_size(is->encoding);
    void *new_contents = kave_realloc(is->contents, new_len * sz);
    if (!new_contents) return NULL;
    if (new_len > old_len) {
        memset((char *)new_contents + old_len * sz, 0, (new_len - old_len) * sz);
    }
    return new_contents;
}

intset *intset_add(intset *is, int64_t value)
{
    if (!is) return NULL;
    uint32_t pos;
    int found = intset_search(is, value, &pos);
    if (found) return is;
    uint32_t new_enc = intset_encoding_for_value(value);
    if (new_enc > is->encoding) {
        uint32_t old_enc = is->encoding;
        is->encoding = new_enc;
        size_t old_sz = intset_encoding_size(old_enc);
        size_t new_sz = intset_encoding_size(new_enc);
        void *new_contents = kave_malloc((is->length + 1) * new_sz);
        if (!new_contents) return NULL;
        size_t old_len = is->length;
        for (uint32_t i = 0; i < old_len; i++) {
            int64_t val;
            switch (old_enc) {
                case INTSET_ENC_INT16: val = (int64_t)((int16_t *)is->contents)[i]; break;
                case INTSET_ENC_INT32: val = (int64_t)((int32_t *)is->contents)[i]; break;
                case INTSET_ENC_INT64: val = ((int64_t *)is->contents)[i]; break;
                default: val = 0;
            }
            switch (new_enc) {
                case INTSET_ENC_INT16: ((int16_t *)new_contents)[i] = (int16_t)val; break;
                case INTSET_ENC_INT32: ((int32_t *)new_contents)[i] = (int32_t)val; break;
                case INTSET_ENC_INT64: ((int64_t *)new_contents)[i] = val; break;
            }
        }
        kave_free(is->contents);
        is->contents = new_contents;
    }
    if (pos < is->length) {
        size_t sz = intset_encoding_size(is->encoding);
        memmove((char *)is->contents + (pos + 1) * sz,
                (char *)is->contents + pos * sz,
                (is->length - pos) * sz);
    }
    switch (is->encoding) {
        case INTSET_ENC_INT16: ((int16_t *)is->contents)[pos] = (int16_t)value; break;
        case INTSET_ENC_INT32: ((int32_t *)is->contents)[pos] = (int32_t)value; break;
        case INTSET_ENC_INT64: ((int64_t *)is->contents)[pos] = value; break;
    }
    is->length++;
    return is;
}

intset *intset_remove(intset *is, int64_t value)
{
    if (!is || is->length == 0) return is;
    uint32_t pos;
    if (!intset_search(is, value, &pos)) return is;
    size_t sz = intset_encoding_size(is->encoding);
    memmove((char *)is->contents + pos * sz,
            (char *)is->contents + (pos + 1) * sz,
            (is->length - pos - 1) * sz);
    is->length--;
    if (is->length == 0) {
        kave_free(is->contents);
        is->contents = NULL;
        is->encoding = INTSET_ENC_INT16;
    }
    return is;
}

int intset_find(const intset *is, int64_t value)
{
    if (!is) return 0;
    uint32_t pos;
    return intset_search(is, value, &pos);
}

int64_t intset_random(const intset *is)
{
    if (!is || is->length == 0) return 0;
    uint32_t idx = rand() % is->length;
    switch (is->encoding) {
        case INTSET_ENC_INT16: return (int64_t)((int16_t *)is->contents)[idx];
        case INTSET_ENC_INT32: return (int64_t)((int32_t *)is->contents)[idx];
        case INTSET_ENC_INT64: return ((int64_t *)is->contents)[idx];
        default: return 0;
    }
}

uint32_t intset_len(const intset *is)
{
    return is ? is->length : 0;
}

intset *intset_union(const intset *a, const intset *b)
{
    if (!a) return b ? intset_new() : NULL;
    if (!b) return intset_new();
    intset *result = intset_new();
    if (!result) return NULL;
    for (uint32_t i = 0; i < a->length; i++) {
        int64_t val;
        switch (a->encoding) {
            case INTSET_ENC_INT16: val = ((int16_t *)a->contents)[i]; break;
            case INTSET_ENC_INT32: val = ((int32_t *)a->contents)[i]; break;
            case INTSET_ENC_INT64: val = ((int64_t *)a->contents)[i]; break;
            default: val = 0;
        }
        result = intset_add(result, val);
    }
    for (uint32_t i = 0; i < b->length; i++) {
        int64_t val;
        switch (b->encoding) {
            case INTSET_ENC_INT16: val = ((int16_t *)b->contents)[i]; break;
            case INTSET_ENC_INT32: val = ((int32_t *)b->contents)[i]; break;
            case INTSET_ENC_INT64: val = ((int64_t *)b->contents)[i]; break;
            default: val = 0;
        }
        result = intset_add(result, val);
    }
    return result;
}

intset *intset_intersection(const intset *a, const intset *b)
{
    if (!a || !b) return intset_new();
    intset *result = intset_new();
    if (!result) return NULL;
    for (uint32_t i = 0; i < a->length; i++) {
        int64_t val;
        switch (a->encoding) {
            case INTSET_ENC_INT16: val = ((int16_t *)a->contents)[i]; break;
            case INTSET_ENC_INT32: val = ((int32_t *)a->contents)[i]; break;
            case INTSET_ENC_INT64: val = ((int64_t *)a->contents)[i]; break;
            default: val = 0;
        }
        if (intset_find(b, val)) {
            result = intset_add(result, val);
        }
    }
    return result;
}

intset *intset_difference(const intset *a, const intset *b)
{
    if (!a) return intset_new();
    if (!b) return intset_new();
    intset *result = intset_new();
    if (!result) return NULL;
    for (uint32_t i = 0; i < a->length; i++) {
        int64_t val;
        switch (a->encoding) {
            case INTSET_ENC_INT16: val = ((int16_t *)a->contents)[i]; break;
            case INTSET_ENC_INT32: val = ((int32_t *)a->contents)[i]; break;
            case INTSET_ENC_INT64: val = ((int64_t *)a->contents)[i]; break;
            default: val = 0;
        }
        if (!intset_find(b, val)) {
            result = intset_add(result, val);
        }
    }
    return result;
}

void intset_foreach(const intset *is, void (*fn)(int64_t value, void *user), void *user)
{
    if (!is || !fn) return;
    for (uint32_t i = 0; i < is->length; i++) {
        int64_t val;
        switch (is->encoding) {
            case INTSET_ENC_INT16: val = ((int16_t *)is->contents)[i]; break;
            case INTSET_ENC_INT32: val = ((int32_t *)is->contents)[i]; break;
            case INTSET_ENC_INT64: val = ((int64_t *)is->contents)[i]; break;
            default: val = 0;
        }
        fn(val, user);
    }
}
