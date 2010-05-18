/*
 *  Copyright (C) 2006-2007, Pascal Gallard, Kerlabs.
 *  Copyright (c) 2008 Jean Parpaillon <jean.parpaillon@kerlabs.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <config.h>
#include <kerrighed.h>

enum {
	NONE,
	STATUS,
	ADD,
	DEL,
	POWEROFF,
	REBOOT,
};

enum mode_t {
	NODES_MODE_UNSET,
	NODES_MODE_ALL,
	NODES_MODE_COUNT,
	NODES_MODE_TOTAL,
	NODES_MODE_LIST,
};

#define NODE_SEP ','
#define NODE_RANGE_SEP '-'
#define POLL_NODES 1
#define NODES_OPTION "nodes", required_argument, NULL, 'n'
#define COUNT_OPTION "count", required_argument, NULL, 'c'
#define TOTAL_OPTION "total", required_argument, NULL, 't'
#define ALL_OPTION "all", no_argument, NULL, 'a'

#define MSG_EXCLUSIVE_OPTIONS "Only one of --all, --count, --total or --nodes option is\n\
supported. Aborting.\n"

static struct option nodes_mode_options[] = {
	{NODES_OPTION},
	{COUNT_OPTION},
	{TOTAL_OPTION},
	{ALL_OPTION},
	{NULL, 0, NULL, 0}
};

void version(char * program_name)
{
	printf("\
%s %s\n\
Copyright (C) 2010 Kerlabs.\n\
This is free software; see source for copying conditions. There is NO\n\
warranty; not even for MERCHANBILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n", program_name, VERSION);
}

void help(char * program_name)
{
	printf("\
Usage: %s [-h|--help] [--version]\n\
  or:  %s cluster {status|poweroff|reboot}\n\
  or:  %s nodes status [-n|--nodes]\n\
  or:  %s nodes {add|del} {(-n|--nodes node_list) | (-c|--count node_count) | (-t|--total node_count) |(-a|--all)}\n",
	       program_name, program_name, program_name, program_name);
	printf("\n\
Cluster Mode:\n\
  status            print cluster status\n\
  poweroff          poweroff all nodes in the cluster\n\
  reboot            restart all nodes in the cluster\n\
\n\
Nodes Mode:\n\
  status            print status of each present nodes\n\
  add               add nodes to the running cluster\n\
  del               remove nodes from the running cluster\n\
\n\
Options:\n\
  -n, --nodes       list of nodes to add/del\n\
                    ie: 2,3,6-10,42-45\n\
  -c, --count       number of nodes to add/del\n\
  -t, --total       number of nodes in resulting cluster\n\
  -a, --all         apply to all nodes\n\
\n\
Node Status:\n\
  present           available for integrating the cluster\n\
  online            participating in the cluster\n\
\n");
}

char* node_set_str(struct krg_node_set* node_set)
{
	char* str;
	int node, l, i = 1;

	l = kerrighed_max_nodes*4+2;
	str = malloc(l);

	str[0] = '[';
	node = krg_node_set_next(node_set, -1);
	while (node != -1) {
		if (i > 1)
			str[i++] = NODE_SEP;
		i += snprintf(str+i, l, "%d", node);
		node = krg_node_set_next(node_set, node);
	}
	str[i] = ']';

	return str;
}

int parse_node(char *ch, struct krg_node_set *node_set)
{
	int node;
	char* endptr;
	int r = 1;

	node = strtol(ch, &endptr, 10);
	if (endptr == ch || *endptr) {
		r = -1;
		errno = EINVAL;
		perror("node");
	} else
		krg_node_set_add(node_set, node);

	return r;
}

int parse_nodes_interval(char *ch, struct krg_node_set *node_set)
{
	int node1, node2, node;
	char* index;
	char* endptr;
	int r = 1;

	index = strchr(ch, NODE_RANGE_SEP);
	node1 = strtol(ch, &endptr, 10);
	if (endptr == ch || NODE_RANGE_SEP != *endptr) {
		r = -1;
		errno = EINVAL;
		perror("first node range");
		goto exit;
	}
	node2 = strtol(index+1, &endptr, 10);
	if (endptr == index+1 || *endptr) {
		r = -1;
		errno = EINVAL;
		perror("end node range");
		goto exit;
	}

	for(node = node1; node <= node2; node++)
		krg_node_set_add(node_set, node);

exit:
	return r;
}

int parse_nodes(char *ch, struct krg_node_set *node_set)
{
	char *ch_item;
	int r = 1;

	ch_item = strrchr(ch, NODE_SEP);

	if(ch_item){
		*ch_item = 0;
		if ((r = parse_nodes(ch, node_set)) == -1)
			goto exit;
		if ((r = parse_nodes(ch_item+1, node_set)) == -1)
			goto exit;
		goto exit;
	}

	ch_item = strrchr(ch, NODE_RANGE_SEP);
	if(ch_item) {
		if ((r = parse_nodes_interval(ch, node_set)) == -1)
			goto exit;
	} else
		if ((r = parse_node(ch, node_set)) == -1)
			goto exit;

exit:
	return r;
}

/*
 * When returning on success, node_set contains nodes to start.
 *
 * Return 0 when i nodes are present, -1 on failure.
 */
