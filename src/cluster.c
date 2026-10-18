#include "kave/cluster.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>

struct cluster_node {
    char id[CLUSTER_NODE_ID_LEN + 1];
    char *host;
    int port;
};

struct cluster {
    cluster_node **nodes;
    size_t num_nodes;
    size_t capacity;
    cluster_node **slot_owner;
    size_t assigned;
};

uint16_t cluster_crc16(const unsigned char *buf, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint16_t cluster_key_slot(const char *key, size_t key_len)
{
    if (!key || key_len == 0) return 0;
    const char *hash_start = NULL;
    for (size_t i = 0; i < key_len; i++) {
        if (key[i] == '{') {
            hash_start = key + i + 1;
            break;
        }
    }
    if (hash_start) {
        size_t start_off = (size_t)(hash_start - key);
        const char *hash_end = NULL;
        for (size_t i = start_off; i < key_len; i++) {
            if (key[i] == '}') {
                hash_end = key + i;
                break;
            }
        }
        if (hash_end && hash_end > hash_start) {
            return cluster_crc16((const unsigned char *)hash_start, (size_t)(hash_end - hash_start)) % CLUSTER_SLOTS;
        }
    }
    return cluster_crc16((const unsigned char *)key, key_len) % CLUSTER_SLOTS;
}

cluster *cluster_new(void)
{
    cluster *c = kave_malloc(sizeof(cluster));
    if (!c) return NULL;
    c->nodes = NULL;
    c->num_nodes = 0;
    c->capacity = 0;
    c->slot_owner = kave_calloc(CLUSTER_SLOTS, sizeof(cluster_node *));
    if (!c->slot_owner) {
        kave_free(c);
        return NULL;
    }
    c->assigned = 0;
    return c;
}

void cluster_free(cluster *c)
{
    if (!c) return;
    for (size_t i = 0; i < c->num_nodes; i++) {
        cluster_node *n = c->nodes[i];
        if (!n) continue;
        if (n->host) kave_free(n->host);
        kave_free(n);
    }
    if (c->nodes) kave_free(c->nodes);
    if (c->slot_owner) kave_free(c->slot_owner);
    kave_free(c);
}

cluster_node *cluster_find_node(cluster *c, const char *id)
{
    if (!c || !id) return NULL;
    for (size_t i = 0; i < c->num_nodes; i++) {
        if (c->nodes[i] && strcmp(c->nodes[i]->id, id) == 0) return c->nodes[i];
    }
    return NULL;
}

int cluster_add_node(cluster *c, const char *id, const char *host, int port)
{
    if (!c || !id || !host) return -1;
    if (cluster_find_node(c, id)) return -1;
    if (c->num_nodes >= c->capacity) {
        size_t new_cap = c->capacity ? c->capacity * 2 : 4;
        cluster_node **new_nodes = kave_realloc(c->nodes, new_cap * sizeof(cluster_node *));
        if (!new_nodes) return -1;
        c->nodes = new_nodes;
        c->capacity = new_cap;
    }
    cluster_node *n = kave_malloc(sizeof(cluster_node));
    if (!n) return -1;
    size_t id_len = strlen(id);
    if (id_len > CLUSTER_NODE_ID_LEN) id_len = CLUSTER_NODE_ID_LEN;
    memcpy(n->id, id, id_len);
    n->id[id_len] = '\0';
    size_t hlen = strlen(host);
    n->host = kave_malloc(hlen + 1);
    if (!n->host) {
        kave_free(n);
        return -1;
    }
    memcpy(n->host, host, hlen + 1);
    n->port = port;
    c->nodes[c->num_nodes++] = n;
    return 0;
}

int cluster_remove_node(cluster *c, const char *id)
{
    if (!c || !id) return -1;
    for (size_t i = 0; i < c->num_nodes; i++) {
        cluster_node *n = c->nodes[i];
        if (!n) continue;
        if (strcmp(n->id, id) == 0) {
            for (int s = 0; s < CLUSTER_SLOTS; s++) {
                if (c->slot_owner[s] == n) {
                    c->slot_owner[s] = NULL;
                    c->assigned--;
                }
            }
            if (n->host) kave_free(n->host);
            kave_free(n);
            c->nodes[i] = NULL;
            return 0;
        }
    }
    return -1;
}

int cluster_assign_slot(cluster *c, const char *node_id, uint16_t slot)
{
    if (!c || !node_id || slot >= CLUSTER_SLOTS) return -1;
    cluster_node *n = cluster_find_node(c, node_id);
    if (!n) return -1;
    if (c->slot_owner[slot] == NULL) c->assigned++;
    c->slot_owner[slot] = n;
    return 0;
}

cluster_node *cluster_get_node_for_slot(const cluster *c, uint16_t slot)
{
    if (!c || slot >= CLUSTER_SLOTS) return NULL;
    return c->slot_owner[slot];
}

cluster_node *cluster_get_node_for_key(const cluster *c, const char *key, size_t key_len)
{
    if (!c) return NULL;
    uint16_t slot = cluster_key_slot(key, key_len);
    return c->slot_owner[slot];
}

const char *cluster_node_id(const cluster_node *n)
{
    return n ? n->id : NULL;
}

const char *cluster_node_host(const cluster_node *n)
{
    return n ? n->host : NULL;
}

int cluster_node_port(const cluster_node *n)
{
    return n ? n->port : 0;
}

size_t cluster_node_count(const cluster *c)
{
    return c ? c->num_nodes : 0;
}

size_t cluster_assigned_slots(const cluster *c)
{
    return c ? c->assigned : 0;
}
