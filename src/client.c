#include "kave/client.h"
#include "kave/allocator.h"
#include "kave/socket.h"
#include "kave/sds.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#define CLIENT_READ_BUF 8192
#define CLIENT_WRITE_INITIAL 4096

client *client_new(int fd, const char *ip, int port)
{
    client *c = kave_malloc(sizeof(client));
    if (!c) return NULL;
    c->fd = fd;
    if (ip) {
        strncpy(c->ip, ip, sizeof(c->ip) - 1);
        c->ip[sizeof(c->ip) - 1] = '\0';
    } else {
        c->ip[0] = '\0';
    }
    c->port = port;
    c->state = CLIENT_CONNECTED;
    c->parser = resp_parser_new();
    if (!c->parser) {
        kave_free(c);
        return NULL;
    }
    c->write_buffer = kave_malloc(CLIENT_WRITE_INITIAL);
    if (!c->write_buffer) {
        resp_parser_free(c->parser);
        kave_free(c);
        return NULL;
    }
    c->write_len = 0;
    c->write_cap = CLIENT_WRITE_INITIAL;
    c->write_pos = 0;
    c->bytes_read = 0;
    c->bytes_written = 0;
    c->commands_processed = 0;
    c->should_close = 0;
    return c;
}

void client_free(client *c)
{
    if (!c) return;
    if (c->parser) resp_parser_free(c->parser);
    if (c->write_buffer) kave_free(c->write_buffer);
    kave_free(c);
}

int client_read_data(client *c)
{
    if (!c || c->fd < 0) return -1;
    char buf[CLIENT_READ_BUF];
    ssize_t n = socket_read(c->fd, buf, sizeof(buf));
    if (n > 0) {
        c->bytes_read += (size_t)n;
        if (resp_parser_feed(c->parser, buf, (size_t)n) < 0) {
            c->should_close = 1;
            return -1;
        }
        c->state = CLIENT_READING;
        return (int)n;
    }
    if (n == 0) {
        c->should_close = 1;
        c->state = CLIENT_CLOSING;
        return 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
    }
    c->should_close = 1;
    return -1;
}

static int client_grow_write_buffer(client *c, size_t needed)
{
    if (needed <= c->write_cap) return 0;
    size_t new_cap = c->write_cap * 2;
    if (new_cap < needed) new_cap = needed;
    char *new_buf = kave_realloc(c->write_buffer, new_cap);
    if (!new_buf) return -1;
    c->write_buffer = new_buf;
    c->write_cap = new_cap;
    return 0;
}

int client_queue_response(client *c, const char *data, size_t len)
{
    if (!c || !data || len == 0) return -1;
    if (client_grow_write_buffer(c, c->write_len + len) < 0) return -1;
    memcpy(c->write_buffer + c->write_len, data, len);
    c->write_len += len;
    c->state = CLIENT_WRITING;
    return 0;
}

int client_write_data(client *c)
{
    if (!c || c->fd < 0) return -1;
    if (c->write_pos >= c->write_len) {
        c->write_len = 0;
        c->write_pos = 0;
        c->state = CLIENT_CONNECTED;
        return 0;
    }
    ssize_t n = socket_write(c->fd, c->write_buffer + c->write_pos, c->write_len - c->write_pos);
    if (n > 0) {
        c->write_pos += (size_t)n;
        c->bytes_written += (size_t)n;
        if (c->write_pos >= c->write_len) {
            c->write_len = 0;
            c->write_pos = 0;
            c->state = CLIENT_CONNECTED;
        }
        return (int)n;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return 0;
    }
    c->should_close = 1;
    return -1;
}

void client_reset(client *c)
{
    if (!c) return;
    c->write_len = 0;
    c->write_pos = 0;
    c->state = CLIENT_CONNECTED;
}

int client_has_pending_write(const client *c)
{
    if (!c) return 0;
    return c->write_len > c->write_pos;
}

void client_mark_close(client *c)
{
    if (!c) return;
    c->should_close = 1;
    c->state = CLIENT_CLOSING;
}