int wait_for_nodes_count(int i, struct krg_node_set* node_set)
{
	struct krg_nodes* status;
	int cur, r = 0;
	int nodes_count;

	if (i > 0) {
		printf("Waiting for %d nodes to join... ", i);
		fflush(stdout);
	} else
		return 0;

	if (i < 1 || i > kerrighed_max_nodes) {
		r = -1;
		errno = ERANGE;
		goto exit;
	}

	do {
		status = krg_nodes_status();
		if (! status) {
			errno = ENOMEM;
			r = -1;
			goto exit;
		}

		nodes_count = krg_nodes_num_present(status);
		printf("%4d/%-4d", nodes_count, i);
		fflush(stdout);

		if (nodes_count < i) {
			sleep(POLL_NODES);
			printf("\b\b\b\b\b\b\b\b\b");
			krg_nodes_destroy(status);
		}

		fflush(stdout);
	} while (nodes_count < i);

	nodes_count = 0;
	cur = -1;
	do {
		cur = krg_nodes_next_present(status, cur);
		krg_node_set_add(node_set, cur);
		nodes_count++;
	} while (nodes_count < i);

	krg_nodes_destroy(status);

exit:
	if (r == 0)
		printf(" done\n");
	else
		printf(" fail (%s)\n", strerror(errno));

	return r;
}

/*
 * Return 0 when nodes in node_set are present, -1 on failure.
 */
int wait_for_nodes(struct krg_node_set* node_set)
{
	struct krg_nodes* status;
	int bcl, r = 0, done, node_count;

	if (krg_node_set_weight(node_set) > 0) {
		printf("Waiting for nodes to join... ");
		fflush(stdout);
	} else
		return 0;

	node_count = krg_node_set_weight(node_set);
	do {
		done = 1;
		status = krg_nodes_status();
		if (! status) {
			r = -1;
			goto exit;
		}
		for (bcl = 0; bcl < kerrighed_max_nodes; bcl++) {
			if (krg_node_set_contains(node_set, bcl)) {
				printf("%4d:", bcl);
				if (krg_nodes_is_present(status, bcl))
					printf("1");
				else {
					done = 0;
					printf("0");
				}
			}
			fflush(stdout);
		}
		krg_nodes_destroy(status);

		if (! done) {
			sleep(POLL_NODES);

			for (bcl = 0; bcl < node_count; bcl++)
				printf("\b\b\b\b\b\b");
			fflush(stdout);
		}

	} while (! done);

exit:
	if (r == 0)
		printf(" done\n");
	else
		printf(" fail (%s)\n", strerror(errno));

	return r;
}

/*
 * nodes_status
 *
 * Print status of nodes in node_set, or all nodes if mode == NODES_MODE_ALL
 *
 * Return 0 on success, -1 on failure
 */
