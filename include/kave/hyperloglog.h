#ifndef KAVE_HYPERLOGLOG_H
#define KAVE_HYPERLOGLOG_H

#include <stddef.h>
#include <stdint.h>

#define HLL_REGISTERS 16384
#define HLL_PRECISION 14

typedef struct hyperloglog {
    uint8_t registers[HLL_REGISTERS];
    uint64_t cached_count;
    int cached_valid;
} hyperloglog;

hyperloglog *hll_new(void);
void hll_free(hyperloglog *hll);
int hll_add(hyperloglog *hll, const char *value, size_t value_len);
uint64_t hll_count(const hyperloglog *hll);
int hll_merge(hyperloglog *dst, const hyperloglog *src);
void hll_reset(hyperloglog *hll);
int hll_serialize(const hyperloglog *hll, void **buffer, size_t *len);
hyperloglog *hll_deserialize(const void *buffer, size_t len);

#endif
