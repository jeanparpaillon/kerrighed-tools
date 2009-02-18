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

#include <kerrighed.h>

enum {
	NONE,
	STATUS,
	START,
	WAIT_START,
	STOP,
	ADD,
	DEL,
	FAIL,
	SWAP,
	BAN,
	UNBAN,
	POWEROFF,
	REBOOT,
};

#define SUBSESSIONS_OPTION "subsession", required_argument, NULL, 's'
#define NODES_OPTION "nodes", required_argument, NULL, 'n'
#define MACS_OPTION "macs", required_argument,NULL, 'm'


static struct option cluster_mode_options[] = {
  {SUBSESSIONS_OPTION},
  {NODES_OPTION},
  {NULL, 0, NULL, 0}
};

static struct option nodes_mode_options[] = {
  {SUBSESSIONS_OPTION},
  {NODES_OPTION},
  {NULL, 0, NULL, 0}
};

void help(char * program_name)
{
	printf("\
Usage: %s -h,--help\n\
  or:  %s cluster {status|start|wait_start|poweroff|reboot} [-s|--subsession] [-n|--nodes]\n\
  or:  %s nodes   {status|add|del|poweroff|reboot|fail|swap|ban|unban} [-s|--subsession] [-n|--nodes]\n",
	       program_name, program_name, program_name);
	printf("\n\
Mode:\n\
  cluster           clusters management\n\
  nodes             nodes management in a cluster\n\
\n\
Options:\n\
  -n, --nodes       list of nodes\n\
  -s, --subsessions subsession id\n");
}

void parse_node(char *ch, struct krg_node_set *node_set)
{
	int node;

	node = strtol(ch, NULL, 10);
	krg_node_set_add(node_set, node);

}

void parse_nodes_interval(char *ch, struct krg_node_set *node_set)
{
	int node1, node2, node;
	char *index;

	index = strchr(ch, ':');
	node1 = strtol(ch, NULL, 10);
	node2 = strtol(index+1, NULL, 10);

	for(node=node1; node<=node2; node++)
		krg_node_set_add(node_set, node);

}

void parse_nodes(char *ch, struct krg_node_set *node_set)
{
	char *ch_item;

	ch_item = strrchr(ch, ',');

	if(ch_item){
		*ch_item = 0;
		parse_nodes(ch, node_set);
		parse_nodes(ch_item+1, node_set);
		return;
	}

	ch_item = strrchr(ch, ':');
	if(ch_item)
		parse_nodes_interval(ch, node_set);
	else
		parse_node(ch, node_set);
}

void cluster(int argc, char* argv[])
{
  int option_index;
	int action = NONE;
	int node = -1;
	int subsessionid = 0;
	int c;
	struct krg_clusters* cluster_status;
	struct krg_node_set node_set;

	node_set.v = malloc(sizeof(char)*krg_get_max_nodes());
	if(!node_set.v)
		return;

	krg_clear_node_set(&node_set);
	node_set.subclusterid = 0;

	cluster_status = krg_cluster_status();
	if(!cluster_status){
		printf("Are you sure to be on a Kerrighed node ?\n");
		goto exit;
	}

	if(argc==0 || !strcmp(*argv, "status")){
		int bcl, i = 0;

		for(bcl=0;bcl<krg_get_max_clusters();bcl++)
			if(cluster_status->clusters[bcl]){
				printf("%3d:%1d ", bcl,
				       cluster_status->clusters[bcl]);
				i=1;
			}

		if(i)
			printf("\n");
		else
			printf("No running cluster\n");

		goto exit;
	}

	if(!strcmp(*argv, "start"))
		action = START;
	else if(!strcmp(*argv, "wait_start"))
		action = WAIT_START;
	else if(!strcmp(*argv, "poweroff"))
		action = POWEROFF;
	else if(!strcmp(*argv, "reboot"))
		action = REBOOT;

	while((c=getopt_long(argc, argv, "s:n:",
			     cluster_mode_options, &option_index))!=-1){
		switch(c){
		case 's':
			subsessionid = strtol(optarg, NULL, 10);
			if(errno){
				perror("cluster");
				goto exit;
			}
			printf("sub cluster: %d\n", subsessionid);
			break;
		case 'n':
			node = 1;
			parse_nodes(optarg, &node_set);
			break;
		default:
			help(argv[0]);
			goto exit;
		}
	}

	switch(action){
	case START:
		if(node==-1){
			struct krg_nodes* nodes_status;
			int bcl, nb;

			printf("No node specified... we're going to start all available nodes\n");

			nodes_status = krg_nodes_status();
			if(!nodes_status){
				printf("Something wrong in your running cluster\n");
				goto exit;
			}

			nb = 0;

			for(bcl=0;bcl<krg_get_max_nodes();bcl++)
				if((nodes_status->nodes[bcl])){
					nb++;
					krg_node_set_add(&node_set, bcl);
				}

		}

		krg_cluster_start(&node_set);

		break;
	case WAIT_START:
		if (node==-1){
			printf("Waiting for cluster to start...\n");
			if (krg_cluster_wait_for_start())
				printf("failed!\n");
			else
				printf("done\n");
		}
		break;
	case POWEROFF:
		if(node==-1){
			krg_cluster_shutdown(0);
		}
		break;
	case REBOOT:
		if(node==-1){
			krg_cluster_reboot(0);
		}
		break;
	default:
		help(argv[0]);
		goto exit;
	}

exit:
	free(node_set.v);
}

