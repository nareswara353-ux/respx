#ifndef KAVE_CLIENT_H
#define KAVE_CLIENT_H

#include <stddef.h>
#include "kave/resp_parser.h"

typedef enum client_state {
    CLIENT_CONNECTED,
    CLIENT_READING,
    CLIENT_WRITING,
    CLIENT_CLOSING,
    CLIENT_CLOSED
} client_state;

typedef struct client {
    int fd;
    char ip[64];
    int port;
    client_state state;
    resp_parser *parser;
    char *write_buffer;
    size_t write_len;
    size_t write_cap;
    size_t write_pos;
    size_t bytes_read;
    size_t bytes_written;
    size_t commands_processed;
    int should_close;
} client;

client *client_new(int fd, const char *ip, int port);
void client_free(client *c);
int client_read_data(client *c);
int client_process_input(client *c, void *server_ctx);
int client_write_data(client *c);
int client_queue_response(client *c, const char *data, size_t len);
void client_reset(client *c);
int client_has_pending_write(const client *c);
void client_mark_close(client *c);

#endif
