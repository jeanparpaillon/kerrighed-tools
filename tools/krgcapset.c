/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (c) 2008-2010 Jean Parpaillon <jean.parpaillon@kerlabs.com>
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>

#include <kerrighed.h>
#include <config.h>

#define CAP_DEF(cap) [CAP_##cap] = #cap

const char * cap_text[CAP_SIZE] = {
	CAP_DEF(CHANGE_KERRIGHED_CAP),
	CAP_DEF(CAN_MIGRATE),
	CAP_DEF(DISTANT_FORK),
	CAP_DEF(FORK_DELAY),
	CAP_DEF(CHECKPOINTABLE),
	CAP_DEF(USE_REMOTE_MEMORY),
	CAP_DEF(USE_INTRA_CLUSTER_KERSTREAMS),
	CAP_DEF(USE_INTER_CLUSTER_KERSTREAMS),
	CAP_DEF(USE_WORLD_VISIBLE_KERSTREAMS),
	CAP_DEF(SEE_LOCAL_PROC_STAT),
	CAP_DEF(DEBUG),
	CAP_DEF(SYSCALL_EXIT_HOOK)
};

static struct option long_options[] = {
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{"show", no_argument, NULL, 's'},
	{"force", no_argument, NULL, 'f'},
	{"pid", required_argument, NULL, 'k'},
	{"permitted", required_argument, NULL, 'p'},
	{"effective", required_argument, NULL, 'e'},
	{"inheritable-permitted", required_argument, NULL, 'i'},
	{"inheritable-effective", required_argument, NULL, 'd'},
	{NULL, 0, NULL, 0}
};

static int supported_caps;

void version(char * program_name)
{
	printf("\
%s %s\n\
Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF\n\
Copyright (C) 2010 Kerlabs.\n\
This is free software; see source for copying conditions. There is NO\n\
warranty; not even for MERCHANBILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n", program_name, VERSION);
}

int print_capability(int current_cap_vector)
{
	int i ;
	int char_printed_on_this_line = 8 ;
	int char_to_print = 0 ;
	int lines_printed = 0 ;
	for (i=0; i < CAP_SIZE ;i++) {
		if (cap_raised(current_cap_vector, i)) {
			char_to_print = strlen(cap_text[i])+2;
			if (char_printed_on_this_line+char_to_print < 80) {
				if (char_printed_on_this_line!=8) {
					printf(", ") ;
					char_to_print += 2 ;
				}
				else
					printf("\t") ;
			} else {
				printf("\n\t") ;
				lines_printed++ ;
				char_printed_on_this_line = 8 ;
			}
			printf("%s",cap_text[i]) ;
			char_printed_on_this_line+=char_to_print ;
		}
	}
	printf("\n") ;
	return 0 ;
}

void print_capabilities(const krg_cap_t *caps)
{
	printf("Permitted Capabilities: 0%o\n", krg_cap_getpermitted(caps));
	print_capability(krg_cap_getpermitted(caps));

	printf("Effective Capabilities: 0%o\n", krg_cap_geteffective(caps));
	print_capability(krg_cap_geteffective(caps));

	printf("Inheritable Permitted Capabilities: 0%o\n", krg_cap_getinheritable_permitted(caps));
	print_capability(krg_cap_getinheritable_permitted(caps));

	printf("Inheritable Effective Capabilities: 0%o\n", krg_cap_getinheritable_effective(caps));
	print_capability(krg_cap_getinheritable_effective(caps));
}

int construct_capability(char * description)
{
	int res = 0;
	int text_matched ;
	int i ;
	char ** next_token = &description ;
	char * current_token ;
	while (*next_token) {
		current_token = strsep (next_token, ",") ;
		text_matched = 0 ;
		for (i=0; i < CAP_SIZE ;i++) {
			if (!strncmp(cap_text[i],current_token, strlen(cap_text[i]))) {
				res |= 1<<i ;
				text_matched = 1 ;
				break ;
			}
		}
		if (!text_matched) {
			fprintf(stderr, "Unrecognized capability: %s\n", current_token) ;
			fprintf(stderr, "Authorized capability names are:\n") ;
			print_capability(supported_caps) ;
		}
	}
	return res ;
}

int update_capability(char* description, int oldcapability)
{
	int res, tmp ;
  
	switch (description[0]) {
	case '+' :
		tmp = construct_capability(&description[1]) ;
		res = tmp | oldcapability ;
		break ;
	case '-' :
		tmp = construct_capability(&description[1]) ;
		res = ~tmp & oldcapability ;
		break ;
	default:
		res = construct_capability(description) ;
	}
	return res ;
}

int usage(char * argv[])
{
	int res;

	printf("\
Usage: %s [-h|--help] [-v|--version]\n\
  or:  %s [-k|--pid <pid>] -s|--show\n\
  or:  %s [-f|--force] [-k|--pid <pid>] {SET {[+|-]CAPABILITY LIST | OCTAL VALUE}...}\n\
  -h, --help                   display this help and exit\n\
  -v, --version                display version informations and exit\n\
  -k, --pid <pid>              act on the task having pid <pid>\n\
                               (default: calling process)\n\
  -s, --show                   show the capabilities of the designated task\n\
\n\
Change capabilities of the designated task.\n\
 SET is one of:\n\
  -e, --effective              set up effective capabilities\n\
  -p, --permitted              set up permitted capabilities\n\
  -d, --inheritable-effective  set up inheritable effective capabilities\n\
  -i, --inheritable-permitted  set up inheritable permitted capabilities\n\
\n\
", argv[0], argv[0], argv[0]);

	res = krg_cap_get_supported(&supported_caps);
	if (res)
		printf("\
WARNING: Capability list not available. You may not be running a Kerrighed kernel.\n");
	else {
		printf("\
CAPABILITY LIST is a comma-separated list of capabilities amongst:\n");
		print_capability(supported_caps);
	}
	return 0;
}

