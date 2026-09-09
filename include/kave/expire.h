#ifndef KAVE_EXPIRE_H
#define KAVE_EXPIRE_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef struct expire_ctx expire_ctx;

expire_ctx *expire_new(void);
void expire_free(expire_ctx *ctx);
int expire_set(expire_ctx *ctx, const char *key, size_t key_len, time_t ttl_seconds);
int expire_remove(expire_ctx *ctx, const char *key, size_t key_len);
time_t expire_get_ttl(const expire_ctx *ctx, const char *key, size_t key_len);
int expire_is_expired(const expire_ctx *ctx, const char *key, size_t key_len);
void expire_check_active(expire_ctx *ctx, void (*on_expire)(const char *key, size_t key_len, void *user), void *user);
void expire_passive_cleanup(expire_ctx *ctx, size_t max_checks);

#endif
