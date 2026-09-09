#ifndef KAVE_PERSISTENCE_H
#define KAVE_PERSISTENCE_H

#include <stddef.h>
#include <stdio.h>

typedef struct aof_ctx aof_ctx;

typedef enum aof_mode {
    AOF_OFF,
    AOF_EVERYSEC,
    AOF_ALWAYS
} aof_mode;

aof_ctx *aof_init(const char *filename, aof_mode mode);
void aof_free(aof_ctx *ctx);
int aof_append_command(aof_ctx *ctx, const char *cmd, size_t len);
int aof_replay(aof_ctx *ctx, void (*cmd_callback)(const char *cmd, size_t len, void *user), void *user);
int aof_rewrite(aof_ctx *ctx, void (*dump_callback)(void *user), void *user);
int aof_flush(aof_ctx *ctx);
void aof_close(aof_ctx *ctx);
aof_mode aof_get_mode(const aof_ctx *ctx);
void aof_set_mode(aof_ctx *ctx, aof_mode mode);

#endif
