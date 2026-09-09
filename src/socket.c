#include "kave/socket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

socket_fd socket_create(int domain, int type, int protocol)
{
    return socket(domain, type, protocol);
}

int socket_bind(socket_fd fd, const char *address, int port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (address && strcmp(address, "0.0.0.0") != 0) {
        inet_pton(AF_INET, address, &addr.sin_addr);
    } else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    return bind(fd, (struct sockaddr *)&addr, sizeof(addr));
}

int socket_listen(socket_fd fd, int backlog)
{
    return listen(fd, backlog);
}

socket_fd socket_accept(socket_fd fd, char *client_ip, size_t ip_len, int *client_port)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    socket_fd client = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client >= 0) {
        if (client_ip) {
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, ip_len);
        }
        if (client_port) {
            *client_port = ntohs(client_addr.sin_port);
        }
    }
    return client;
}

int socket_set_nonblock(socket_fd fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int socket_set_reuseaddr(socket_fd fd)
{
    int reuse = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
}

int socket_set_keepalive(socket_fd fd)
{
    int keepalive = 1;
    return setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
}

void socket_close(socket_fd fd)
{
    if (fd >= 0) close(fd);
}

int socket_read(socket_fd fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

int socket_write(socket_fd fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}
