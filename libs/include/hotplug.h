#ifndef __LIB_HOTPLUG__
#define __LIB_HOTPLUG__

#include <stdio.h>

struct krg_nodes {
	char* nodes;
};

struct krg_clusters {
	char* clusters;
};

struct krg_node_set {
	int subclusterid;
	char* v;
};

int krg_get_max_nodes(void);
int krg_get_max_clusters(void);

static inline void krg_clear_node_set(struct krg_node_set *item){
	int bcl;

	for(bcl=0;bcl<krg_get_max_nodes();bcl++)
		item->v[bcl] = 0;

}

static inline void krg_node_set_add(struct krg_node_set *item, int n){
	if(n>=0 && n<krg_get_max_nodes())
		item->v[n] = 1;
	else
		printf("ERROR: krg_node_set_add: out of range (%d)\n", n);
}

int krg_nodes_add(struct krg_node_set *node_set);
int krg_nodes_remove(struct krg_node_set *node_set);
int krg_nodes_fail(struct krg_node_set *node_set);
int krg_nodes_poweroff(struct krg_node_set *node_set);
struct krg_nodes* krg_nodes_status(void);
struct krg_clusters* krg_cluster_status(void);
int krg_set_cluster_creator(int enable);
int krg_cluster_start(struct krg_node_set *krg_node_set);
int krg_cluster_wait_for_start(void);
int krg_cluster_shutdown(int subclusterid);
int krg_cluster_reboot(int subclusterid);

#endif
