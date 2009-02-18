/** Kerrighed Open File Access Forwarding System.
 *  @file faf.h
 *
 *  @author Renaud Lottiaux
 */

#ifndef __FAF__
#define __FAF__

#include <ghost/ghost_types.h>
#include <fs/file_struct_io_linker.h>

struct epm_action;


/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                 MACROS                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



#define FAF_HASH_TABLE_SIZE 1024



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



typedef struct faf_client_data {
	kerrighed_node_t server_id;
	int server_fd;
	unsigned int flags;
	mode_t mode;
	loff_t pos;
	wait_queue_head_t poll_wq;
	unsigned int poll_revents;
} faf_client_data_t;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/



void faf_init (void);
void faf_finalize (void);

void check_activate_faf (struct task_struct *tsk, int index, struct file *file,
			 struct epm_action *action);

int setup_faf_file(struct file *file);

void check_last_faf_client_close(struct file *file,
				 struct dvfs_file_struct *dvfs_file);
void check_close_faf_srv_file(struct file *file);
void free_faf_file_private_data(struct file *file);

#endif // __FAF__
