#include "kave/persistence.h"
#include "kave/allocator.h"
#include "kave/sds.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

#define AOF_BUF_SIZE 8192
#define AOF_REWRITE_MIN_SIZE 1048576

struct aof_ctx {
    char *filename;
    int fd;
    aof_mode mode;
    char *buffer;
    size_t buf_len;
    size_t buf_cap;
    time_t last_flush;
    size_t write_count;
};

static int aof_open_file(const char *filename, int flags)
{
    return open(filename, flags, 0644);
}

static int aof_write_all(int fd, const char *buf, size_t len)
{
    ssize_t written = 0;
    while (written < (ssize_t)len) {
        ssize_t n = write(fd, buf + written, len - written);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        written += n;
    }
    return 0;
}

static int aof_fsync(int fd)
{
    return fsync(fd);
}

aof_ctx *aof_init(const char *filename, aof_mode mode)
{
    aof_ctx *ctx = kave_malloc(sizeof(aof_ctx));
    if (!ctx) return NULL;
    ctx->filename = sds_new(filename);
    if (!ctx->filename) {
        kave_free(ctx);
        return NULL;
    }
    ctx->fd = aof_open_file(filename, O_WRONLY | O_CREAT | O_APPEND);
    if (ctx->fd < 0) {
        sds_free(ctx->filename);
        kave_free(ctx);
        return NULL;
    }
    ctx->mode = mode;
    ctx->buffer = kave_malloc(AOF_BUF_SIZE);
    if (!ctx->buffer) {
        close(ctx->fd);
        sds_free(ctx->filename);
        kave_free(ctx);
        return NULL;
    }
    ctx->buf_len = 0;
    ctx->buf_cap = AOF_BUF_SIZE;
    ctx->last_flush = time(NULL);
    ctx->write_count = 0;
    return ctx;
}

void aof_free(aof_ctx *ctx)
{
    if (!ctx) return;
    if (ctx->buffer) {
        if (ctx->buf_len > 0) {
            aof_write_all(ctx->fd, ctx->buffer, ctx->buf_len);
        }
        kave_free(ctx->buffer);
    }
    if (ctx->fd >= 0) close(ctx->fd);
    if (ctx->filename) sds_free(ctx->filename);
    kave_free(ctx);
}

int aof_append_command(aof_ctx *ctx, const char *cmd, size_t len)
{
    if (!ctx || !cmd || len == 0) return -1;
    if (ctx->mode == AOF_OFF) return 0;
    if (ctx->buf_len + len + 2 > ctx->buf_cap) {
        if (aof_flush(ctx) < 0) return -1;
    }
    memcpy(ctx->buffer + ctx->buf_len, cmd, len);
    ctx->buf_len += len;
    ctx->buffer[ctx->buf_len] = '\n';
    ctx->buf_len++;
    ctx->write_count++;
    if (ctx->mode == AOF_ALWAYS) {
        if (aof_flush(ctx) < 0) return -1;
    } else if (ctx->mode == AOF_EVERYSEC) {
        time_t now = time(NULL);
        if (now - ctx->last_flush >= 1) {
            if (aof_flush(ctx) < 0) return -1;
            ctx->last_flush = now;
        }
    }
    return 0;
}

int aof_flush(aof_ctx *ctx)
{
    if (!ctx || ctx->buf_len == 0) return 0;
    if (aof_write_all(ctx->fd, ctx->buffer, ctx->buf_len) < 0) return -1;
    if (aof_fsync(ctx->fd) < 0) return -1;
    ctx->buf_len = 0;
    return 0;
}

static int aof_replay_lines(aof_ctx *ctx, void (*cmd_callback)(const char *cmd, size_t len, void *user), void *user)
{
    if (!ctx || !cmd_callback) return -1;
    FILE *fp = fopen(ctx->filename, "r");
    if (!fp) return -1;
    char *line = NULL;
    size_t line_cap = 0;
    ssize_t read_len;
    while ((read_len = getline(&line, &line_cap, fp)) != -1) {
        if (read_len > 0 && line[read_len - 1] == '\n') {
            line[read_len - 1] = '\0';
            read_len--;
        }
        if (read_len > 0) {
            cmd_callback(line, read_len, user);
        }
    }
    free(line);
    fclose(fp);
    return 0;
}

int aof_replay(aof_ctx *ctx, void (*cmd_callback)(const char *cmd, size_t len, void *user), void *user)
{
    return aof_replay_lines(ctx, cmd_callback, user);
}

static void aof_rewrite_temp(const char *tempfile, void (*dump_callback)(void *user), void *user)
{
    int fd = aof_open_file(tempfile, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) return;
    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        close(fd);
        return;
    }
    void *old_stdout = stdout;
    stdout = fp;
    dump_callback(user);
    fflush(fp);
    stdout = old_stdout;
    fclose(fp);
}

int aof_rewrite(aof_ctx *ctx, void (*dump_callback)(void *user), void *user)
{
    if (!ctx || !dump_callback) return -1;
    struct stat st;
    if (stat(ctx->filename, &st) < 0) return -1;
    if ((size_t)st.st_size < AOF_REWRITE_MIN_SIZE) return 0;
    char tempfile[256];
    snprintf(tempfile, sizeof(tempfile), "%s.tmp", ctx->filename);
    aof_rewrite_temp(tempfile, dump_callback, user);
    if (rename(tempfile, ctx->filename) < 0) {
        unlink(tempfile);
        return -1;
    }
    if (ctx->fd >= 0) {
        close(ctx->fd);
    }
    ctx->fd = aof_open_file(ctx->filename, O_WRONLY | O_CREAT | O_APPEND);
    if (ctx->fd < 0) return -1;
    ctx->write_count = 0;
    return 0;
}

void aof_close(aof_ctx *ctx)
{
    if (!ctx) return;
    if (ctx->buffer && ctx->buf_len > 0) {
        aof_write_all(ctx->fd, ctx->buffer, ctx->buf_len);
        ctx->buf_len = 0;
    }
    if (ctx->fd >= 0) {
        aof_fsync(ctx->fd);
        close(ctx->fd);
        ctx->fd = -1;
    }
}

aof_mode aof_get_mode(const aof_ctx *ctx)
{
    return ctx ? ctx->mode : AOF_OFF;
}

void aof_set_mode(aof_ctx *ctx, aof_mode mode)
{
    if (!ctx) return;
    ctx->mode = mode;
}