void nodes(int argc, char* argv[])
{
	int option_index;
	int action = NONE;
	int node = -1;
	/* int subsessionid = 0; */
	int c;
	struct krg_node_set node_set;

	if(argc==0 || !strcmp(*argv, "status")){
		struct krg_nodes* status;
		int bcl;

		status = krg_nodes_status();
		if(status){
			for(bcl=0;bcl<krg_get_max_nodes();bcl++)
				if(status->nodes[bcl])
					printf("%3d:%1d ", bcl, status->nodes[bcl]);
			printf("\n");
		}else{
			printf("Are you sure to run Kerrighed ?\n");
		}

		return;
	}

	node_set.v = malloc(sizeof(*node_set.v)*krg_get_max_nodes());
	if(!node_set.v){
		printf("Out of memory!\n");
		return;
	}

	krg_clear_node_set(&node_set);
	node_set.subclusterid = 0;

	if(!strcmp(*argv, "add"))
		action = ADD;
	else if(!strcmp(*argv, "del"))
		action = DEL;
	else if(!strcmp(*argv, "poweroff"))
		action = POWEROFF;
	else if(!strcmp(*argv, "reboot"))
		action = REBOOT;
	else if(!strcmp(*argv, "fail"))
		action = FAIL;
	else if(!strcmp(*argv, "swap"))
		action = SWAP;

	while((c=getopt_long(argc, argv, "s:n:",
			     nodes_mode_options, &option_index))!=-1){
		switch(c){
		case 's':
			node_set.subclusterid = strtol(optarg, NULL, 10);
			if(errno){
				perror("nodes");
				return;
			}
			printf("sub cluster: %d\n", node_set.subclusterid);
			break;
		case 'n':
			node = 1;
			parse_nodes(optarg, &node_set);
			break;
		default:
			printf("c: %c\n", c);
			help(argv[0]);
			return;
		}
	}

	switch(action){
	case DEL:
		if(node==-1){
			printf("You cannot remove all the nodes, use the shutdown mode\n");
			return;
		}

		krg_nodes_remove(&node_set);
		break;

	case ADD:{
		if(node == -1){
			printf("TODO: add all the available nodes\n");
		}

		krg_nodes_add(&node_set);
		break;
	}

	case POWEROFF:{
		if(node == -1){
			printf("You can't poweroff all the node in the same time!!!\n");
			break;
		}

		krg_nodes_poweroff(&node_set);
		break;
	}

	case REBOOT:
	case FAIL:{
		if(node == -1){
			printf("You can't fail all the node in the same time!!!\n");
			break;
		}

		krg_nodes_fail(&node_set);
		break;
	}

	case SWAP:
		break;
	default:
		help(argv[0]);
		return;
	}

}

int main(int argc, char* argv[])
{
  char **arg;
  int count;

  arg = argv;
  count = argc;

  if(argc==1){
    help(argv[0]);
    exit(EXIT_SUCCESS);
  }

  opterr = 0;

  count--;
  arg++;

  if (!strcmp(argv[1], "cluster"))
    cluster(count-1, arg+1);
  else if (!strcmp(argv[1], "nodes"))
    nodes(count-1, arg+1);
  else
    help(argv[0]);

  return EXIT_SUCCESS;
}
