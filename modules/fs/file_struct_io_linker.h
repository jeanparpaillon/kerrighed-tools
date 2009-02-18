/** DVFS level 3 - File Struct Linker.
 *  @file file_struct_io_linker.h
 *
 *  @author Renaud Lottiaux
 */

#ifndef __DVFS_FILE_STRUCT_LINKER__
#define __DVFS_FILE_STRUCT_LINKER__



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



typedef struct dvfs_file_struct {
	loff_t f_pos;
	int count;
	struct file *file;
} dvfs_file_struct_t ;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                            EXTERN VARIABLES                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/



extern struct iolinker_struct dvfs_file_struct_io_linker;
extern struct kmem_cache *dvfs_file_cachep;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/



#endif // __FILE_STRUCT_LINKER__
