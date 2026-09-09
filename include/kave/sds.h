#ifndef KAVE_SDS_H
#define KAVE_SDS_H

#include <stddef.h>

typedef char *sds;

sds sds_new(const char *init);
void sds_free(sds s);
size_t sds_len(const sds s);
size_t sds_avail(const sds s);
char *sds_data(const sds s);
sds sds_append(sds s, const char *add);
sds sds_append_len(sds s, const char *add, size_t len);
sds sds_cat_printf(sds s, const char *fmt, ...);
sds sds_trim(sds s, const char *cset);
sds sds_dup(const sds s);
int sds_cmp(const sds a, const sds b);
void sds_clear(sds s);

#endif