int nodes_status(struct krg_node_set* node_set, enum mode_t mode)
{
	struct krg_nodes* status;
	int bcl, node;

	status = krg_nodes_status();
	if (! status) {
		errno = ENOMEM;
		perror("Error adding nodes");
		return -1;
	}
	
	if (mode == NODES_MODE_ALL) {
		/* Add all nodes with status PRESENT or ONLINE */
		node_set = krg_node_set_create();
		for (bcl = 0; bcl < kerrighed_max_nodes; bcl++) {
			if (status->nodes[bcl] > HOTPLUG_NODE_POSSIBLE)
				krg_node_set_add(node_set, bcl);
		}
	} else {
		node = krg_node_set_next(node_set, -1);
		while (node != -1) {
			if(status->nodes[node] < HOTPLUG_NODE_PRESENT)
				krg_node_set_remove(node_set, node);
			node = krg_node_set_next(node_set, node);
		}
	}

	node = krg_node_set_next(node_set, -1);
	while (node != -1) {
		printf("%d:%s\n", node, krg_status_str(status->nodes[node]));
		node = krg_node_set_next(node_set, node);
	}

	return 0;
}

/*
 * nodes_add
 *
 * If mode == NODES_MODE_ALL, add all PRESENT nodes
 * If mode == NODES_MODE_LIST, add nodes in node_set
 * If mode == NODES_MODE_COUNT, wait for n nodes to be PRESENT, then add them
 * If mode == NODES_MODE_TOTAL, wait for n nodes to be PRESENT or ONLINE,
 * then add PRESENT nodes.
 *
 * Return 0 on success, -1 on failure
 */
int nodes_add(struct krg_node_set* node_set, int n, enum mode_t mode)
{
	struct krg_nodes* status;
	int node, r = 0;

	status = krg_nodes_status();
	if (! status) {
		perror("Error adding nodes");
		return -1;
	}

	switch (mode) {
	case NODES_MODE_ALL:
		/* add all PRESENT nodes */
		node_set = krg_nodes_get_present(status);
		if (! node_set) {
			errno = ENOMEM;
			perror("Error looking for present nodes");
			krg_node_set_destroy(node_set);
			return -1;
		}
		break;
	case NODES_MODE_LIST:
		/* Remove online nodes from node_set */
		node = krg_node_set_next(node_set, -1);
		while (node != -1) {
			if (krg_nodes_is_online(status, node))
				krg_node_set_remove(node_set, node);
			node = krg_node_set_next(node_set, node);
		}

		if (wait_for_nodes(node_set) == -1)
			return -1;
		break;
	case NODES_MODE_TOTAL:
		n -= krg_nodes_num_online(status);
	case NODES_MODE_COUNT:
		node_set = krg_node_set_create();
		if (wait_for_nodes_count(n, node_set) == -1)
			return -1;
		break;
	default:
		return -1;
	}

	if (krg_node_set_weight(node_set) > 0) {
		printf("Adding nodes %s... ", node_set_str(node_set));
		fflush(stdout);
		r = krg_nodes_add(node_set);
		if (r == -1)
			perror("fail");
		else
			printf("done\n");
	} else
		printf("No present node to add.\n");
	return r;
}

/*
 * nodes_remove
 *
 * If mode == NODES_MODE_ALL, remove all PRESENT nodes except current one
 * If mode == NODES_MODE_LIST, remove nodes in node_set
 * If mode == NODES_MODE_COUNT, remove n ONLINE nodes
 * If mode == NODES_MODE_TOTAL, remove enough nodes for the cluster to contains n nodes
 *
 * Return 0 on success, -1 on failure
 */
