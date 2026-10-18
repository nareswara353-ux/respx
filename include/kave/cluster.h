#ifndef KAVE_CLUSTER_H
#define KAVE_CLUSTER_H

#include <stddef.h>
#include <stdint.h>

#define CLUSTER_SLOTS 16384
#define CLUSTER_NODE_ID_LEN 40

typedef struct cluster cluster;
typedef struct cluster_node cluster_node;

cluster *cluster_new(void);
void cluster_free(cluster *c);
uint16_t cluster_crc16(const unsigned char *buf, size_t len);
uint16_t cluster_key_slot(const char *key, size_t key_len);
int cluster_add_node(cluster *c, const char *id, const char *host, int port);
int cluster_remove_node(cluster *c, const char *id);
cluster_node *cluster_find_node(cluster *c, const char *id);
int cluster_assign_slot(cluster *c, const char *node_id, uint16_t slot);
cluster_node *cluster_get_node_for_slot(const cluster *c, uint16_t slot);
cluster_node *cluster_get_node_for_key(const cluster *c, const char *key, size_t key_len);
const char *cluster_node_id(const cluster_node *n);
const char *cluster_node_host(const cluster_node *n);
int cluster_node_port(const cluster_node *n);
size_t cluster_node_count(const cluster *c);
size_t cluster_assigned_slots(const cluster *c);

#endif
