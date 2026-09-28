#include "kave/bitmap.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>

#define BITMAP_MIN_BYTES 16

static size_t bitmap_bytes_for(size_t bits)
{
    return (bits + 7) / 8;
}

bitmap *bitmap_new(size_t initial_bits)
{
    bitmap *bm = kave_malloc(sizeof(bitmap));
    if (!bm) return NULL;
    size_t bytes = bitmap_bytes_for(initial_bits);
    if (bytes < BITMAP_MIN_BYTES) bytes = BITMAP_MIN_BYTES;
    bm->data = kave_calloc(bytes, 1);
    if (!bm->data) {
        kave_free(bm);
        return NULL;
    }
    bm->capacity_bits = bytes * 8;
    bm->max_bit = 0;
    bm->num_set = 0;
    return bm;
}

void bitmap_free(bitmap *bm)
{
    if (!bm) return;
    if (bm->data) kave_free(bm->data);
    kave_free(bm);
}

void bitmap_clear(bitmap *bm)
{
    if (!bm) return;
    memset(bm->data, 0, bitmap_bytes_for(bm->capacity_bits));
    bm->max_bit = 0;
    bm->num_set = 0;
}

static int bitmap_grow(bitmap *bm, size_t bit)
{
    if (bit < bm->capacity_bits) return 0;
    size_t new_bits = bm->capacity_bits;
    while (new_bits <= bit) new_bits *= 2;
    size_t old_bytes = bitmap_bytes_for(bm->capacity_bits);
    size_t new_bytes = bitmap_bytes_for(new_bits);
    uint8_t *new_data = kave_realloc(bm->data, new_bytes);
    if (!new_data) return -1;
    memset(new_data + old_bytes, 0, new_bytes - old_bytes);
    bm->data = new_data;
    bm->capacity_bits = new_bits;
    return 0;
}

int bitmap_set(bitmap *bm, size_t bit, int value)
{
    if (!bm) return -1;
    if (bitmap_grow(bm, bit) < 0) return -1;
    size_t byte_idx = bit / 8;
    uint8_t mask = (uint8_t)(1u << (bit % 8));
    uint8_t old = bm->data[byte_idx];
    if (value) {
        if (!(old & mask)) {
            bm->data[byte_idx] = old | mask;
            bm->num_set++;
        }
    } else {
        if (old & mask) {
            bm->data[byte_idx] = old & (uint8_t)~mask;
            bm->num_set--;
        }
    }
    if (bit > bm->max_bit) bm->max_bit = bit;
    return 0;
}

int bitmap_get(const bitmap *bm, size_t bit)
{
    if (!bm || bit >= bm->capacity_bits) return 0;
    return (bm->data[bit / 8] >> (bit % 8)) & 1;
}

size_t bitmap_count(const bitmap *bm)
{
    return bm ? bm->num_set : 0;
}

size_t bitmap_max_bit(const bitmap *bm)
{
    return bm ? bm->max_bit : 0;
}

static size_t bitmap_op_bytes(const bitmap *a, const bitmap *b)
{
    size_t bytes_a = bitmap_bytes_for(a->capacity_bits);
    size_t bytes_b = bitmap_bytes_for(b->capacity_bits);
    return bytes_a > bytes_b ? bytes_a : bytes_b;
}

int bitmap_and(bitmap *dst, const bitmap *a, const bitmap *b)
{
    if (!dst || !a || !b) return -1;
    size_t bytes = bitmap_op_bytes(a, b);
    if (bitmap_grow(dst, bytes * 8) < 0) return -1;
    size_t bytes_a = bitmap_bytes_for(a->capacity_bits);
    size_t bytes_b = bitmap_bytes_for(b->capacity_bits);
    size_t count = 0;
    for (size_t i = 0; i < bytes; i++) {
        uint8_t va = i < bytes_a ? a->data[i] : 0;
        uint8_t vb = i < bytes_b ? b->data[i] : 0;
        uint8_t r = va & vb;
        dst->data[i] = r;
        for (int k = 0; k < 8; k++) if (r & (1u << k)) count++;
    }
    dst->num_set = count;
    return 0;
}

int bitmap_or(bitmap *dst, const bitmap *a, const bitmap *b)
{
    if (!dst || !a || !b) return -1;
    size_t bytes = bitmap_op_bytes(a, b);
    if (bitmap_grow(dst, bytes * 8) < 0) return -1;
    size_t bytes_a = bitmap_bytes_for(a->capacity_bits);
    size_t bytes_b = bitmap_bytes_for(b->capacity_bits);
    size_t count = 0;
    for (size_t i = 0; i < bytes; i++) {
        uint8_t va = i < bytes_a ? a->data[i] : 0;
        uint8_t vb = i < bytes_b ? b->data[i] : 0;
        uint8_t r = va | vb;
        dst->data[i] = r;
        for (int k = 0; k < 8; k++) if (r & (1u << k)) count++;
    }
    dst->num_set = count;
    return 0;
}

int bitmap_xor(bitmap *dst, const bitmap *a, const bitmap *b)
{
    if (!dst || !a || !b) return -1;
    size_t bytes = bitmap_op_bytes(a, b);
    if (bitmap_grow(dst, bytes * 8) < 0) return -1;
    size_t bytes_a = bitmap_bytes_for(a->capacity_bits);
    size_t bytes_b = bitmap_bytes_for(b->capacity_bits);
    size_t count = 0;
    for (size_t i = 0; i < bytes; i++) {
        uint8_t va = i < bytes_a ? a->data[i] : 0;
        uint8_t vb = i < bytes_b ? b->data[i] : 0;
        uint8_t r = va ^ vb;
        dst->data[i] = r;
        for (int k = 0; k < 8; k++) if (r & (1u << k)) count++;
    }
    dst->num_set = count;
    return 0;
}

int bitmap_not(bitmap *dst, const bitmap *src)
{
    if (!dst || !src) return -1;
    if (bitmap_grow(dst, src->capacity_bits) < 0) return -1;
    size_t bytes = bitmap_bytes_for(src->capacity_bits);
    size_t count = 0;
    for (size_t i = 0; i < bytes; i++) {
        uint8_t r = (uint8_t)~src->data[i];
        dst->data[i] = r;
        for (int k = 0; k < 8; k++) if (r & (1u << k)) count++;
    }
    dst->num_set = count;
    return 0;
}
