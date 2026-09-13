#include "kave/server.h"
#include "kave/signal.h"
#include "kave/allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_PORT 6380
#define DEFAULT_MAX_CLIENTS 1024
#define DEFAULT_TIMEOUT 300
#define DEFAULT_MAX_MEMORY (256ULL * 1024ULL * 1024ULL)

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("  -p, --port <port>            Listen port (default: %d)\n", DEFAULT_PORT);
    printf("  -c, --max-clients <n>        Max clients (default: %d)\n", DEFAULT_MAX_CLIENTS);
    printf("  -t, --timeout <sec>          Client timeout (default: %d)\n", DEFAULT_TIMEOUT);
    printf("  -m, --max-memory <bytes>     Max memory (default: %llu)\n", (unsigned long long)DEFAULT_MAX_MEMORY);
    printf("  -a, --aof                    Enable AOF persistence\n");
    printf("  -e, --eviction <policy>      Eviction: lru|lfu|none\n");
    printf("  -h, --help                   Show this help\n");
}

static int parse_eviction_policy(const char *s)
{
    if (strcasecmp(s, "lru") == 0) return 1;
    if (strcasecmp(s, "lfu") == 0) return 2;
    return 0;
}

int main(int argc, char **argv)
{
    server_config cfg;
    cfg.port = DEFAULT_PORT;
    cfg.max_clients = DEFAULT_MAX_CLIENTS;
    cfg.timeout_seconds = DEFAULT_TIMEOUT;
    cfg.max_memory_bytes = DEFAULT_MAX_MEMORY;
    cfg.enable_aof = 0;
    cfg.enable_eviction = 0;
    cfg.eviction_policy = 0;
    static struct {
        const char *long_opt;
        char short_opt;
        int has_arg;
    } options[] = {
        {"port", 'p', 1},
        {"max-clients", 'c', 1},
        {"timeout", 't', 1},
        {"max-memory", 'm', 1},
        {"aof", 'a', 0},
        {"eviction", 'e', 1},
        {"help", 'h', 0},
        {NULL, 0, 0}
    };
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        int matched = -1;
        if (arg[0] == '-' && arg[1] == '-') {
            for (int j = 0; options[j].long_opt; j++) {
                if (strcmp(arg + 2, options[j].long_opt) == 0) {
                    matched = j;
                    break;
                }
            }
        } else if (arg[0] == '-' && arg[1] != '\0') {
            for (int j = 0; options[j].long_opt; j++) {
                if (arg[1] == options[j].short_opt) {
                    matched = j;
                    break;
                }
            }
        }
        if (matched < 0) {
            fprintf(stderr, "Unknown option: %s\n", arg);
            print_usage(argv[0]);
            return 1;
        }
        if (strcmp(options[matched].long_opt, "help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (options[matched].has_arg) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Option %s requires argument\n", arg);
                return 1;
            }
            const char *val = argv[++i];
            switch (options[matched].short_opt) {
                case 'p': cfg.port = atoi(val); break;
                case 'c': cfg.max_clients = atoi(val); break;
                case 't': cfg.timeout_seconds = atoi(val); break;
                case 'm': cfg.max_memory_bytes = strtoull(val, NULL, 10); break;
                case 'e':
                    cfg.eviction_policy = parse_eviction_policy(val);
                    cfg.enable_eviction = (cfg.eviction_policy != 0);
                    break;
                default: break;
            }
        } else {
            if (options[matched].short_opt == 'a') {
                cfg.enable_aof = 1;
            }
        }
    }
    if (cfg.port <= 0 || cfg.port > 65535) {
        fprintf(stderr, "Invalid port: %d\n", cfg.port);
        return 1;
    }
    if (cfg.max_clients <= 0) {
        fprintf(stderr, "Invalid max-clients: %d\n", cfg.max_clients);
        return 1;
    }
    if (signal_setup_handlers() < 0) {
        fprintf(stderr, "Failed to setup signal handlers\n");
        return 1;
    }
    server *srv = server_new(&cfg);
    if (!srv) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }
    int result = server_start(srv);
    server_free(srv);
    kave_alloc_stats stats = kave_get_global_stats();
    if (stats.active_bytes > 0 || stats.active_blocks > 0) {
        fprintf(stderr, "Warning: %zu bytes still allocated in %zu blocks\n",
                stats.active_bytes, stats.active_blocks);
    }
    return result == 0 ? 0 : 1;
}
