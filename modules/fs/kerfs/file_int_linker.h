/** Container File interface linker.
 *  @file file_int_linker.h
 *
 *  Link containers and open files.
 *  @author Renaud Lottiaux
 */

#ifndef __FILE_INT_LINKER__
#define __FILE_INT_LINKER__



#include <ghost/ghost.h>
#include <ctnr/container.h>



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                 MACROS                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                            EXTERN VARIABLES                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/** Link a file to a container
 *  @author Renaud Lottiaux
 *
 *  @param file         File to link to the container.
 *  @param ctnrId       Id of the container to link.
 *
 *  @return   0 If everything OK,
 *            Negative value otherwise.
 */
int link_file_to_ctnr (struct file **file, struct task_struct *tsk);

void file_int_init (void);
void file_int_finalize (void);


#endif // __FILE_INT_LINKER__
