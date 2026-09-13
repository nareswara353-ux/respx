#ifndef KAVE_SIGNAL_H
#define KAVE_SIGNAL_H

typedef void (*signal_callback)(int signum, void *user);

int signal_setup_handlers(void);
int signal_register_callback(signal_callback cb, void *user);
int signal_is_shutdown_requested(void);
void signal_request_shutdown(void);
void signal_reset(void);

#endif
