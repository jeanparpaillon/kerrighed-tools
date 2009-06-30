/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#include <stdio.h>
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



/*****************************************************************************/
/*                                                                           */
/*                              INTERNAL FUNCTIONS                           */
/*                                                                           */
/*****************************************************************************/



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

  fd = open("/proc/kerrighed/services", O_RDONLY);
  if (fd == -1)
    {
      close (fd);
      return -1;
    }
  
  res = ioctl(fd, service_id, data);

  close(fd);

  return res;
} 



/** open kerrighed services
 * @author David Margery
 * @return the file descriptor of the openned kerrighed service, -1 if failure
 */
int open_kerrighed_services() 
{

  return open("/proc/kerrighed/services", O_RDONLY);
}



/** close kerrighed services
 * @author David Margery
 * @param fd : the file descriptor returned when the kerrighed services where opened
 */
void close_kerrighed_services(int fd) 
{
  close ( fd ) ;
}



/** call a kerrighed service that was allready opened
 * @author David Margery
 * @param fd : the file descriptor returned when the kerrighed services where opened
 * @param service_id : the service called
 * @param data : the data needed by the service
 */ 
int call_opened_kerrighed_services(int fd, int service_id, void * data) 
{
  return ioctl(fd, service_id, data);
}
