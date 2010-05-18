#ifndef __LIB_HOTPLUG__
#define __LIB_HOTPLUG__

enum krg_status {
	HOTPLUG_NODE_INVALID,
	HOTPLUG_NODE_POSSIBLE,
	HOTPLUG_NODE_PRESENT,
	HOTPLUG_NODE_ONLINE,
};

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

extern int kerrighed_max_nodes;
extern int kerrighed_max_clusters;

/*
 * krg_hotplug_init
 *
 * Initialize some hotplug internals
 *
 * Return 0 on success, -1 on failure
 */
int krg_hotplug_init(void);

/*
 * krg_check_hotplug
 *
 * Check if we are in a running Kerrighed container
 *
 * Return 0 if ok, -1 otherwise
 */
int krg_check_hotplug(void);

/*
 * krg_check_container
 *
 * Check if Kerrighed container is already running
 *
 * Return 0 if running, -1 otherwise
 */
int krg_check_container(void);

/*
 * krg_status_str
 *
 * Return the name of the status
 */
const char* krg_status_str(int s);

/*
 * krg_node_set_create
 *
 * Creates and initializes a krg_node_set.
 *
 * Returns NULL on memory allocation error
 */
struct krg_node_set* krg_node_set_create(void);

/*
 * krg_node_set_destroy
 *
 * Destroy a krg_node_set
 */
void krg_node_set_destroy(struct krg_node_set* node_set);

/*
 * krg_node_set_clear
 *
 * Reset node_set
 */
void krg_node_set_clear(struct krg_node_set* node_set);

/*
 * krg_node_set_add
 *
 * Add node with id n to node_set.
 * n must be >= 0 and < kerrighed_max_nodes
 *
 * Returns 0 on success, -1 on failure (n out of range)
 */
int krg_node_set_add(struct krg_node_set* node_set, int n);

/*
 * krg_node_set_remove
 *
 * Remove node with id n from node_set.
 * n must be >= 0 and < kerrighed_max_nodes
 *
 * Returns 0 on success, -1 on failure (n out of range)
 */
int krg_node_set_remove(struct krg_node_set* node_set, int n);

/*
 * krg_node_set_contains
 *
 * Returns 1 if node is present in the set, 0 if not
 */
int krg_node_set_contains(struct krg_node_set* node_set, int n);

/*
 * krg_node_set_weight
 *
 * Returns number of nodes in set
 */
int krg_node_set_weight(struct krg_node_set* node_set);

/*
 * krg_node_set_next
 *
 * Give -1 as n to initialize iterator
 *
 * Returns next node in the set, -1 if no next
 */
int krg_node_set_next(struct krg_node_set* node_set, int n);

/*
 * krg_nodes_create
 *
 * Creates and initializes a krg_nodes struct
 */
struct krg_nodes* krg_nodes_create(void);

/*
 * krg_nodes_destroy
 *
 * Destroy a krg_nodes
 */
void krg_nodes_destroy(struct krg_nodes *nodes);

/*
 * krg_nodes_num
 *
 * Return number of nodes in the given status
 */
int krg_nodes_num(struct krg_nodes* nodes, enum krg_status s);
int krg_nodes_num_online(struct krg_nodes* nodes);
int krg_nodes_num_possible(struct krg_nodes* nodes);
int krg_nodes_num_present(struct krg_nodes* nodes);

/*
 * krg_nodes_is
 * node must be >= 0 and < kerrighed_max_nodes
 *
 * Return 1 if node is in the given status, 0 otherwise, -1 in case of failure (n out of range)
 */
int krg_nodes_is(struct krg_nodes* nodes, int node, enum krg_status s);
int krg_nodes_is_online(struct krg_nodes* nodes, int node);
int krg_nodes_is_possible(struct krg_nodes* nodes, int node);
int krg_nodes_is_present(struct krg_nodes* nodes, int node);

/*
 * krg_nodes_next
 * node must be < kerrighed_max_nodes
 * give -1 as node to initialize iterator
 *
 * Return node id in the given state following given node, -1 if no next
 */
int krg_nodes_next(struct krg_nodes* nodes, int node, enum krg_status s);
int krg_nodes_next_online(struct krg_nodes* nodes, int node);
int krg_nodes_next_possible(struct krg_nodes* nodes, int node);
int krg_nodes_next_present(struct krg_nodes* nodes, int node);

/*
 * krg_nodes_get
 *
 * Return set of nodes in given state, NULL in case of failure (out of mem)
 */
struct krg_node_set* krg_nodes_get(struct krg_nodes* nodes, enum krg_status s);
struct krg_node_set* krg_nodes_get_online(struct krg_nodes* nodes);
struct krg_node_set* krg_nodes_get_possible(struct krg_nodes* nodes);
struct krg_node_set* krg_nodes_get_present(struct krg_nodes* nodes);

/*
 * krg_nodes_getnode
 *
 * Return status of given node
 */
enum krg_status krg_nodes_getnode(struct krg_nodes* nodes, int node);

/*
 * krg_nodes_nextnode
 * Give -1 as node to initialize iterator
 *
 * Return node id following given node which is not invalid, -1 if no next
 */
int krg_nodes_nextnode(struct krg_nodes* nodes, int node);

/*
 * krg_clusters_create
 *
 * Creates and initializes a krg_clusters struct
 * Returns NULL on failure (out of mem)
 */
struct krg_clusters* krg_clusters_create(void);

/*
 * krg_clusters_destroy
 *
 * Destroy a krg_clusters
 */
void krg_clusters_destroy(struct krg_clusters* clusters);

/*
 * krg_clusters_is_up
 *
 * Return 1 if cluster is up, 0 otherwise, -1 in case of failure (invalid cluster id)
 */
int krg_clusters_is_up(struct krg_clusters* nodes, int cluster);

int krg_nodes_add(struct krg_node_set *node_set);
int krg_nodes_remove(struct krg_node_set *node_set);
int krg_nodes_fail(struct krg_node_set *node_set);
int krg_nodes_poweroff(struct krg_node_set *node_set);
struct krg_nodes* krg_nodes_status(void);
struct krg_clusters* krg_cluster_status(void);
int krg_set_cluster_creator(int enable);
int krg_node_ready(int setup_ok);
int krg_cluster_shutdown(int subclusterid);
int krg_cluster_reboot(int subclusterid);

#endif
