#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sched.h>
#include <linux/sched.h>
#include <hotplug.h>

int ns_clone_flags = 0;

void print_usage(const char *name)
{
	fprintf(stderr,
		"Usage: %s [-uimpnU] [--] <container_bootstrop_helper> [<init_arg1> ...]\n"
		"Options:\n"
		" -u  Isolate UTSname namespace\n"
		" -i  Isolate IPC namespace\n"
		" -m  Isolate Mount namespace\n"
		" -p  Isolate PID namespace\n"
		" -n  Isolate Net namespace\n"
		" -U  Isolate User namespace\n",
		name);
}

int get_config(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "uimpnU")) != -1) {
		switch (opt) {
		case 'u':
			ns_clone_flags |= CLONE_NEWUTS;
			break;
		case 'i':
			ns_clone_flags |= CLONE_NEWIPC;
			break;
		case 'm':
			ns_clone_flags |= CLONE_NEWNS;
			break;
		case 'p':
			ns_clone_flags |= CLONE_NEWPID;
			break;
		case 'n':
			ns_clone_flags |= CLONE_NEWNET;
			break;
		case 'U':
			ns_clone_flags |= CLONE_NEWUSER;
			break;
		default:
			print_usage(argv[0]);
			exit(1);
		}
	}

	return optind;
}

int do_exec_helper(void *arg)
{
	char **argv = arg;
	int ret;

	ret = execvp(argv[0], argv);
	fprintf(stderr, "execvp(%s, ...) failed! %s\n",
		argv[0], strerror(errno));
	exit(1);
	/* not reached */
	return 0;
}

void do_clone_helper(char *argv[])
{
	void *stack;
	size_t stack_size;
	int ret;

	stack_size = sysconf(_SC_PAGESIZE);
	stack = alloca(stack_size) + stack_size;
	ret = clone(do_exec_helper, stack, ns_clone_flags|SIGCHLD, argv);
	if (ret < 0) {
		fprintf(stderr, "clone(...) failed! %s\n", strerror(errno));
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	int exec_ind = 1;
	int ret;

	exec_ind = get_config(argc, argv);
	if (exec_ind == argc) {
		print_usage(argv[0]);
		exit(1);
	}

	ret = krg_set_cluster_creator(1);
	if (ret) {
		fprintf(stderr,
			"%s krg_set_clutser_creator(1) failed! %s\n",
			argv[0], strerror(errno));
		exit(1);
	}

	if (ns_clone_flags)
		do_clone_helper(argv + exec_ind);
	else
		do_exec_helper(argv + exec_ind);

	return 0;
}
