#ifndef KAVE_PUBSUB_H
#define KAVE_PUBSUB_H

#include <stddef.h>

typedef struct pubsub pubsub;
typedef struct pubsub_client pubsub_client;

typedef void (*pubsub_deliver_fn)(void *client_data, const char *channel, const char *message, size_t message_len);

pubsub *pubsub_new(void);
void pubsub_free(pubsub *ps);

int pubsub_subscribe(pubsub *ps, const char *channel, void *client_data, pubsub_deliver_fn deliver);
int pubsub_unsubscribe(pubsub *ps, const char *channel, void *client_data);
int pubsub_psubscribe(pubsub *ps, const char *pattern, void *client_data, pubsub_deliver_fn deliver);
int pubsub_punsubscribe(pubsub *ps, const char *pattern, void *client_data);
int pubsub_publish(pubsub *ps, const char *channel, const char *message, size_t message_len);
size_t pubsub_channel_count(const pubsub *ps);
size_t pubsub_pattern_count(const pubsub *ps);
void pubsub_client_cleanup(pubsub *ps, void *client_data);

#endif
