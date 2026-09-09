#ifndef KAVE_EVENT_LOOP_H
#define KAVE_EVENT_LOOP_H

#include <stddef.h>

typedef struct event_loop event_loop;

typedef void (*event_callback)(int fd, void *userdata);

event_loop *event_loop_new(int max_events);
void event_loop_free(event_loop *loop);
int event_loop_add_fd(event_loop *loop, int fd, int events, event_callback cb, void *userdata);
int event_loop_mod_fd(event_loop *loop, int fd, int events);
int event_loop_del_fd(event_loop *loop, int fd);
int event_loop_poll(event_loop *loop, int timeout_ms);

#endif