int nodes_remove(struct krg_node_set* node_set, int n, enum mode_t mode)
{
	struct krg_nodes* status;
	int bcl, node, r = 0;

	status = krg_nodes_status();
	if (! status) {
		perror("Error removing nodes");
		return -1;
	}

	switch (mode) {
	case NODES_MODE_ALL:
		/* remove all nodes except current */
		node_set = krg_node_set_create();
		for (bcl = 0; bcl < kerrighed_max_nodes; bcl++) {
			if (krg_nodes_is_online(status, bcl) && get_node_id() != bcl)
				krg_node_set_add(node_set, bcl);
		}
		break;
	case NODES_MODE_LIST:
		/* check given nodes are 'online', and not the current one */
		node = krg_node_set_next(node_set, -1);
		while (node != -1) {
			if (! krg_nodes_is_online(status, node)) {
				printf("Unable to suppress node %d (must be 'online').\n", node);
				krg_node_set_remove(node_set, node);
			} else if (get_node_id() == node) {
				printf("Unable to suppress current node.\n");
				krg_node_set_remove(node_set, node);
			}
			node = krg_node_set_next(node_set, node);
		}
		break;
	case NODES_MODE_TOTAL:
		n = krg_nodes_num_online(status) - n;
	case NODES_MODE_COUNT:
		/* remove 'n' first online nodes */
		node_set = krg_nodes_get_online(status);
		if (n < krg_node_set_weight(node_set)) {
			bcl = 0;
			node = krg_node_set_next(node_set, -1);
			while (node != -1) {
				if (bcl < n)
					if (get_node_id() == node)
						krg_node_set_remove(node_set, node);
					else
						bcl++;
				else
					krg_node_set_remove(node_set, node);
				node = krg_node_set_next(node_set, node);
			}
		} else {
			printf("Not enough nodes to remove. Aborting.\n");
			return -1;
		}
		break;
	default:
		return -1;
	}

	if (krg_node_set_weight(node_set) > 0) {
		printf("Removing nodes %s... ", node_set_str(node_set));
		fflush(stdout);
		r = krg_nodes_remove(node_set);
		if (r == -1)
			perror("fail");
		else
			printf("done\n");
	} else
		printf("No online node to remove.\n");
	return r;
}

/*
 * Return number of nodes in the cluster if up, 0 if not, -1 on failure
 */
int cluster_status(void)
{
	struct krg_clusters* cluster_status;
	int i = 0;

	cluster_status = krg_cluster_status();
	if(! cluster_status){
		i = -1;
		goto exit;
	}

	if (krg_clusters_is_up(cluster_status, 0)){
		i = krg_nodes_num_online(krg_nodes_status());
	}

exit:
	krg_clusters_destroy(cluster_status);
	return i;
}

