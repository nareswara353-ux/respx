#include "kave/signal.h"
#include <signal.h>
#include <string.h>
#include <stddef.h>

static volatile sig_atomic_t g_shutdown_requested = 0;
static signal_callback g_callback = NULL;
static void *g_callback_user = NULL;

static void signal_internal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        g_shutdown_requested = 1;
    }
    if (g_callback) {
        g_callback(signum, g_callback_user);
    }
}

int signal_setup_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_internal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) < 0) return -1;
    if (sigaction(SIGTERM, &sa, NULL) < 0) return -1;
    struct sigaction sa_pipe;
    memset(&sa_pipe, 0, sizeof(sa_pipe));
    sa_pipe.sa_handler = SIG_IGN;
    sigemptyset(&sa_pipe.sa_mask);
    sa_pipe.sa_flags = 0;
    if (sigaction(SIGPIPE, &sa_pipe, NULL) < 0) return -1;
    return 0;
}

int signal_register_callback(signal_callback cb, void *user)
{
    if (!cb) return -1;
    g_callback = cb;
    g_callback_user = user;
    return 0;
}

int signal_is_shutdown_requested(void)
{
    return g_shutdown_requested != 0;
}

void signal_request_shutdown(void)
{
    g_shutdown_requested = 1;
}

void signal_reset(void)
{
    g_shutdown_requested = 0;
    g_callback = NULL;
    g_callback_user = NULL;
}
