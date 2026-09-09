#ifndef KAVE_SOCKET_H
#define KAVE_SOCKET_H

#include <stddef.h>

typedef int socket_fd;

socket_fd socket_create(int domain, int type, int protocol);
int socket_bind(socket_fd fd, const char *address, int port);
int socket_listen(socket_fd fd, int backlog);
socket_fd socket_accept(socket_fd fd, char *client_ip, size_t ip_len, int *client_port);
int socket_set_nonblock(socket_fd fd);
int socket_set_reuseaddr(socket_fd fd);
int socket_set_keepalive(socket_fd fd);
void socket_close(socket_fd fd);
int socket_read(socket_fd fd, void *buf, size_t count);
int socket_write(socket_fd fd, const void *buf, size_t count);

#endif
