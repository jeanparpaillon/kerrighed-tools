/* Cluster configuration related interface functions.
 * @file libhotplug.c
 *
 * Copyright (C) 2006-2007, Kerlabs
 *
 * Authors:
 *    Pascal Gallard, <pascal.gallard@kerlabs.com>
 *    Jean Parpaillon, <jean.parpaillon@kerlabs.com>
 */

#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include <types.h>
#include <krgnodemask.h>
#include <kerrighed_tools.h>
#include <hotplug.h>

int kerrighed_max_nodes = -1;
int kerrighed_max_clusters = -1;

const char* krg_status_str(int s)
{
	static char *str[4] = { "invalid", "possible", "present", "online" };

	if (s <= 0 || s > HOTPLUG_NODE_ONLINE)
		s = HOTPLUG_NODE_INVALID;

	return str[s];
}

struct krg_nodes* krg_nodes_create(void)
{
	struct krg_nodes* item;

	item = malloc(sizeof(struct krg_nodes));
	if (!item) {
		return NULL;
	}

	item->nodes = malloc(kerrighed_max_nodes);
	if (!item->nodes) {
		free(item);
		return NULL;
	}

	return item;
}

void krg_nodes_destroy(struct krg_nodes* item)
{
	if (item) {
		free(item->nodes);
		free(item);
	}
}

int krg_nodes_num(struct krg_nodes* nodes, enum krg_status s)
{
	int ret = 0;
	int bcl;

	for (bcl = 0; bcl < kerrighed_max_nodes; bcl++) {
		if (nodes->nodes[bcl] == s)
			ret++;
	}

	return ret;
}

int krg_nodes_num_possible(struct krg_nodes* nodes)
{
	return krg_nodes_num(nodes, HOTPLUG_NODE_POSSIBLE);
}

int krg_nodes_num_present(struct krg_nodes* nodes)
{
	return krg_nodes_num(nodes, HOTPLUG_NODE_PRESENT);
}

int krg_nodes_num_online(struct krg_nodes* nodes)
{
	return krg_nodes_num(nodes, HOTPLUG_NODE_ONLINE);
}

int krg_nodes_is(struct krg_nodes* nodes, int n, enum krg_status s)
{
	int ret = 0;

	if (n >= 0 && n < kerrighed_max_nodes) {
		if (nodes->nodes[n] == s)
			ret = 1;
	} else {
		ret = -1;
	}
	
	return ret;
}

int krg_nodes_is_possible(struct krg_nodes* nodes, int n)
{
	return krg_nodes_is(nodes, n, HOTPLUG_NODE_POSSIBLE);
}

int krg_nodes_is_present(struct krg_nodes* nodes, int n)
{
	return krg_nodes_is(nodes, n, HOTPLUG_NODE_PRESENT);
}

int krg_nodes_is_online(struct krg_nodes* nodes, int n)
{
	return krg_nodes_is(nodes, n, HOTPLUG_NODE_ONLINE);
}

int krg_nodes_next(struct krg_nodes* nodes, int node, enum krg_status s)
{
	int bcl;

	bcl = node + 1;
	while (bcl < kerrighed_max_nodes) {
		if (nodes->nodes[bcl] == s)
			return bcl;
		bcl++;
	}
	return -1;
}

int krg_nodes_next_online(struct krg_nodes* nodes, int node)
{
	return krg_nodes_next(nodes, node, HOTPLUG_NODE_ONLINE);
}

int krg_nodes_next_possible(struct krg_nodes* nodes, int node)
{
	return krg_nodes_next(nodes, node, HOTPLUG_NODE_POSSIBLE);
}

int krg_nodes_next_present(struct krg_nodes* nodes, int node)
{
	return krg_nodes_next(nodes, node, HOTPLUG_NODE_PRESENT);
}

struct krg_node_set* krg_nodes_get(struct krg_nodes* nodes, enum krg_status s)
{
	struct krg_node_set* r = NULL;
	int bcl;

	r = krg_node_set_create();
	if (r) {
		for (bcl = 0; bcl < kerrighed_max_nodes; bcl++) {
			if (nodes->nodes[bcl] == s)
				krg_node_set_add(r, bcl);
		}
	}

	return r;
}

struct krg_node_set* krg_nodes_get_online(struct krg_nodes* nodes)
{
	return krg_nodes_get(nodes, HOTPLUG_NODE_ONLINE);
}

