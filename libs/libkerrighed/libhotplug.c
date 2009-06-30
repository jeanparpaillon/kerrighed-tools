/** Cluster configuration related interface functions.
 *  @file libhotplug.c
 *
 *  Copyright (C) 2006-2007, Pascal Gallard, Kerlabs.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>

#include <types.h>
#include <krgnodemask.h>
#include <kerrighed_tools.h>
#include <hotplug.h>

static int kerrighed_max_nodes = -1;
static int kerrighed_max_clusters = -1;

int krg_get_max_nodes(void){
	int r;

	if(kerrighed_max_nodes == -1){
		r = call_kerrighed_services(KSYS_NB_MAX_NODES, &kerrighed_max_nodes);
		if(r) return -1;
	};

	return kerrighed_max_nodes;
};

int krg_get_max_clusters(void){
	int r;

	if(kerrighed_max_clusters == -1){
		r = call_kerrighed_services(KSYS_NB_MAX_CLUSTERS, &kerrighed_max_clusters);
		if(r) return -1;
	};

	return kerrighed_max_clusters;
};

int krg_nodes_add(struct krg_node_set *krg_node_set){
	struct hotplug_node_set node_set;
	int i, r;

	krgnodes_clear(node_set.v);
	node_set.subclusterid = krg_node_set->subclusterid;
	
	for(i=0;i<krg_get_max_nodes();i++){
		if(krg_node_set->v[i]){
			krgnode_set(i, node_set.v);
		};
	};

	r = call_kerrighed_services(KSYS_HOTPLUG_ADD, &node_set);
	if (r) return -1;
	
	return 0;
};

int krg_nodes_remove(struct krg_node_set *krg_node_set){
	struct hotplug_node_set node_set;
	int i, r;

	krgnodes_clear(node_set.v);
	node_set.subclusterid = krg_node_set->subclusterid;
	
	for(i=0;i<krg_get_max_nodes();i++){
		if(krg_node_set->v[i]){
			printf("krg_nodes_remove: %d\n", i);
			krgnode_set(i, node_set.v);
		};
	};

	r = call_kerrighed_services(KSYS_HOTPLUG_REMOVE, &node_set);
	if (r) return -1;

	return 0;
};

int krg_nodes_fail(struct krg_node_set *krg_node_set){
	struct hotplug_node_set node_set;
	int i, r;

	krgnodes_clear(node_set.v);
	
	for(i=0;i<krg_get_max_nodes();i++){
		if(krg_node_set->v[i]){
			krgnode_set(i, node_set.v);
		};
	};

	r = call_kerrighed_services(KSYS_HOTPLUG_FAIL, &node_set);
	if (r) return -1;

	return 0;
};

int krg_nodes_poweroff(struct krg_node_set *krg_node_set){
	struct hotplug_node_set node_set;
	int i, r;

	krgnodes_clear(node_set.v);
	
	for(i=0;i<krg_get_max_nodes();i++){
		if(krg_node_set->v[i]){
			krgnode_set(i, node_set.v);
		};
	};

	r = call_kerrighed_services(KSYS_HOTPLUG_POWEROFF, &node_set);
	if (r) return -1;

	return 0;
};

struct krg_nodes*  krg_nodes_status(void){
	struct krg_nodes *krg_nodes;
	struct hotplug_nodes hotplug_nodes;
	int r;

	krg_nodes = malloc(sizeof(struct krg_nodes));
	if(!krg_nodes)
		return NULL;
	krg_nodes->nodes = malloc(sizeof(char)*krg_get_max_nodes());
	if(!krg_nodes->nodes)
		return NULL;
	
	hotplug_nodes.nodes = krg_nodes->nodes;
	
	r = call_kerrighed_services(KSYS_HOTPLUG_NODES, &hotplug_nodes);
	
	if(r){
		free(krg_nodes->nodes);
		free(krg_nodes);
		return  NULL;
	};

	return krg_nodes;
};

struct krg_clusters* krg_cluster_status(void){
	struct krg_clusters *krg_clusters;
	struct hotplug_clusters hotplug_clusters;
	int r;

	if(! (krg_clusters = malloc(sizeof(*krg_clusters))) )
		return NULL;
	if(! (krg_clusters->clusters = malloc(sizeof(char)*krg_get_max_clusters())))
	      return NULL;
	
	r = call_kerrighed_services(KSYS_HOTPLUG_STATUS, &hotplug_clusters);

	if(r){
		free(krg_clusters->clusters);
		free(krg_clusters);
		return NULL;
	};

	memcpy(krg_clusters->clusters, hotplug_clusters.clusters, krg_get_max_clusters());
	
	return krg_clusters;
};

int krg_cluster_start(struct krg_node_set *krg_node_set){
	struct hotplug_node_set node_set;
	int i, r;

	node_set.subclusterid = krg_node_set->subclusterid;

	krgnodes_clear(node_set.v);
	
	for(i=0;i<krg_get_max_nodes();i++){
		if(krg_node_set->v[i]){
			krgnode_set(i, node_set.v);
		};
	};
	
	r = call_kerrighed_services(KSYS_HOTPLUG_START, &node_set);
	if(r)
		return -1;
	
	return 0;
};

int krg_cluster_wait_for_start(void)
{
	return call_kerrighed_services(KSYS_HOTPLUG_WAIT_FOR_START, NULL);
}

int krg_cluster_shutdown(int subclusterid){
	int r;

	r = call_kerrighed_services(KSYS_HOTPLUG_SHUTDOWN, &subclusterid);
	if(r)
		return -1;
	
	return 0;
};

int krg_cluster_reboot(int subclusterid){
	int r;

	r = call_kerrighed_services(KSYS_HOTPLUG_RESTART, &subclusterid);
	if(r)
		return -1;
	
	return 0;
};