/*
 * Return EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int cluster(int argc, char* argv[], char* program_name)
{
	int action = NONE;
	int ret = EXIT_SUCCESS;
	int r;

	r = krg_check_hotplug();
	if (r) {
		perror("Kerrighed is not running");
		return EXIT_FAILURE;
	}

	krg_hotplug_init();

	if(argc == 0 || ! strcmp(*argv, "status"))
		action = STATUS;
	else if(! strcmp(*argv, "poweroff"))
		action = POWEROFF;
	else if(! strcmp(*argv, "reboot"))
		action = REBOOT;

	switch (action) {
	case STATUS:
		printf("status: ");
		r = cluster_status();
		switch(r){
		case -1:
			printf("error\n");
			ret = EXIT_FAILURE;
			break;
		case 0:
			printf("down\n");
			break;
		default:
			printf("up on %d nodes\n", r);
		}
		break;
	case POWEROFF:
		printf("Shutting down cluster... ");
		fflush(stdout);
		if (cluster_status() == 0) {
			printf("fail (cluster not running)\n");
			ret = EXIT_FAILURE;
		} else {
			r = krg_cluster_shutdown(0);
			if (r == -1) {
				printf("fail (%s)\n", strerror(errno));
				ret = EXIT_FAILURE;
			} else
				printf("done\n");
		}
		break;
	case REBOOT:
		printf("Rebooting cluster... ");
		fflush(stdout);
		if (cluster_status() == 0) {
			printf("fail (cluster not running)\n");
			ret = EXIT_FAILURE;
		} else {
			r = krg_cluster_reboot(0);
			if (r == -1) {
				printf("fail (%s)\n", strerror(errno));
				ret = EXIT_FAILURE;
			} else
				printf("done\n");
		}
		break;
	default:
		help(program_name);
	}

	return ret;
}

/*
 * Return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int nodes(int argc, char* argv[], char* program_name)
{
	struct krg_node_set* node_set = NULL;
	char* endptr;
	int c, option_index;
	enum mode_t mode = NODES_MODE_UNSET;
	int nb_nodes = -1;
	int action = NONE;
	int ret;

	ret = krg_check_hotplug();
	if (ret) {
		perror("Kerrighed is not running");
		return EXIT_FAILURE;
	}

	krg_hotplug_init();

	if(argc == 0 || !strcmp(*argv, "status")) {
		action = STATUS;
	} else if(! strcmp(*argv, "add"))
		action = ADD;
	else if(! strcmp(*argv, "del"))
		action = DEL;

	while ((c = getopt_long(argc, argv, "n:c:t:a",
				nodes_mode_options, &option_index)) != -1) {
		if ( 'n' == c ) {
			if (mode != NODES_MODE_UNSET) {
				printf(MSG_EXCLUSIVE_OPTIONS);
				return EXIT_FAILURE;
			}
			mode = NODES_MODE_LIST;

			node_set = krg_node_set_create();
			if (! node_set)
				return EXIT_FAILURE;
			if (parse_nodes(optarg, node_set) == -1)
				return EXIT_FAILURE;
		} else if ( 'c' == c || 't' == c ) {
			if (mode != NODES_MODE_UNSET) {
				printf(MSG_EXCLUSIVE_OPTIONS);
				return EXIT_FAILURE;
			}
			if ( 'c' == c )
				mode = NODES_MODE_COUNT;
			else
				mode = NODES_MODE_TOTAL;

			if (action == STATUS) {
				help(program_name);
				return EXIT_FAILURE;
			}
			nb_nodes = strtol(optarg, &endptr, 10);
			if (endptr == optarg || *endptr) {
				errno = EINVAL;
				perror("nodes number");
				return EXIT_FAILURE;
			}
		} else if ( 'a' == c ) {
			if (mode != NODES_MODE_UNSET) {
				printf(MSG_EXCLUSIVE_OPTIONS);
				return EXIT_FAILURE;
			}
			mode = NODES_MODE_ALL;
		} else {
			help(program_name);
			return EXIT_FAILURE;
		}
	}

	if ( (action == ADD || action == DEL) && mode == NODES_MODE_UNSET ) {
		/* ADD and DEL actions requires a mode */
		help(program_name);
		return EXIT_FAILURE;
	}

	switch (action) {
	case STATUS:
		if (mode == NODES_MODE_UNSET)
			mode = NODES_MODE_ALL;
		if (nodes_status(node_set, mode) == -1)
			return EXIT_FAILURE;
		break;
	case ADD:
		if (nodes_add(node_set, nb_nodes, mode) == -1)
			return EXIT_FAILURE;
		break;
	case DEL:
		if (nodes_remove(node_set, nb_nodes, mode) == -1)
			return EXIT_FAILURE;
		break;
	default:
		help(program_name);
	}

	return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
	char **arg;
	char* program_name;
	int count;
	int ret = EXIT_SUCCESS;

	arg = argv;
	count = argc;
	program_name = argv[0];

	if(argc == 1){
	  help(program_name);
	  exit(EXIT_SUCCESS);
	}

	opterr = 0;

	count--;
	arg++;

	if (! strcmp(argv[1], "cluster"))
		ret = cluster(count-1, arg+1, program_name);
	else if (! strcmp(argv[1], "nodes"))
		ret = nodes(count-1, arg+1, program_name);
	else if (! strcmp(argv[1], "--version") )
		version(program_name);
	else
		help(program_name);

	return ret;
}
