#include "kave/event_loop.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/event.h>
#include <sys/time.h>
#endif

#define EVENT_MAX 1024

struct event_loop {
    int epoll_fd;
    int max_events;
    event_callback *callbacks;
    void **userdata;
    int *fds;
    int count;
};

static int event_loop_find_fd(event_loop *loop, int fd)
{
    for (int i = 0; i < loop->count; i++) {
        if (loop->fds[i] == fd) return i;
    }
    return -1;
}

event_loop *event_loop_new(int max_events)
{
    event_loop *loop = kave_malloc(sizeof(event_loop));
    if (!loop) return NULL;
#ifdef __linux__
    loop->epoll_fd = epoll_create1(0);
#else
    loop->epoll_fd = kqueue();
#endif
    if (loop->epoll_fd < 0) {
        kave_free(loop);
        return NULL;
    }
    loop->max_events = max_events > 0 ? max_events : EVENT_MAX;
    loop->callbacks = kave_calloc(loop->max_events, sizeof(event_callback));
    loop->userdata = kave_calloc(loop->max_events, sizeof(void *));
    loop->fds = kave_calloc(loop->max_events, sizeof(int));
    if (!loop->callbacks || !loop->userdata || !loop->fds) {
        event_loop_free(loop);
        return NULL;
    }
    loop->count = 0;
    return loop;
}

void event_loop_free(event_loop *loop)
{
    if (!loop) return;
    if (loop->epoll_fd >= 0) close(loop->epoll_fd);
    if (loop->callbacks) kave_free(loop->callbacks);
    if (loop->userdata) kave_free(loop->userdata);
    if (loop->fds) kave_free(loop->fds);
    kave_free(loop);
}

int event_loop_add_fd(event_loop *loop, int fd, int events, event_callback cb, void *userdata)
{
    if (!loop || fd < 0 || !cb) return -1;
    int idx = event_loop_find_fd(loop, fd);
    if (idx >= 0) return -1;
    if (loop->count >= loop->max_events) return -1;
#ifdef __linux__
    struct epoll_event ev;
    ev.events = 0;
    if (events & 0x01) ev.events |= EPOLLIN;
    if (events & 0x02) ev.events |= EPOLLOUT;
    ev.data.fd = fd;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) return -1;
#else
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    if (kevent(loop->epoll_fd, &ev, 1, NULL, 0, NULL) < 0) return -1;
#endif
    loop->fds[loop->count] = fd;
    loop->callbacks[loop->count] = cb;
    loop->userdata[loop->count] = userdata;
    loop->count++;
    return 0;
}

int event_loop_mod_fd(event_loop *loop, int fd, int events)
{
    if (!loop || fd < 0) return -1;
#ifdef __linux__
    struct epoll_event ev;
    ev.events = 0;
    if (events & 0x01) ev.events |= EPOLLIN;
    if (events & 0x02) ev.events |= EPOLLOUT;
    ev.data.fd = fd;
    return epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
#else
    return 0;
#endif
}

int event_loop_del_fd(event_loop *loop, int fd)
{
    if (!loop || fd < 0) return -1;
    int idx = event_loop_find_fd(loop, fd);
    if (idx < 0) return -1;
#ifdef __linux__
    epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
#else
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(loop->epoll_fd, &ev, 1, NULL, 0, NULL);
#endif
    loop->fds[idx] = loop->fds[loop->count - 1];
    loop->callbacks[idx] = loop->callbacks[loop->count - 1];
    loop->userdata[idx] = loop->userdata[loop->count - 1];
    loop->count--;
    return 0;
}

int event_loop_poll(event_loop *loop, int timeout_ms)
{
    if (!loop) return -1;
#ifdef __linux__
    struct epoll_event events[EVENT_MAX];
    int n = epoll_wait(loop->epoll_fd, events, EVENT_MAX, timeout_ms);
    if (n < 0) return -1;
    for (int i = 0; i < n; i++) {
        int fd = events[i].data.fd;
        int idx = event_loop_find_fd(loop, fd);
        if (idx >= 0 && loop->callbacks[idx]) {
            loop->callbacks[idx](fd, loop->userdata[idx]);
        }
    }
    return n;
#else
    struct kevent events[EVENT_MAX];
    struct timespec ts;
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000;
    int n = kevent(loop->epoll_fd, NULL, 0, events, EVENT_MAX, timeout_ms >= 0 ? &ts : NULL);
    if (n < 0) return -1;
    for (int i = 0; i < n; i++) {
        int fd = events[i].ident;
        int idx = event_loop_find_fd(loop, fd);
        if (idx >= 0 && loop->callbacks[idx]) {
            loop->callbacks[idx](fd, loop->userdata[idx]);
        }
    }
    return n;
#endif
}
