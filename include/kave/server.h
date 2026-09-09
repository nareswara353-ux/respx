#ifndef KAVE_SERVER_H
#define KAVE_SERVER_H

#include <stddef.h>
#include <stdint.h>

typedef struct server_config {
    int port;
    int max_clients;
    int timeout_seconds;
    size_t max_memory_bytes;
    int enable_aof;
    int enable_eviction;
    int eviction_policy;
} server_config;

typedef struct server server;

server *server_new(const server_config *cfg);
int server_start(server *srv);
void server_stop(server *srv);
void server_free(server *srv);
void server_graceful_shutdown(server *srv);
server_config server_get_config(const server *srv);
void server_set_config(server *srv, const server_config *cfg);

#endif
