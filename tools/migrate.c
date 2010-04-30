/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (c) 2010, Kerlabs
 *
 * Authors:
 *    Jean Parpaillon <jean.parpaillon@kerlabs.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>

#include <kerrighed.h>
#include <config.h>

int pid, nodeid ;

void version(char * program_name)
{
	printf("\
%s %s\n\
Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.\n\
Copyright (C) 2010 Kerlabs.\n\
This is free software; see source for copying conditions. There is NO\n\
warranty; not even for MERCHANBILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n", program_name, VERSION);
}

void help(char * program_name)
{
	printf("\
Usage: %s [-h|--help] [-v|--version] <pid> <nodeid>\n",
	       program_name);
}

void parse_args(int argc, char *argv[])
{
	char c;
	int option_index =0;
	char * short_options = "hv";
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, short_options,
			  long_options, &option_index)) != -1) {
		switch (c) {
		case 'h':
			help(argv[0]);
			exit(EXIT_SUCCESS);
		case 'v':
			version(argv[0]);
			exit(EXIT_SUCCESS);
		default:
			help(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (optind != argc - 2) {
		help(argv[0]);
		exit(EXIT_FAILURE);
	}
	pid = atoi(argv[optind++]);
	nodeid = atoi(argv[optind++]);
}

int main(int argc, char *argv[])
{
	int r ;

	parse_args(argc, argv);

	r = migrate (pid, nodeid);

	if (r != 0) {
	    perror("migrate");
	    return 1 ;
	}

	return 0;
}
