#include "kave/ziplist.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>

#define ZL_INITIAL_CAP 64
#define ZL_ENTRY_HEADER 5

ziplist *zl_new(void)
{
    ziplist *zl = kave_malloc(sizeof(ziplist));
    if (!zl) return NULL;
    zl->data = kave_malloc(ZL_INITIAL_CAP);
    if (!zl->data) {
        kave_free(zl);
        return NULL;
    }
    zl->capacity = ZL_INITIAL_CAP;
    zl->length = 0;
    zl->num_entries = 0;
    return zl;
}

void zl_free(ziplist *zl)
{
    if (!zl) return;
    if (zl->data) kave_free(zl->data);
    kave_free(zl);
}

size_t zl_length(const ziplist *zl)
{
    return zl ? zl->length : 0;
}

size_t zl_entries(const ziplist *zl)
{
    return zl ? zl->num_entries : 0;
}

static int zl_grow(ziplist *zl, size_t needed)
{
    if (zl->length + needed <= zl->capacity) return 0;
    size_t new_cap = zl->capacity ? zl->capacity : ZL_INITIAL_CAP;
    while (new_cap < zl->length + needed) new_cap *= 2;
    zl_byte *new_data = kave_realloc(zl->data, new_cap);
    if (!new_data) return -1;
    zl->data = new_data;
    zl->capacity = new_cap;
    return 0;
}

static size_t zl_entry_offset(const ziplist *zl, size_t index)
{
    size_t offset = 0;
    for (size_t i = 0; i < index && offset < zl->length; i++) {
        uint32_t len;
        memcpy(&len, zl->data + offset + 1, sizeof(uint32_t));
        offset += ZL_ENTRY_HEADER + len;
    }
    return offset;
}

static int zl_write_entry(ziplist *zl, size_t offset, const char *value, size_t value_len)
{
    zl->data[offset] = (zl_byte)0;
    uint32_t len32 = (uint32_t)value_len;
    memcpy(zl->data + offset + 1, &len32, sizeof(uint32_t));
    if (value_len > 0) {
        memcpy(zl->data + offset + ZL_ENTRY_HEADER, value, value_len);
    }
    return 0;
}

int zl_push_tail(ziplist *zl, const char *value, size_t value_len)
{
    if (!zl) return -1;
    if (zl_grow(zl, ZL_ENTRY_HEADER + value_len) < 0) return -1;
    zl_write_entry(zl, zl->length, value, value_len);
    zl->length += ZL_ENTRY_HEADER + value_len;
    zl->num_entries++;
    return 0;
}

int zl_push_head(ziplist *zl, const char *value, size_t value_len)
{
    if (!zl) return -1;
    if (zl_grow(zl, ZL_ENTRY_HEADER + value_len) < 0) return -1;
    memmove(zl->data + ZL_ENTRY_HEADER + value_len, zl->data, zl->length);
    zl_write_entry(zl, 0, value, value_len);
    zl->length += ZL_ENTRY_HEADER + value_len;
    zl->num_entries++;
    return 0;
}

int zl_get(const ziplist *zl, size_t index, char **value, size_t *value_len)
{
    if (!zl || index >= zl->num_entries) return -1;
    size_t offset = zl_entry_offset(zl, index);
    uint32_t len;
    memcpy(&len, zl->data + offset + 1, sizeof(uint32_t));
    if (value) *value = (char *)(zl->data + offset + ZL_ENTRY_HEADER);
    if (value_len) *value_len = len;
    return 0;
}

int zl_set(ziplist *zl, size_t index, const char *value, size_t value_len)
{
    if (!zl || index >= zl->num_entries) return -1;
    size_t offset = zl_entry_offset(zl, index);
    uint32_t old_len;
    memcpy(&old_len, zl->data + offset + 1, sizeof(uint32_t));
    size_t old_total = ZL_ENTRY_HEADER + old_len;
    size_t new_total = ZL_ENTRY_HEADER + value_len;
    if (new_total > old_total) {
        if (zl_grow(zl, new_total - old_total) < 0) return -1;
        memmove(zl->data + offset + new_total,
                zl->data + offset + old_total,
                zl->length - offset - old_total);
    } else if (new_total < old_total) {
        memmove(zl->data + offset + new_total,
                zl->data + offset + old_total,
                zl->length - offset - old_total);
    }
    zl->length = zl->length - old_total + new_total;
    zl_write_entry(zl, offset, value, value_len);
    return 0;
}

int zl_delete(ziplist *zl, size_t index)
{
    if (!zl || index >= zl->num_entries) return -1;
    size_t offset = zl_entry_offset(zl, index);
    uint32_t len;
    memcpy(&len, zl->data + offset + 1, sizeof(uint32_t));
    size_t total = ZL_ENTRY_HEADER + len;
    memmove(zl->data + offset, zl->data + offset + total, zl->length - offset - total);
    zl->length -= total;
    zl->num_entries--;
    return 0;
}

int zl_insert(ziplist *zl, size_t index, const char *value, size_t value_len)
{
    if (!zl || index > zl->num_entries) return -1;
    if (index == zl->num_entries) return zl_push_tail(zl, value, value_len);
    if (index == 0) return zl_push_head(zl, value, value_len);
    size_t offset = zl_entry_offset(zl, index);
    size_t add = ZL_ENTRY_HEADER + value_len;
    if (zl_grow(zl, add) < 0) return -1;
    memmove(zl->data + offset + add, zl->data + offset, zl->length - offset);
    zl_write_entry(zl, offset, value, value_len);
    zl->length += add;
    zl->num_entries++;
    return 0;
}

void zl_foreach(const ziplist *zl, void (*callback)(size_t index, const char *value, size_t value_len, void *user), void *user)
{
    if (!zl || !callback) return;
    size_t offset = 0;
    for (size_t i = 0; i < zl->num_entries; i++) {
        uint32_t len;
        memcpy(&len, zl->data + offset + 1, sizeof(uint32_t));
        callback(i, (const char *)(zl->data + offset + ZL_ENTRY_HEADER), len, user);
        offset += ZL_ENTRY_HEADER + len;
    }
}

ziplist *zl_dup(const ziplist *zl)
{
    if (!zl) return NULL;
    ziplist *copy = kave_malloc(sizeof(ziplist));
    if (!copy) return NULL;
    copy->data = kave_malloc(zl->capacity);
    if (!copy->data) {
        kave_free(copy);
        return NULL;
    }
    memcpy(copy->data, zl->data, zl->length);
    copy->length = zl->length;
    copy->capacity = zl->capacity;
    copy->num_entries = zl->num_entries;
    return copy;
}