int force = 0 ;
int changed_father_cap = 0 ;
int main (int argc, char * argv[])
{
	pid_t pid = -1 ;
	int c ;
	int res ;
	long int cap_value;
	char * ignored = NULL ;
	krg_cap_t initial_caps ;

	if (argc == 1) {
		usage(argv);
		exit(EXIT_SUCCESS);
	}

	/*
	 * First pass on args for -h or -v
	 */
	while (1) {
		c = getopt_long(argc, argv, "hvsfk:p:e:i:d:",
				long_options, NULL);

		if (c == -1)
			break;

		cap_value = 0;
		switch (c) {
		case 'v':
			version(argv[0]);
			exit(EXIT_SUCCESS);
		case 'h':
			usage(argv);
			exit(EXIT_SUCCESS);
		}
	}

	res = krg_cap_get_supported(&supported_caps);
	if (res) {
		fprintf(stderr, "Unable to get the list of supported caps.\n"
			"Is Kerrighed running?\n");
		exit(res);
	}
	if (~((1 << CAP_SIZE) - 1) & supported_caps)
		fprintf(stderr, "%s version is too old for the running kernel.\n"
			"Some capability names won't be recognized.\n",
			argv[0]);

	res = krg_father_capget(&initial_caps) ;
	if (res) {
		printf ("Unable to get my father's capabilities res: %d, errno: %d\n", res, errno) ;
		exit(res);
	}

	optind = 1;
	while (1) {
		c = getopt_long(argc, argv, "hvsfk:p:e:i:d:",
				long_options, NULL);

		if (c == -1)
			break;

		cap_value = 0;
		switch (c) {
		case 'f':
			force = 1 ;
			break;
		case 's':
			print_capabilities(&initial_caps);
			break;
		case 'e':
			cap_value = strtol(optarg, &ignored, 0);
			if (*ignored != '\0')
				cap_value = update_capability(optarg, initial_caps.krg_cap_effective);
			initial_caps.krg_cap_effective = cap_value;
			changed_father_cap = 1 ;
			break;
		case 'p':
			cap_value = strtol(optarg, &ignored, 0);
			if (*ignored != '\0')
				cap_value = update_capability(optarg, initial_caps.krg_cap_permitted);
			initial_caps.krg_cap_permitted = cap_value;
			changed_father_cap = 1 ;
			break;
		case 'i':
			cap_value = strtol(optarg, &ignored, 0);
			if (*ignored != '\0')
				cap_value = update_capability(optarg, initial_caps.krg_cap_inheritable_permitted);
			initial_caps.krg_cap_inheritable_permitted = cap_value;
			changed_father_cap = 1 ;
			break;
		case 'd':
			cap_value = strtol(optarg, &ignored, 0);
			if (*ignored != '\0')
				cap_value = update_capability(optarg, initial_caps.krg_cap_inheritable_effective);
			initial_caps.krg_cap_inheritable_effective = cap_value;
			changed_father_cap = 1 ;
			break;
		case 'k':
			pid = atoi (optarg) ;
			if (changed_father_cap) {
				printf("Conflict in implementation: -k must be the first option used\n") ;
				exit(EXIT_FAILURE) ;
			}
			res = krg_pid_capget(pid, &initial_caps) ;
			if (res) {
				printf ("Unable to get capabilities of process %d (res: %d, errno: %d))\n", pid, res, errno) ;
				exit(res) ;
			}
			break;
		default:
			usage(argv);
			exit(EXIT_FAILURE);
			break;
		}
	}

	if (changed_father_cap && (!force && (!cap_raised(initial_caps.krg_cap_permitted, CAP_CHANGE_KERRIGHED_CAP) ||
					      !cap_raised(initial_caps.krg_cap_effective, CAP_CHANGE_KERRIGHED_CAP) ))) {
		int answer ;
		int use_default = 1;
		printf("Are you sure you want to loose the capability to change your capabilities (y/N)\n") ;
		answer = getchar() ;
		while (answer != 'y' && answer != 'Y' && answer != 'n' && answer != 'N' && answer != EOF && answer != '\n') {
			use_default = 0;
			answer = getchar();
		}
		if (answer != 'y' && answer != 'Y' && (answer = '\n' || use_default) )
			exit(EXIT_SUCCESS);
	}
	if (changed_father_cap) {
		if (pid == -1) {
			res = krg_father_capset(&initial_caps) ;
			if (res)
				printf ("Unable to set my father's capabilities\n") ;
		}
		else {
			res = krg_pid_capset(pid, &initial_caps) ;
			if (res)
				printf ("Unable to set the capabilities of process %d (res is %d, errno %d)\n", pid, res, errno) ;
		}
	}
	exit(EXIT_SUCCESS);
}
