#include "kave/server.h"
#include "kave/socket.h"
#include "kave/event_loop.h"
#include "kave/hash_table.h"
#include "kave/allocator.h"
#include "kave/resp_parser.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG 1024
#define READ_BUF_SIZE 8192

struct server {
    server_config config;
    socket_fd listen_fd;
    event_loop *loop;
    ht *storage;
    int running;
};

static server *g_server = NULL;

static void server_accept_callback(int fd, void *userdata)
{
    server *srv = (server *)userdata;
    char client_ip[64];
    int client_port;
    socket_fd client = socket_accept(fd, client_ip, sizeof(client_ip), &client_port);
    if (client < 0) return;
    socket_set_nonblock(client);
    socket_set_keepalive(client);
    event_loop_add_fd(srv->loop, client, 0x01, server_accept_callback, srv);
}

static void server_signal_handler(int signum)
{
    (void)signum;
    if (g_server) {
        g_server->running = 0;
    }
}

server *server_new(const server_config *cfg)
{
    server *srv = kave_malloc(sizeof(server));
    if (!srv) return NULL;
    memcpy(&srv->config, cfg, sizeof(server_config));
    srv->listen_fd = -1;
    srv->loop = NULL;
    srv->storage = ht_new(1024);
    if (!srv->storage) {
        kave_free(srv);
        return NULL;
    }
    srv->running = 0;
    return srv;
}

int server_start(server *srv)
{
    if (!srv) return -1;
    g_server = srv;
    signal(SIGINT, server_signal_handler);
    signal(SIGTERM, server_signal_handler);
    srv->listen_fd = socket_create(AF_INET, SOCK_STREAM, 0);
    if (srv->listen_fd < 0) return -1;
    socket_set_reuseaddr(srv->listen_fd);
    if (socket_bind(srv->listen_fd, "0.0.0.0", srv->config.port) < 0) {
        socket_close(srv->listen_fd);
        return -1;
    }
    if (socket_listen(srv->listen_fd, BACKLOG) < 0) {
        socket_close(srv->listen_fd);
        return -1;
    }
    socket_set_nonblock(srv->listen_fd);
    srv->loop = event_loop_new(1024);
    if (!srv->loop) {
        socket_close(srv->listen_fd);
        return -1;
    }
    event_loop_add_fd(srv->loop, srv->listen_fd, 0x01, server_accept_callback, srv);
    srv->running = 1;
    printf("kave-server running on port %d\n", srv->config.port);
    while (srv->running) {
        event_loop_poll(srv->loop, 1000);
    }
    return 0;
}

void server_stop(server *srv)
{
    if (!srv) return;
    srv->running = 0;
    if (srv->listen_fd >= 0) {
        socket_close(srv->listen_fd);
        srv->listen_fd = -1;
    }
}

void server_free(server *srv)
{
    if (!srv) return;
    if (srv->loop) {
        event_loop_free(srv->loop);
        srv->loop = NULL;
    }
    if (srv->storage) {
        ht_free(srv->storage);
        srv->storage = NULL;
    }
    if (srv->listen_fd >= 0) {
        socket_close(srv->listen_fd);
        srv->listen_fd = -1;
    }
    kave_free(srv);
    g_server = NULL;
}

void server_graceful_shutdown(server *srv)
{
    if (!srv) return;
    srv->running = 0;
}

server_config server_get_config(const server *srv)
{
    return srv->config;
}

void server_set_config(server *srv, const server_config *cfg)
{
    if (srv && cfg) {
        memcpy(&srv->config, cfg, sizeof(server_config));
    }
}
