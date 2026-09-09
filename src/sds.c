#include "kave/sds.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#define SDS_HDR(s) ((struct sds_hdr *)((char *)(s) - sizeof(struct sds_hdr)))
#define SDS_HDR_LEN 8

struct sds_hdr {
    size_t len;
    size_t alloc;
    char buf[];
};

sds sds_new_len(const char *init, size_t init_len)
{
    struct sds_hdr *h;
    size_t alloc = init_len + 1;
    if (alloc < SDS_HDR_LEN) alloc = SDS_HDR_LEN;
    h = malloc(sizeof(struct sds_hdr) + alloc);
    if (!h) return NULL;
    h->len = init_len;
    h->alloc = alloc;
    if (init && init_len) {
        memcpy(h->buf, init, init_len);
    }
    h->buf[init_len] = '\0';
    return h->buf;
}

sds sds_new(const char *init)
{
    size_t init_len = init ? strlen(init) : 0;
    return sds_new_len(init, init_len);
}

void sds_free(sds s)
{
    if (!s) return;
    free(SDS_HDR(s));
}

size_t sds_len(const sds s)
{
    if (!s) return 0;
    return SDS_HDR(s)->len;
}

size_t sds_avail(const sds s)
{
    if (!s) return 0;
    struct sds_hdr *h = SDS_HDR(s);
    return h->alloc - h->len - 1;
}

char *sds_data(const sds s)
{
    return (char *)s;
}

static sds sds_make_room(sds s, size_t add_len)
{
    struct sds_hdr *h = SDS_HDR(s);
    size_t new_len = h->len + add_len;
    if (new_len + 1 <= h->alloc) return s;
    size_t new_alloc = new_len + 1;
    if (new_alloc < SDS_HDR_LEN) new_alloc = SDS_HDR_LEN;
    h = realloc(h, sizeof(struct sds_hdr) + new_alloc);
    if (!h) return NULL;
    h->alloc = new_alloc;
    return h->buf;
}

sds sds_append(sds s, const char *add)
{
    if (!add) return s;
    return sds_append_len(s, add, strlen(add));
}

sds sds_append_len(sds s, const char *add, size_t len)
{
    if (!s || !add || len == 0) return s;
    sds new_s = sds_make_room(s, len);
    if (!new_s) return NULL;
    struct sds_hdr *h = SDS_HDR(new_s);
    memcpy(h->buf + h->len, add, len);
    h->len += len;
    h->buf[h->len] = '\0';
    return new_s;
}

sds sds_cat_printf(sds s, const char *fmt, ...)
{
    if (!fmt) return s;
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) return s;
    size_t add_len = (size_t)needed;
    sds new_s = sds_make_room(s, add_len);
    if (!new_s) return NULL;
    struct sds_hdr *h = SDS_HDR(new_s);
    va_start(ap, fmt);
    vsnprintf(h->buf + h->len, h->alloc - h->len, fmt, ap);
    va_end(ap);
    h->len += add_len;
    return new_s;
}

sds sds_trim(sds s, const char *cset)
{
    if (!s || !cset) return s;
    struct sds_hdr *h = SDS_HDR(s);
    size_t start = 0;
    size_t end = h->len;
    while (start < end && strchr(cset, h->buf[start])) start++;
    while (end > start && strchr(cset, h->buf[end - 1])) end--;
    if (start != 0 || end != h->len) {
        memmove(h->buf, h->buf + start, end - start);
        h->len = end - start;
        h->buf[h->len] = '\0';
    }
    return s;
}

sds sds_dup(const sds s)
{
    if (!s) return NULL;
    struct sds_hdr *h = SDS_HDR(s);
    return sds_new_len(h->buf, h->len);
}

int sds_cmp(const sds a, const sds b)
{
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    struct sds_hdr *ha = SDS_HDR(a);
    struct sds_hdr *hb = SDS_HDR(b);
    size_t min_len = ha->len < hb->len ? ha->len : hb->len;
    int r = memcmp(ha->buf, hb->buf, min_len);
    if (r != 0) return r;
    if (ha->len < hb->len) return -1;
    if (ha->len > hb->len) return 1;
    return 0;
}

void sds_clear(sds s)
{
    if (!s) return;
    struct sds_hdr *h = SDS_HDR(s);
    h->len = 0;
    h->buf[0] = '\0';
}