struct krg_node_set* krg_nodes_get_possible(struct krg_nodes* nodes)
{
	return krg_nodes_get(nodes, HOTPLUG_NODE_POSSIBLE);
}

struct krg_node_set* krg_nodes_get_present(struct krg_nodes* nodes)
{
	return krg_nodes_get(nodes, HOTPLUG_NODE_PRESENT);
}

enum krg_status krg_nodes_getnode(struct krg_nodes* nodes, int n)
{
	if (n >= 0 && n < kerrighed_max_nodes) {
		return nodes->nodes[n];
	} else
		return HOTPLUG_NODE_INVALID;
}

int krg_nodes_nextnode(struct krg_nodes* nodes, int node)
{
	int bcl;

	bcl = node + 1;
	while (bcl < kerrighed_max_nodes) {
		if (nodes->nodes[bcl] > HOTPLUG_NODE_INVALID)
			return bcl;
		bcl++;
	}
	return -1;
}

struct krg_clusters* krg_clusters_create(void)
{
	struct krg_clusters* item;

	item = malloc(sizeof(struct krg_clusters));
	if (!item) {
		return NULL;
	}

	item->clusters = malloc(kerrighed_max_clusters);
	if (!item->clusters) {
		free(item);
		return NULL;
	}

	return item;
}

void krg_clusters_destroy(struct krg_clusters* item)
{
	if (item) {
		free(item->clusters);
		free(item);
	}
}

int krg_clusters_is_up(struct krg_clusters* item, int n)
{
	if (n >= 0 && n < kerrighed_max_clusters) {
		if (item->clusters[n])
			return 1;
		else
			return 0;
	} else
		return -1;
}

struct krg_node_set* krg_node_set_create(void)
{
	struct krg_node_set* item;

	item = malloc(sizeof(struct krg_node_set));
	if (!item) {
		return NULL;
	}

	item->v = malloc(kerrighed_max_nodes);
	if (!item->v) {
		free(item);
		return NULL;
	}

	krg_node_set_clear(item);

	return item;
}

void krg_node_set_destroy(struct krg_node_set* item)
{
	if (item) {
		free(item->v);
		free(item);
	}
}

void krg_node_set_clear(struct krg_node_set* item)
{
	int bcl;

	item->subclusterid = 0;
	for (bcl = 0; bcl < kerrighed_max_nodes; bcl++)
		item->v[bcl] = 0;
}

int krg_node_set_add(struct krg_node_set* item, int n)
{
	if (n >= 0 && n < kerrighed_max_nodes) {
		item->v[n] = 1;
		return 0;
	} else
		return -1;
}

int krg_node_set_remove(struct krg_node_set* item, int n)
{
	if (n >= 0 && n < kerrighed_max_nodes) {
		item->v[n] = 0;
		return 0;
	} else
		return -1;
}

int krg_node_set_contains(struct krg_node_set* node_set, int n)
{
	if (n >= 0 && n < kerrighed_max_nodes)
		return node_set->v[n];
	else
		return 0;
}

int krg_node_set_weight(struct krg_node_set* node_set)
{
	int r, bcl;

	r = 0;
	for (bcl = 0; bcl < kerrighed_max_nodes; bcl++)
		if (node_set->v[bcl])
			r++;

	return r;
}

int krg_node_set_next(struct krg_node_set* node_set, int node)
{
	int bcl;

	bcl = node + 1;
	while (bcl < kerrighed_max_nodes) {
		if (node_set->v[bcl])
			return bcl;
		bcl++;
	}
	return -1;
}

int krg_get_max_nodes(void)
{
	int r;

	if(kerrighed_max_nodes == -1){
		r = call_kerrighed_services(KSYS_NB_MAX_NODES, &kerrighed_max_nodes);
		if(r) {
			errno = EPERM;
			return -1;
		}
	}

	return kerrighed_max_nodes;
}

int krg_get_max_clusters(void)
{
	int r;

	if(kerrighed_max_clusters == -1){
		r = call_kerrighed_services(KSYS_NB_MAX_CLUSTERS, &kerrighed_max_clusters);
		if(r) {
			errno = EPERM;
			return -1;
		}
	}

	return kerrighed_max_clusters;
}

