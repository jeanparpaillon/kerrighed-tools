/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2010, Kerlabs
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <errno.h>

#include <types.h>
#include <kerrighed.h>
#include <version.h>

int check_abi_version(void)
{
	int ret, fd;
	char version[256];

	fd = open("/sys/kerrighed/abi", O_RDONLY);
	if (fd == -1)
		return -1;

	ret = read(fd, version, 256);
	if (ret < 2) {
		ret = -1;
		goto err_close;
	}

	ret = strncmp(version, KERRIGHED_ABI, ret-1);
	if (ret)
		ret = -1;

err_close:
	close(fd);
	return ret;
}

void __attribute__ ((constructor)) init_krg_lib(void)
{
	int ret = check_abi_version();
	if (ret) {
		fprintf(stderr,
			"kerrighed: tools and kernel version mismatch\n");
		exit(EXIT_FAILURE);
	}
}

/** open kerrighed services
 * @author David Margery
 * @return the file descriptor of the openned kerrighed service, -1 if failure
 */
int open_kerrighed_services(void)
{
	return open("/proc/kerrighed/services", O_RDONLY);
}

/** close kerrighed services
 * @author David Margery
 * @param fd : the file descriptor returned when the kerrighed services where opened
 */
void close_kerrighed_services(int fd)
{
	close(fd) ;
}

/** call a kerrighed service that was allready opened
 * @author David Margery
 * @param fd : the file descriptor returned when the kerrighed services where opened
 * @param service_id : the service called
 * @param data : the data needed by the service
 */
int call_opened_kerrighed_services(int fd, int service_id, void *data)
{
	return ioctl(fd, service_id, data);
}

/** Call a Kerrighed kernel service through «/proc/kerrighed/services».
 *  @author Renaud Lottiaux
 *
 *  @param service_id  Identifier of the service to call.
 *  @param data        Data to give to the called service.
 *  @return            The data returned by the called service.
 */
int call_kerrighed_services(int service_id, void * data)
{
	int fd;
	int res;

	fd = open_kerrighed_services();
	if (fd == -1)
		return -1;

	res = call_opened_kerrighed_services(fd, service_id, data);

	close_kerrighed_services(fd);

	return res;
}

