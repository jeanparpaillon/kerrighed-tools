/** Application management of object(s) shared by several processes
 *  @author Matthieu Fertré
 */

#ifndef __APPLICATION_SHARED_H__
#define __APPLICATION_SHARED_H__

#include <epm/application/application.h>
#include <ghost/ghost_types.h>

/*--------------------------------------------------------------------------*/

void clear_shared_objects(struct app_struct *app);

void destroy_shared_objects(struct app_struct *app,
			    struct task_struct *fake);

int global_chkpt_shared(struct rpc_desc *desc,
			struct app_kddm_object *obj);

int local_chkpt_shared(struct rpc_desc *desc,
		       struct app_struct *app,
		       int chkpt_sn,
		       credentials_t *user_creds);

int global_restart_shared(struct rpc_desc *desc,
			  struct app_kddm_object *obj);

int local_restart_shared(struct rpc_desc *desc,
			 struct app_struct *app,
			 struct task_struct *fake,
			 int chkpt_sn,
			 credentials_t *user_creds);

int local_restart_shared_complete(struct app_struct *app,
				  struct task_struct *fake);

/*--------------------------------------------------------------------------*/

enum shared_obj_type {
	/* file descriptors */
	REGULAR_FILE,
	REGULAR_DVFS_FILE,
	FAF_FILE,

	/* other objects */
	FILES_STRUCT,
	FS_STRUCT,
	MM_STRUCT,
	SEMUNDO_LIST,
	SIGHAND_STRUCT,
	SIGNAL_STRUCT,
	NO_OBJ
};

struct file_export_args {
	int index;
	struct file *file;
};

union export_args {
	struct file_export_args file_args;
};

int add_to_shared_objects_list(struct rb_root *root,
			       enum shared_obj_type type,
			       unsigned long key,
			       int is_local,
			       struct task_struct* exporting_task,
			       union export_args *args);

void *get_imported_shared_object(struct rb_root *root,
				  enum shared_obj_type type,
				  unsigned long key);

struct shared_object_operations {
	size_t restart_data_size;

	int (*export_now) (struct epm_action *, ghost_t *, struct task_struct *,
			   union export_args *);
	int (*import_now) (struct epm_action *, ghost_t *, struct task_struct *,
			   void  **);
	int (*import_complete) (struct task_struct *, void *);
	int (*delete) (struct task_struct *, void *);
};

#endif