int krg_hotplug_init(void)
{
	kerrighed_max_nodes = krg_get_max_nodes();
	if (kerrighed_max_nodes == -1)
		return -1;
	kerrighed_max_clusters = krg_get_max_clusters();
	if (kerrighed_max_clusters == -1)
		return -1;

	return 0;
}

int krg_check_hotplug(void)
{
	int r, node_id;

	r = call_kerrighed_services(KSYS_GET_NODE_ID, &node_id);
	if (r != 0) {
		errno = EAGAIN;
		return -1;
	}

	return 0;
}

int krg_check_container(void)
{
	struct stat buf;

	if ( stat("/proc/nodes/self", &buf) != 0 ) {
		return -1;
	}

	return 0;
}

int krg_nodes_add(struct krg_node_set *krg_node_set)
{
	struct hotplug_node_set node_set;
	int i;

	krgnodes_clear(node_set.v);
	node_set.subclusterid = krg_node_set->subclusterid;
	
	for (i = 0; i < kerrighed_max_nodes; i++) {
		if(krg_node_set->v[i]){
			krgnode_set(i, node_set.v);
		}
	}

	return call_kerrighed_services(KSYS_HOTPLUG_ADD, &node_set);
}

int krg_nodes_remove(struct krg_node_set *krg_node_set)
{
	struct hotplug_node_set node_set;
	int i;

	krgnodes_clear(node_set.v);
	node_set.subclusterid = krg_node_set->subclusterid;
	
	for(i = 0; i < kerrighed_max_nodes; i++) {
		if(krg_node_set->v[i]){
			krgnode_set(i, node_set.v);
		}
	}

	return call_kerrighed_services(KSYS_HOTPLUG_REMOVE, &node_set);
}

int krg_nodes_fail(struct krg_node_set *krg_node_set){
	struct hotplug_node_set node_set;
	int i;

	krgnodes_clear(node_set.v);
	
	for (i = 0; i < kerrighed_max_nodes; i++) {
		if (krg_node_set->v[i]){
			krgnode_set(i, node_set.v);
		}
	}

	return call_kerrighed_services(KSYS_HOTPLUG_FAIL, &node_set);
}

int krg_nodes_poweroff(struct krg_node_set *krg_node_set)
{
	struct hotplug_node_set node_set;
	int i;

	krgnodes_clear(node_set.v);
	
	for (i = 0; i < kerrighed_max_nodes; i++) {
		if (krg_node_set->v[i]) {
			krgnode_set(i, node_set.v);
		}
	}

	return call_kerrighed_services(KSYS_HOTPLUG_POWEROFF, &node_set);
}

struct krg_nodes* krg_nodes_status(void)
{
	struct krg_nodes *krg_nodes;
	struct hotplug_nodes hotplug_nodes;
	int r;

	krg_nodes = krg_nodes_create();
	if (!krg_nodes)
		return NULL;
	
	hotplug_nodes.nodes = krg_nodes->nodes;
	
	r = call_kerrighed_services(KSYS_HOTPLUG_NODES, &hotplug_nodes);
	
	if (r) {
		krg_nodes_destroy(krg_nodes);
		return  NULL;
	}

	return krg_nodes;
}

struct krg_clusters* krg_cluster_status(void)
{
	struct krg_clusters *krg_clusters;
	struct hotplug_clusters hotplug_clusters;
	int r;

	krg_clusters = krg_clusters_create();
	if (!krg_clusters)
		return NULL;
	
	r = call_kerrighed_services(KSYS_HOTPLUG_STATUS, &hotplug_clusters);

	if (r) {
		krg_clusters_destroy(krg_clusters);
		return NULL;
	}

	memcpy(krg_clusters->clusters, hotplug_clusters.clusters, kerrighed_max_clusters);
	
	return krg_clusters;
}

int krg_set_cluster_creator(int enable)
{
	return call_kerrighed_services(KSYS_HOTPLUG_SET_CREATOR,
				       enable ? (void *)1 : NULL);
}

int krg_node_ready(int setup_ok)
{
	return call_kerrighed_services(KSYS_HOTPLUG_READY,
				       setup_ok ? NULL : (void *)1);
}

int krg_cluster_shutdown(int subclusterid)
{
	return call_kerrighed_services(KSYS_HOTPLUG_SHUTDOWN, &subclusterid);
}

int krg_cluster_reboot(int subclusterid)
{
	return call_kerrighed_services(KSYS_HOTPLUG_RESTART, &subclusterid);
}
