#ifndef KAVE_BITMAP_H
#define KAVE_BITMAP_H

#include <stddef.h>
#include <stdint.h>

typedef struct bitmap {
    uint8_t *data;
    size_t capacity_bits;
    size_t max_bit;
    size_t num_set;
} bitmap;

bitmap *bitmap_new(size_t initial_bits);
void bitmap_free(bitmap *bm);
int bitmap_set(bitmap *bm, size_t bit, int value);
int bitmap_get(const bitmap *bm, size_t bit);
size_t bitmap_count(const bitmap *bm);
size_t bitmap_max_bit(const bitmap *bm);
int bitmap_and(bitmap *dst, const bitmap *a, const bitmap *b);
int bitmap_or(bitmap *dst, const bitmap *a, const bitmap *b);
int bitmap_xor(bitmap *dst, const bitmap *a, const bitmap *b);
int bitmap_not(bitmap *dst, const bitmap *src);
void bitmap_clear(bitmap *bm);

#endif
