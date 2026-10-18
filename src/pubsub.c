#include "kave/pubsub.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>

typedef struct sub_entry {
    char *name;
    void *client_data;
    pubsub_deliver_fn deliver;
    struct sub_entry *next;
} sub_entry;

struct pubsub {
    sub_entry *channels;
    sub_entry *patterns;
    size_t num_channels;
    size_t num_patterns;
};

static sub_entry *sub_entry_new(const char *name, void *client_data, pubsub_deliver_fn deliver)
{
    sub_entry *e = kave_malloc(sizeof(sub_entry));
    if (!e) return NULL;
    size_t len = strlen(name);
    e->name = kave_malloc(len + 1);
    if (!e->name) {
        kave_free(e);
        return NULL;
    }
    memcpy(e->name, name, len + 1);
    e->client_data = client_data;
    e->deliver = deliver;
    e->next = NULL;
    return e;
}

static void sub_entry_free(sub_entry *e)
{
    if (!e) return;
    if (e->name) kave_free(e->name);
    kave_free(e);
}

static int pattern_match(const char *pat, const char *str)
{
    while (*pat) {
        if (*pat == '*') {
            pat++;
            if (!*pat) return 1;
            while (*str) {
                if (pattern_match(pat, str)) return 1;
                str++;
            }
            return pattern_match(pat, str);
        } else if (*pat == '?') {
            if (!*str) return 0;
            pat++;
            str++;
        } else {
            if (*pat != *str) return 0;
            pat++;
            str++;
        }
    }
    return *str == '\0';
}

pubsub *pubsub_new(void)
{
    pubsub *ps = kave_malloc(sizeof(pubsub));
    if (!ps) return NULL;
    ps->channels = NULL;
    ps->patterns = NULL;
    ps->num_channels = 0;
    ps->num_patterns = 0;
    return ps;
}

void pubsub_free(pubsub *ps)
{
    if (!ps) return;
    sub_entry *e = ps->channels;
    while (e) {
        sub_entry *next = e->next;
        sub_entry_free(e);
        e = next;
    }
    e = ps->patterns;
    while (e) {
        sub_entry *next = e->next;
        sub_entry_free(e);
        e = next;
    }
    kave_free(ps);
}

int pubsub_subscribe(pubsub *ps, const char *channel, void *client_data, pubsub_deliver_fn deliver)
{
    if (!ps || !channel || !deliver) return -1;
    sub_entry *e = ps->channels;
    while (e) {
        if (strcmp(e->name, channel) == 0 && e->client_data == client_data) return 0;
        e = e->next;
    }
    e = sub_entry_new(channel, client_data, deliver);
    if (!e) return -1;
    e->next = ps->channels;
    ps->channels = e;
    ps->num_channels++;
    return 0;
}

int pubsub_unsubscribe(pubsub *ps, const char *channel, void *client_data)
{
    if (!ps || !channel) return -1;
    sub_entry **pp = &ps->channels;
    while (*pp) {
        sub_entry *e = *pp;
        if (strcmp(e->name, channel) == 0 && e->client_data == client_data) {
            *pp = e->next;
            sub_entry_free(e);
            ps->num_channels--;
            return 0;
        }
        pp = &e->next;
    }
    return -1;
}

int pubsub_psubscribe(pubsub *ps, const char *pattern, void *client_data, pubsub_deliver_fn deliver)
{
    if (!ps || !pattern || !deliver) return -1;
    sub_entry *e = ps->patterns;
    while (e) {
        if (strcmp(e->name, pattern) == 0 && e->client_data == client_data) return 0;
        e = e->next;
    }
    e = sub_entry_new(pattern, client_data, deliver);
    if (!e) return -1;
    e->next = ps->patterns;
    ps->patterns = e;
    ps->num_patterns++;
    return 0;
}

int pubsub_punsubscribe(pubsub *ps, const char *pattern, void *client_data)
{
    if (!ps || !pattern) return -1;
    sub_entry **pp = &ps->patterns;
    while (*pp) {
        sub_entry *e = *pp;
        if (strcmp(e->name, pattern) == 0 && e->client_data == client_data) {
            *pp = e->next;
            sub_entry_free(e);
            ps->num_patterns--;
            return 0;
        }
        pp = &e->next;
    }
    return -1;
}

int pubsub_publish(pubsub *ps, const char *channel, const char *message, size_t message_len)
{
    if (!ps || !channel) return -1;
    int delivered = 0;
    sub_entry *e = ps->channels;
    while (e) {
        if (strcmp(e->name, channel) == 0) {
            e->deliver(e->client_data, channel, message, message_len);
            delivered++;
        }
        e = e->next;
    }
    e = ps->patterns;
    while (e) {
        if (pattern_match(e->name, channel)) {
            e->deliver(e->client_data, channel, message, message_len);
            delivered++;
        }
        e = e->next;
    }
    return delivered;
}

size_t pubsub_channel_count(const pubsub *ps)
{
    return ps ? ps->num_channels : 0;
}

size_t pubsub_pattern_count(const pubsub *ps)
{
    return ps ? ps->num_patterns : 0;
}

void pubsub_client_cleanup(pubsub *ps, void *client_data)
{
    if (!ps) return;
    sub_entry **pp = &ps->channels;
    while (*pp) {
        sub_entry *e = *pp;
        if (e->client_data == client_data) {
            *pp = e->next;
            sub_entry_free(e);
            ps->num_channels--;
        } else {
            pp = &e->next;
        }
    }
    pp = &ps->patterns;
    while (*pp) {
        sub_entry *e = *pp;
        if (e->client_data == client_data) {
            *pp = e->next;
            sub_entry_free(e);
            ps->num_patterns--;
        } else {
            pp = &e->next;
        }
    }
}
