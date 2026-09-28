#include "kave/hyperloglog.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define HLL_ALPHA 0.7213
#define HLL_SERIAL_MAGIC 0x484C4C31
#define HLL_SERIAL_SIZE (HLL_REGISTERS + 8)

static uint64_t hll_hash(const char *value, size_t len)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)value[i];
        h *= 0x100000001b3ULL;
    }
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

static int hll_leading_zeros(uint64_t value, int max_bits)
{
    int count = 0;
    uint64_t bit = 1ULL << (max_bits - 1);
    while (count < max_bits && (value & bit) == 0) {
        count++;
        bit >>= 1;
    }
    return count;
}

hyperloglog *hll_new(void)
{
    hyperloglog *hll = kave_malloc(sizeof(hyperloglog));
    if (!hll) return NULL;
    memset(hll->registers, 0, sizeof(hll->registers));
    hll->cached_count = 0;
    hll->cached_valid = 0;
    return hll;
}

void hll_free(hyperloglog *hll)
{
    if (!hll) return;
    kave_free(hll);
}

void hll_reset(hyperloglog *hll)
{
    if (!hll) return;
    memset(hll->registers, 0, sizeof(hll->registers));
    hll->cached_count = 0;
    hll->cached_valid = 0;
}

int hll_add(hyperloglog *hll, const char *value, size_t value_len)
{
    if (!hll || !value) return -1;
    uint64_t hash = hll_hash(value, value_len);
    uint64_t index = hash >> (64 - HLL_PRECISION);
    uint64_t rest = hash << HLL_PRECISION;
    int lz = hll_leading_zeros(rest, 64 - HLL_PRECISION) + 1;
    if (lz > 255) lz = 255;
    if (hll->registers[index] < (uint8_t)lz) {
        hll->registers[index] = (uint8_t)lz;
        hll->cached_valid = 0;
    }
    return 0;
}

static double hll_alpha(size_t m)
{
    if (m == 16) return 0.673;
    if (m == 32) return 0.697;
    if (m == 64) return 0.709;
    return HLL_ALPHA;
}

uint64_t hll_count(const hyperloglog *hll)
{
    if (!hll) return 0;
    if (hll->cached_valid) return hll->cached_count;
    double m = (double)HLL_REGISTERS;
    double sum = 0.0;
    int zeros = 0;
    for (int i = 0; i < HLL_REGISTERS; i++) {
        uint8_t r = hll->registers[i];
        if (r == 0) zeros++;
        sum += 1.0 / (double)(1ULL << r);
    }
    double estimate = hll_alpha((size_t)m) * m * m / sum;
    if (estimate <= 2.5 * m && zeros > 0) {
        estimate = m * log(m / (double)zeros);
    } else if (estimate > (1.0 / 30.0) * 4294967296.0) {
        double ratio = estimate / 4294967296.0;
        estimate = -4294967296.0 * log(1.0 - ratio);
    }
    ((hyperloglog *)hll)->cached_count = (uint64_t)(estimate + 0.5);
    ((hyperloglog *)hll)->cached_valid = 1;
    return hll->cached_count;
}

int hll_merge(hyperloglog *dst, const hyperloglog *src)
{
    if (!dst || !src) return -1;
    for (int i = 0; i < HLL_REGISTERS; i++) {
        if (src->registers[i] > dst->registers[i]) {
            dst->registers[i] = src->registers[i];
        }
    }
    dst->cached_valid = 0;
    return 0;
}

int hll_serialize(const hyperloglog *hll, void **buffer, size_t *len)
{
    if (!hll || !buffer || !len) return -1;
    uint8_t *out = kave_malloc(HLL_SERIAL_SIZE);
    if (!out) return -1;
    uint32_t magic = HLL_SERIAL_MAGIC;
    memcpy(out, &magic, sizeof(uint32_t));
    memcpy(out + 4, hll->registers, HLL_REGISTERS);
    uint32_t checksum = 0;
    for (int i = 0; i < HLL_REGISTERS; i++) {
        checksum = (checksum * 31) + hll->registers[i];
    }
    memcpy(out + 4 + HLL_REGISTERS, &checksum, sizeof(uint32_t));
    *buffer = out;
    *len = HLL_SERIAL_SIZE;
    return 0;
}

hyperloglog *hll_deserialize(const void *buffer, size_t len)
{
    if (!buffer || len < HLL_SERIAL_SIZE) return NULL;
    const uint8_t *in = (const uint8_t *)buffer;
    uint32_t magic;
    memcpy(&magic, in, sizeof(uint32_t));
    if (magic != HLL_SERIAL_MAGIC) return NULL;
    hyperloglog *hll = kave_malloc(sizeof(hyperloglog));
    if (!hll) return NULL;
    memcpy(hll->registers, in + 4, HLL_REGISTERS);
    uint32_t checksum = 0;
    for (int i = 0; i < HLL_REGISTERS; i++) {
        checksum = (checksum * 31) + hll->registers[i];
    }
    uint32_t stored;
    memcpy(&stored, in + 4 + HLL_REGISTERS, sizeof(uint32_t));
    if (stored != checksum) {
        kave_free(hll);
        return NULL;
    }
    hll->cached_count = 0;
    hll->cached_valid = 0;
    return hll;
}
