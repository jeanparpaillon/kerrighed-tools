/*
 *  Kerrighed/modules/proc/children.c
 *
 *  Copyright (C) 2006-2007 Louis Rilling - Kerlabs
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/pid_namespace.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/rwsem.h>
#include <linux/kref.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <kerrighed/children.h>
#include <kerrighed/task.h>
#include <kerrighed/sched.h>
#include <kerrighed/pid.h>
#include <kerrighed/ptrace.h>
#include <kerrighed/krginit.h>

#include "debug_proc.h"

#define MODULE_NAME "children"

#include <rpc/rpc.h>
#include <hotplug/hotplug.h>
#include <ctnr/kddm.h>
#include "krg_exit.h"   /* For remote zombies handling */
#include "libproc.h"

struct remote_child {
	struct list_head sibling;
	struct list_head thread_group;
	pid_t pid;
	pid_t tgid;
	pid_t pgid;
	pid_t sid;
	pid_t parent;
	pid_t real_parent;
	int exit_signal;
	long exit_state;
	kerrighed_node_t node;
};

struct children_kddm_object {
	pid_t tgid;
	struct list_head children;
	unsigned long nr_children;
	unsigned long nr_threads;
	u32 self_exec_id;

	/* Remaining fields are not shared */
	struct rw_semaphore sem;
	int write_locked;

	int alive;
	struct kref kref;

	struct rcu_head rcu;
};

static struct kmem_cache *children_obj_cachep;
static struct kmem_cache *remote_child_cachep;
struct kddm_set;
static struct kddm_set *children_kddm_set;

static struct kmem_cache *krg_parent_head_cachep;
/** Size of krg_parent task struct list hash table.
 */
#define PROCESS_HASH_TABLE_SIZE 1024
static hashtable_t *krg_parent_table; /* list_head of local children */

/************************************************************************
 * Global children list of a task					*
 ************************************************************************/

static void kcb_children_get(struct children_kddm_object *obj)
{
	if (obj) {
		kref_get(&obj->kref);
		DEBUG(DBG_CHILDREN, 4, "%d count=%d\n",
		      obj->tgid, atomic_read(&obj->kref.refcount));
	}
}

static void children_free(struct children_kddm_object *obj)
{
	struct remote_child *child, *next;

	DEBUG(DBG_CHILDREN, 2, "%d\n", obj->tgid);

	list_for_each_entry_safe(child, next, &obj->children, sibling) {
		list_del(&child->sibling);
		kmem_cache_free(remote_child_cachep, child);
	}
	kmem_cache_free(children_obj_cachep, obj);
}

static void delayed_children_free(struct rcu_head *rhp)
{
	struct children_kddm_object *obj =
		container_of(rhp, struct children_kddm_object, rcu);
	children_free(obj);
}

static void children_free_rcu(struct kref *kref)
{
	struct children_kddm_object *obj =
		container_of(kref, struct children_kddm_object, kref);
	call_rcu(&obj->rcu, delayed_children_free);
}

static void kcb_children_put(struct children_kddm_object *obj)
{
	if (obj) {
		DEBUG(DBG_CHILDREN, 4, "%d count=%d\n",
		      obj->tgid, atomic_read(&obj->kref.refcount));
		kref_put(&obj->kref, children_free_rcu);
	}
}

static inline void remove_child_links(struct children_kddm_object *obj,
				      struct remote_child *child)
{
	list_del(&child->sibling);
	list_del(&child->thread_group);
}

static void set_child_links(struct children_kddm_object *obj,
			    struct remote_child *child)
{
	struct remote_child *item;

	INIT_LIST_HEAD(&child->thread_group);
	if (child->pid != child->tgid) {
		list_for_each_entry(item, &obj->children, sibling)
			if (item->tgid == child->tgid) {
				list_add_tail(&child->thread_group,
					      &item->thread_group);
				break;
			}
		BUG_ON(list_empty(&child->thread_group));
	}
	list_add_tail(&child->sibling, &obj->children);
}

static int children_alloc_object(struct kddm_obj *obj_entry,
				 struct kddm_set *set, objid_t objid)
{
	struct children_kddm_object *obj = NULL;
	pid_t tgid = objid;
	struct task_struct *task, *first;

	read_lock(&tasklist_lock);
	first = find_task_by_pid(tgid);
	if (first) {
		task = first;
		do {
			if (task->children_obj) {
				obj = task->children_obj;
				break;
			}
			task = next_thread(task);
		} while (task != first);
	}
	BUG_ON(obj);
	read_unlock(&tasklist_lock);

	if (likely(!obj))
		obj = kmem_cache_alloc(children_obj_cachep, GFP_KERNEL);
	if (unlikely(obj == NULL))
		return -ENOMEM;

	obj->tgid = objid;
	INIT_LIST_HEAD(&obj->children);
	obj->nr_children = 0;
	obj->nr_threads = 0;
	obj->self_exec_id = 0;
	init_rwsem(&obj->sem);
	obj->alive = 1;
	kref_init(&obj->kref);
	obj_entry->object = obj;

	DEBUG(DBG_CHILDREN, 2, "%d obj=0x%p\n", tgid, obj);

	return 0;
}

static int children_first_touch(struct kddm_obj *obj_entry,
				struct kddm_set *set, objid_t objid,int flags)
{
	return children_alloc_object(obj_entry, set, objid);
}

static int children_export_object(struct rpc_desc *desc,
				  struct kddm_obj *obj_entry)
{
	struct children_kddm_object *obj = obj_entry->object;
	struct remote_child *child;
	int retval = 0;

	BUG_ON(!obj);
	BUG_ON(!(obj->tgid & GLOBAL_PID_MASK));

	DEBUG(DBG_CHILDREN, 3, "%d\n", obj->tgid);

	retval = rpc_pack_type(desc, obj->nr_children);
	if (unlikely(retval))
		goto out;
	retval = rpc_pack_type(desc, obj->nr_threads);
	if (unlikely(retval))
		goto out;
	list_for_each_entry(child, &obj->children, sibling) {
		retval = rpc_pack_type(desc, *child);
		if (unlikely(retval))
			goto out;
	}

 out:
	return retval;
}

static int children_import_object(struct kddm_obj *obj_entry,
				  struct rpc_desc *desc)
{
	struct children_kddm_object *obj = obj_entry->object;
	struct remote_child *child, *next;
	typeof(obj->nr_children) nr_children;
	typeof(obj->nr_children) min_children;
	typeof(min_children) i;
	LIST_HEAD(children_head);
	int retval = 0;

	BUG_ON(!obj);
	BUG_ON(!(obj->tgid & GLOBAL_PID_MASK));

	DEBUG(DBG_CHILDREN, 3, "%d\n", obj->tgid);

	retval = rpc_unpack_type(desc, nr_children);
	if (unlikely(retval))
		goto out;
	retval = rpc_unpack_type(desc, obj->nr_threads);
	if (unlikely(retval))
		goto out;

	DEBUG(DBG_CHILDREN, 4, "existing=%lu, incoming=%lu\n",
	      obj->nr_children, nr_children);
	min_children = min(nr_children, obj->nr_children);

	/* Reuse allocated elements as much as possible */

	/* First, delete elements that won't be used anymore */
	i = 0;
	list_for_each_entry_safe(child, next, &obj->children, sibling) {
		if (i + min_children == obj->nr_children)
			break;
		remove_child_links(obj, child);
		kmem_cache_free(remote_child_cachep, child);
		i++;
	}
	BUG_ON(i + min_children != obj->nr_children);

	/* Second, fill in already allocated elements */
	i = 0;
	list_splice_init(&obj->children, &children_head);
	list_for_each_entry_safe(child, next, &children_head, sibling) {
		/* Does not need that child be linked to the obj->children
		 * list, but only to a list */
		remove_child_links(obj, child);
		retval = rpc_unpack_type(desc, *child);
		if (unlikely(retval))
			goto err_free_child;
		/* Put the child to the obj->children list */
		set_child_links(obj, child);
		DEBUG(DBG_CHILDREN, 4, "(existing) #%lu %d\n", i, child->pid);
		i++;
	}
	BUG_ON(i != min_children);

	/* Third, allocate, fill in, and add remaininig elements to import */
	for (; i < nr_children; i++) {
		child = kmem_cache_alloc(remote_child_cachep, GFP_KERNEL);
		if (unlikely(!child)) {
			retval = -ENOMEM;
			goto out;
		}
		retval = rpc_unpack_type(desc, *child);
		if (unlikely(retval))
			goto err_free_child;
		set_child_links(obj, child);
		DEBUG(DBG_CHILDREN, 4, "(added) #%lu %d\n", i, child->pid);
	}
	BUG_ON(i != nr_children);

	obj->nr_children = nr_children;

 out:
	DEBUG(DBG_CHILDREN, 3, "retval=%d\n", retval);
	return retval;

 err_free_child:
	kmem_cache_free(remote_child_cachep, child);
	goto out;
}

static int children_remove_object(void *object, struct kddm_set *set,
				  objid_t objid)
{
	struct children_kddm_object *obj;

	obj = object;
	BUG_ON(!obj);

	DEBUG(DBG_CHILDREN, 2, "%d\n", obj->tgid);

	obj->alive = 0;
	kcb_children_put(obj);

	return 0;
}

static struct iolinker_struct children_io_linker = {
	.linker_name   = "children ",
	.linker_id     = CHILDREN_LINKER,
	.alloc_object  = children_alloc_object,
	.first_touch   = children_first_touch,
	.export_object = children_export_object,
	.import_object = children_import_object,
	.remove_object = children_remove_object,
	.default_owner = global_pid_default_owner,
};

static struct children_kddm_object * kcb_children_readlock(pid_t tgid)
{
	struct children_kddm_object *obj;

	/* Filter well known cases of no children kddm object. */
	if (!(tgid & GLOBAL_PID_MASK))
		return NULL;

	obj = _kddm_get_object_no_ft(children_kddm_set, tgid);
	if (likely(obj)) {
		down_read(&obj->sem);
		/* Marker for unlock. Dirty but temporary. */
		obj->write_locked = 0;
		DEBUG(DBG_CHILDREN, 3, "%d down_read\n", tgid);
	}
	DEBUG(DBG_CHILDREN, 2, "%d 0x%p\n", tgid, obj);

	return obj;
}

static struct children_kddm_object * children_writelock(pid_t tgid, int nested)
{
	struct children_kddm_object *obj;

	/* Filter well known cases of no children kddm object. */
	if (!(tgid & GLOBAL_PID_MASK))
		return NULL;

	obj = _kddm_grab_object_no_ft(children_kddm_set, tgid);
	if (likely(obj)) {
		if (!nested)
			down_write(&obj->sem);
		else
			down_write_nested(&obj->sem, SINGLE_DEPTH_NESTING);
		/* Marker for unlock. Dirty but temporary. */
		obj->write_locked = 1;
		DEBUG(DBG_CHILDREN, 3, "%d down_write\n", tgid);
	}
	DEBUG(DBG_CHILDREN, 2, "%d 0x%p\n", tgid, obj);

	return obj;
}

static struct children_kddm_object * kcb_children_writelock(pid_t tgid)
{
	return children_writelock(tgid, 0);
}

static struct children_kddm_object * kcb_children_writelock_nested(pid_t tgid)
{
	return children_writelock(tgid, 1);
}

static struct children_kddm_object * children_create_writelock(pid_t tgid)
{
	struct children_kddm_object *obj;

	BUG_ON(!(tgid & GLOBAL_PID_MASK));

	obj = _kddm_grab_object(children_kddm_set, tgid);
	if (likely(obj)) {
		down_write(&obj->sem);
		/* Marker for unlock. Dirty but temporary. */
		obj->write_locked = 1;
		DEBUG(DBG_CHILDREN, 3, "%d down_write\n", tgid);
	}
	DEBUG(DBG_CHILDREN, 2, "%d 0x%p\n", tgid, obj);

	return obj;
}

static void kcb_children_unlock(pid_t tgid)
{
	/* Filter well known cases of no children kddm object. */
	if (!(tgid & GLOBAL_PID_MASK))
		return;

	DEBUG(DBG_CHILDREN, 2, "%d\n", tgid);

	{
		/* Dirty tricks here. Hopefully it should be temporary waiting
		 * for kddm to implement locking on a task basis. */
		struct children_kddm_object *obj;

		obj = _kddm_find_object(children_kddm_set, tgid);
		DEBUG(DBG_CHILDREN, 3, "%d 0x%p\n", tgid, obj);
		if (likely(obj)) {
			_kddm_put_object(children_kddm_set, tgid);
			if (obj->write_locked) {
				DEBUG(DBG_CHILDREN, 3, "%d up_write\n", tgid);
				up_write(&obj->sem);
			} else {
				DEBUG(DBG_CHILDREN, 3, "%d up_read\n", tgid);
				up_read(&obj->sem);
			}
		}
	}
	_kddm_put_object(children_kddm_set, tgid);
}

static
struct children_kddm_object * kcb_alloc_children(struct task_struct *task)
{
	/* Filter well known cases of no children kddm object. */
	BUG_ON(!(task->tgid & GLOBAL_PID_MASK));

	DEBUG(DBG_CHILDREN, 1, "%d\n", task->tgid);

	task->children_obj = children_create_writelock(task->tgid);
	if (likely(task->children_obj)) {
		task->children_obj->nr_threads = 1;
		task->children_obj->self_exec_id = task->self_exec_id;
	}
	kcb_children_unlock(task->tgid);

	return task->children_obj;
}

static void free_children(struct task_struct *task)
{
	struct children_kddm_object *obj = task->children_obj;

	/* Filter well known cases of no children kddm object. */
	BUG_ON(!(task->tgid & GLOBAL_PID_MASK));

	DEBUG(DBG_CHILDREN, 1, "%d\n", task->tgid);

	BUG_ON(!obj);
	BUG_ON(!list_empty(&obj->children));
	BUG_ON(obj->nr_threads);

	rcu_assign_pointer(task->children_obj, NULL);

	up_write(&obj->sem);
	_kddm_remove_frozen_object(children_kddm_set, task->tgid);
}

void kcb___share_children(struct task_struct *task)
{
	struct children_kddm_object *obj = task->children_obj;
	DEBUG(DBG_CHILDREN, 1, "%d by %d\n", obj->tgid, task->pid);
	obj->nr_threads++;
}

static void kcb_share_children(struct task_struct *task)
{
	struct children_kddm_object *obj;

	obj = kcb_children_writelock(task->tgid);
	BUG_ON(!obj);
	BUG_ON(obj != task->children_obj);
	kcb___share_children(task);
	kcb_children_unlock(task->tgid);

}

/* Must be called under kcb_children_writelock
 */
static void kcb_exit_children(struct task_struct *task)
{
	int free;

	DEBUG(DBG_CHILDREN, 1, "%d\n", task->children_obj->tgid);

	free = !(--task->children_obj->nr_threads);
	if (free)
		free_children(task);
	else {
		rcu_assign_pointer(task->children_obj, NULL);
		kh_children_unlock(task->tgid);
	}
}

static int kcb_children_alive(struct children_kddm_object *obj)
{
	return obj && obj->alive;
}

static int kcb_new_child(struct children_kddm_object *obj,
			 pid_t parent_pid,
			 pid_t child_pid, pid_t child_tgid,
			 pid_t pgid, pid_t sid,
			 int exit_signal)
{
	struct remote_child *item;

	if (!obj)
		return 0;
	BUG_ON(parent_pid == 1);

	DEBUG(DBG_CHILDREN, 2, "%d welcomes %d\n", obj->tgid, child_pid);

	item = kmem_cache_alloc(remote_child_cachep, GFP_ATOMIC);
	if (!item)
		return -ENOMEM;

	item->pid = child_pid;
	item->tgid = child_tgid;
	item->pgid = pgid;
	item->sid = sid;
	item->parent = item->real_parent = parent_pid;
	item->exit_signal = exit_signal;
	item->exit_state = 0;
	item->node = kerrighed_node_id;
	set_child_links(obj, item);
	obj->nr_children++;

	return 0;
}

/* Expects obj write locked
 */
static void kcb_set_child_pgid(struct children_kddm_object *obj,
			       pid_t pid, pid_t pgid)
{
	struct remote_child *item;

	if (unlikely(!obj))
		return;

	list_for_each_entry(item, &obj->children, sibling)
		if (item->pid == pid) {
			item->pgid = pgid;
			break;
		}
}

/* Expects obj write locked
 */
static void kcb_set_child_exit_signal(struct children_kddm_object *obj,
				      pid_t pid, int exit_signal)
{
	struct remote_child *item;

	if (unlikely(!obj))
		return;

	list_for_each_entry(item, &obj->children, sibling)
		if (item->pid == pid) {
			item->exit_signal = exit_signal;
			break;
		}
}

/* Expects obj write locked
 */
static void kcb_set_child_exit_state(struct children_kddm_object *obj,
				     pid_t pid, long exit_state)
{
	struct remote_child *item;

	if (unlikely(!obj))
		return;

	list_for_each_entry(item, &obj->children, sibling)
		if (item->pid == pid) {
			item->exit_state = exit_state;
			break;
		}
}

static void kcb_set_child_location(struct children_kddm_object *obj,
				   pid_t pid, kerrighed_node_t node)
{
	struct remote_child *item;

	if (unlikely(!obj))
		return;

	list_for_each_entry(item, &obj->children, sibling)
		if (item->pid == pid) {
			item->node = node;
			break;
		}
}

/* Expects obj write locked
 */
static void remove_child(struct children_kddm_object *obj,
			 struct remote_child *child)
{
	DEBUG(DBG_CHILDREN, 2, "%d abandons %d\n", obj->tgid, child->pid);

	remove_child_links(obj, child);
	kmem_cache_free(remote_child_cachep, child);
	obj->nr_children--;
}

static void reparent_child(struct children_kddm_object *obj,
			   struct remote_child *child,
			   pid_t reaper_pid, int same_group)
{
	/* A child can be reparented:
	 * either to another thread of the same thread group,
	 * or to its child reaper -> local child reaper
	 */

	BUG_ON(child->parent == reaper_pid);
	if (!same_group)
		/* Local child reaper doesn't need a children
		 * kddm object
		 */
		/* TODO: Is it true with PID namespaces? */
		remove_child(obj, child);
	else {
		BUG_ON(!(reaper_pid & GLOBAL_PID_MASK));
		/* For ptraced children, child->parent was already wrong since
		 * it is not assigned when ptrace-attaching. So keep it wrong
		 * the same way. */
		child->parent = child->real_parent = reaper_pid;
	}
}

/* Expects obj write locked
 */
static void kcb_reparent_child(struct children_kddm_object *obj,
			       pid_t child_pid,
			       pid_t reaper_pid, int same_group)
{
	struct remote_child *item;

	if (unlikely(!obj))
		return;

	list_for_each_entry(item, &obj->children, sibling)
		if (item->pid == child_pid) {
			reparent_child(obj, item, reaper_pid, same_group);
			break;
		}
}

/* Expects parent->children_obj write locked
 * and tasklist_lock write locked
 */
static void kcb_forget_original_remote_parent(struct task_struct *parent,
					      struct task_struct *reaper)
{
	int threaded_reparent = reaper->group_leader == parent->group_leader;
	struct children_kddm_object *obj = parent->children_obj;
	struct remote_child *child, *tmp_child;

	if (!obj)
		return;

	list_for_each_entry_safe(child, tmp_child, &obj->children, sibling)
		if (child->real_parent == parent->pid) {
			if (!threaded_reparent
			    && child->exit_state == EXIT_ZOMBIE
			    && child->node != kerrighed_node_id)
				/* Have it reaped by its local child reaper */
				/* Asynchronous */
				notify_remote_child_reaper(child->pid,
							   child->node);
			reparent_child(obj, child,
				       reaper->pid, threaded_reparent);
		}
}

/* Expects obj write locked
 */
static void kcb_ptrace_link(struct children_kddm_object *obj,
			    pid_t ptracer_pid,
			    pid_t child_pid)
{
	if (!obj)
		return;
	PANIC("Not supported yet!");
}

/* Expects obj write locked
 */
static void kcb_ptrace_unlink(struct children_kddm_object *obj, pid_t child_pid)
{
	if (!obj)
		return;
	PANIC("Not supported yet!");
}

/* Expects obj write locked
 */
static void kcb_remove_child(struct children_kddm_object *obj,
			     pid_t child_pid)
{
	struct remote_child *item;

	if (!obj)
		return;

	list_for_each_entry(item, &obj->children, sibling)
		if (item->pid == child_pid) {
			remove_child(obj, item);
			break;
		}
}

/* Expects obj at least read locked
 */
static int is_child(struct children_kddm_object *obj, pid_t pid)
{
	struct remote_child *item;
	int retval = 0;

	if (!obj)
		return 0;

	list_for_each_entry(item, &obj->children, sibling)
		if (item->pid == pid) {
			retval = 1;
			break;
		}

	return retval;
}

/* Expects obj locked
 */
static int krg_eligible_child(struct children_kddm_object *obj,
			      struct remote_child *child,
			      pid_t pid,
			      int options)
{
	int retval = 0;

	if (pid > 0) {
		if (child->pid != pid)
			goto out;
	} else if (!pid) {
		if (child->pgid != process_group(current))
			goto out;
	} else if (pid != -1) {
		if (child->pgid != -pid)
			goto out;
	}

	if (child->exit_signal == -1)
		/* Vanilla Linux also requires that child is not ptraced
		 * before rejecting it, but this is already done in
		 * eligible_child since ptraced children are only
		 * local. */
		goto out;

	if (((child->exit_signal != SIGCHLD)
	     ^ ((options & __WCLONE) != 0))
	    && !(options & __WALL))
		goto out;

	if ((options & __WNOTHREAD)
	    && child->parent != current->pid)
		goto out;

	retval = 1;

	/* This is the test for delayed group leader. */
	if (child->pid == child->tgid
	    && !list_empty(&child->thread_group))
		retval = 2;

 out:
	return retval;
}

/* Expects obj locked. Releases obj lock.
 *
 * @return	pid (> 1) of task reaped, if any (init cannot be reaped), or
 *		1 if some tasks could be reaped in the future, or
 *		0 if no task is expected to be reapable even in the future, or
 *		negative error code if an error occurs when
 *			reaping a task (do_wait should abort)
 */
static int kcb_do_wait(struct children_kddm_object *obj,
		       pid_t pid,  int options, struct siginfo __user *infop,
		       int __user *stat_addr, struct rusage __user *ru)
{
	struct remote_child *item;
	int retval = 0;
	int ret;

	if (!obj)
		goto out_unlock;

	list_for_each_entry(item, &obj->children, sibling) {
		ret = krg_eligible_child(obj, item, pid, options);
		if (!ret) {
			if (pid > 0 && item->pid == pid)
				break;
			continue;
		}

		/* item->exit_state should not reach EXIT_DEAD since this can
		 * only happen when a thread self-reaps, and the thread is
		 * removed from its parent's children object before releasing
		 * the lock on it. */
		BUG_ON(item->exit_state == EXIT_DEAD);
		if (item->exit_state == EXIT_ZOMBIE) {
			if (ret == 2)
				goto check_continued;
			if (!likely(options & WEXITED))
				continue;
			kcb_children_unlock(current->tgid);
			ret = krg_wait_task_zombie(item->pid, item->node,
						   (options & WNOWAIT),
						   infop, stat_addr, ru);
			if (ret)
				retval = ret;
			goto out;
		}

	check_continued:
		/* Check for continued task is not implemented right now. */
		retval = 1;
	}

 out_unlock:
	kcb_children_unlock(current->tgid);
 out:
	return retval;
}

static void kcb_update_self_exec_id(struct task_struct *task)
{
	struct children_kddm_object *obj;

	obj = kcb_children_writelock(task->tgid);
	BUG_ON(!obj);
	obj->self_exec_id = task->self_exec_id;
	kcb_children_unlock(task->tgid);
}

static u32 kcb_get_real_parent_self_exec_id(struct children_kddm_object *obj)
{
	return obj->self_exec_id;
}

/* Must be called under rcu_read_lock()
 */
static pid_t kcb_get_real_parent_tgid(struct task_struct *task)
{
	pid_t real_parent_tgid;
	struct task_struct *real_parent = rcu_dereference(task->real_parent);

	if (!pid_alive(task))
		return 0;

	if (real_parent != baby_sitter)
		real_parent_tgid = real_parent->tgid;
	else {
		struct task_kddm_object *task_obj =
			rcu_dereference(task->task_obj);
		struct children_kddm_object *parent_children_obj =
			rcu_dereference(task->parent_children_obj);

		if (task_obj && kcb_children_alive(parent_children_obj))
			real_parent_tgid = task_obj->real_parent_tgid;
		else
			/* TODO: will child_reaper remain always
			 * safe? */
			real_parent_tgid = child_reaper(task)->tgid;
	}

	return real_parent_tgid;
}

/* Expects obj locked
 */
static int kcb_get_parent(struct children_kddm_object *obj, pid_t pid,
			  pid_t *parent_pid, pid_t *real_parent_pid)
{
	struct remote_child *item;
	int retval = -ESRCH;

	if (!obj)
		goto out;

	list_for_each_entry(item, &obj->children, sibling)
		if (item->pid == pid) {
			*parent_pid = item->parent;
			*real_parent_pid = item->real_parent;
			retval = 0;
			goto out;
		}

 out:
	return retval;
}

static struct children_kddm_object * kcb_parent_children_writelock(
	struct task_struct *task,
	pid_t *parent_tgid)
{
	struct children_kddm_object *obj;
	pid_t tgid;

	rcu_read_lock();
	tgid = kcb_get_real_parent_tgid(task);
	obj = rcu_dereference(task->parent_children_obj);
	rcu_read_unlock();
	BUG_ON(!tgid);
	if (!obj || tgid == child_reaper(task)->tgid) {
		DEBUG(DBG_CHILDREN, 2, "%d no parent_children_obj\n",
		      task->pid);
		obj = NULL;
		goto out;
	}

	DEBUG(DBG_CHILDREN, 2, "try %d -> %d\n", task->pid, tgid);

	obj = kcb_children_writelock(tgid);
	/* Check that thread group tgid is really the parent of task.
	 * If not, unlock obj immediately, and return NULL.
	 *
	 * is_child may also return 0 if task's parent is init. In that case, it
	 * is still correct to return NULL as long as parent_tgid is set.
	 */
	if (!is_child(obj, task->pid)) {
		kcb_children_unlock(tgid);
		obj = NULL;
		tgid = child_reaper(task)->tgid;
		goto out;
	}

 out:
	DEBUG(DBG_CHILDREN, 2, "%d -> %d\n", task->pid, tgid);
	*parent_tgid = tgid;
	return obj;
}

static struct children_kddm_object * kcb_parent_children_readlock(
	struct task_struct *task,
	pid_t *parent_tgid)
{
	struct children_kddm_object *obj;
	pid_t tgid;

	rcu_read_lock();
	tgid = kcb_get_real_parent_tgid(task);
	obj = rcu_dereference(task->parent_children_obj);
	rcu_read_unlock();
	BUG_ON(!tgid);
	if (!obj || tgid == child_reaper(task)->tgid) {
		DEBUG(DBG_CHILDREN, 2, "%d no parent_children_obj\n",
		      task->pid);
		obj = NULL;
		goto out;
	}

	DEBUG(DBG_CHILDREN, 2, "try %d -> %d\n", task->pid, tgid);

	obj = kcb_children_readlock(tgid);
	/* Check that thread group tgid is really the parent of task.
	 * If not, unlock obj immediately, and return NULL.
	 *
	 * is_child may also return 0 if task's parent is init. In that case, it
	 * is still correct to return NULL as long as parent_tgid is set.
	 */
	if (!is_child(obj, task->pid)) {
		kcb_children_unlock(tgid);
		obj = NULL;
		tgid = child_reaper(task)->tgid;
		goto out;
	}

 out:
	DEBUG(DBG_CHILDREN, 2, "%d -> %d\n", task->pid, tgid);
	*parent_tgid = tgid;
	return obj;
}

/**
 * @author Louis Rilling
 *
 * Called from init_prekerrighed_process, assumes read lock on tasklist
 */
int kcb_fill_children_kddm_object(struct task_struct *task)
{
	pid_t tgid = task->tgid;
	struct children_kddm_object *obj;
	struct task_struct *child;
	int retval = 0;

	/* Exclude kernel treads and local pids from using children kddm set. */
	if (!(tgid & GLOBAL_PID_MASK ) || !task->mm)
		return 0;

	BUG_ON(task->children_obj);
	if (!task->exit_state)
		obj = children_create_writelock(tgid);
	else
		/* Task is already dead and doesn't need a children kddm
		 * object */
		return 0;

	task->children_obj = obj;
	BUG_ON(!task->children_obj);
	obj->nr_threads++;
	obj->self_exec_id = task->self_exec_id;

	/* Need more work to support ptrace
	 */
	list_for_each_entry(child, &task->children, sibling) {
		if ((retval = kcb_new_child(obj, task->pid,
					    child->pid,
					    child->tgid,
					    process_group(child),
					    process_session(child),
					    child->exit_signal)))
			break;
		kcb_children_get(obj);
		rcu_assign_pointer(child->parent_children_obj, obj);
		if (child->exit_state)
			kcb_set_child_exit_state(obj, child->pid,
						 child->exit_state);
	}

	kcb_children_unlock(tgid);

	return retval;
}

/************************************************************************
 * Local children list of a task					*
 ************************************************************************/

static inline struct list_head * new_krg_parent_entry(pid_t key)
{
	struct list_head *entry;

	entry = kmem_cache_alloc(krg_parent_head_cachep, GFP_ATOMIC);
	if (!entry)
		return NULL;

	INIT_LIST_HEAD(entry);
	__hashtable_add(krg_parent_table, key, entry);

	return entry;
}

static inline struct list_head * get_krg_parent_entry(pid_t key)
{
	return __hashtable_find(krg_parent_table, key);
}

static inline void delete_krg_parent_entry(pid_t key)
{
	struct list_head *entry;

	entry = __hashtable_find(krg_parent_table, key);
	BUG_ON(!entry);
	BUG_ON(!list_empty(entry));

	__hashtable_remove(krg_parent_table, key);
	kmem_cache_free(krg_parent_head_cachep, entry);
}

static inline void add_to_krg_parent(struct task_struct *tsk, pid_t parent_pid)
{
	struct list_head *children;

	children = get_krg_parent_entry(parent_pid);
	if (children == NULL) {
		children = new_krg_parent_entry(parent_pid);
		if (!children)
			OOM;
	}
	list_add_tail(&tsk->sibling, children);
}

static inline void remove_from_krg_parent(struct task_struct *tsk,
					  pid_t parent_pid)
{
	struct list_head *children;

	children = get_krg_parent_entry(parent_pid);
	BUG_ON(!children);

	list_del(&tsk->sibling);
	if (list_empty(children))
		delete_krg_parent_entry(parent_pid);
}

/* Used in two cases:
 * 1/ When child considers its parent as remote, and this parent is now local
 *    -> link directly in parent's children list
 * 2/ When child considers its parent as local, and parent is leaving the node
 *    -> unlink child from any process children list,
 *       and add it to its parent's entry in krg_parent table
 * In both cases, child->parent is assumed to be correctly set according to
 * the desired result.
 */
static inline void fix_chain_to_parent(struct task_struct *child,
				       pid_t parent_pid)
{
	if (child->parent != baby_sitter) {
		/* Child may be still linked in baby_sitter's children */
		remove_parent(child);
		add_parent(child);
		return;
	}

	/* At this point, child is chained in baby_sitter's or local parent's
	 * children
	 * Fix this right now */
	remove_parent(child);
	add_to_krg_parent(child, parent_pid);
}

/* Parent was remote, and can now be considered as local for its local children
 * Relink all its local children in its children list
 *
 * Assumes at least a read lock on parent's children kddm object
 */
static inline void rechain_local_children(struct task_struct *parent)
{
	struct list_head *children;
	struct task_struct *child, *tmp;

	children = get_krg_parent_entry(parent->pid);
	if (!children)
		return;

	list_for_each_entry_safe(child, tmp, children, sibling) {
		/* TODO: This will need more serious work to support ptrace */
		if (!likely(is_child(parent->children_obj, child->pid)))
			continue;
		list_del(&child->sibling);
		child->parent = parent;
		if (child->real_parent == baby_sitter &&
		    child->task_obj->real_parent == parent->pid)
			child->real_parent = parent;
		add_parent(child);
	}

	if (likely(list_empty(children)))
		delete_krg_parent_entry(parent->pid);
}

/* Expects write lock on tasklist held
 */
static inline void update_links(struct task_struct *orphan)
{
	fix_chain_to_parent(orphan, orphan->task_obj->parent);
	rechain_local_children(orphan);
}

static inline struct task_struct * find_relative(pid_t pid)
{
	struct task_struct *p;

	p = find_task_by_pid(pid);
	if (p && !unlikely(p->flags & PF_AWAY))
		return p;
	else
		return baby_sitter;
}

static inline struct task_struct * find_live_relative(pid_t pid)
{
	struct task_struct *p;

	p = find_relative(pid);
	if (p != baby_sitter
	    && unlikely(p->exit_state || (p->flags & PF_EXIT_NOTIFYING)))
		return baby_sitter;
	else
		return p;
}

static void update_relatives(struct task_struct *task)
{
	/* In case of local (real_)parent's death, delay reparenting as if
	 * (real_)parent was still remote */
	task->parent = find_live_relative(task->task_obj->parent);
	task->real_parent = find_live_relative(task->task_obj->real_parent);
	task->group_leader = find_relative(task->task_obj->group_leader);
}

/* Used by import_process
 */
void join_local_relatives(struct task_struct *orphan)
{
	kcb_children_readlock(orphan->tgid);
	write_lock_irq(&tasklist_lock);

	/* Need to do it early to avoid a group leader task to consider itself
	 * as remote when updating the group leader pointer */
	orphan->flags &= ~PF_AWAY;

	update_relatives(orphan);
	update_links(orphan);

	write_unlock_irq(&tasklist_lock);
	kcb_children_unlock(orphan->tgid);
}

/* Expects write lock on tasklist held
 */
static void __reparent_to_baby_sitter(struct task_struct *orphan,
				      pid_t parend_pid)
{
	orphan->real_parent = baby_sitter;
	if (orphan->parent == baby_sitter)
		return;

	remove_parent(orphan);
	orphan->parent = baby_sitter;
	add_parent(orphan);
}

/* Expects write lock on tasklist held
 */
static void reparent_to_baby_sitter(struct task_struct *orphan,
				    pid_t parent_pid)
{
	__reparent_to_baby_sitter(orphan, parent_pid);
	fix_chain_to_parent(orphan, parent_pid);
}

/* Expects write lock on tasklist held
 */
static void leave_baby_sitter(struct task_struct *tsk, pid_t old_parent)
{
	BUG_ON(tsk->parent != baby_sitter);
	DEBUG(DBG_CHILDREN, 2,
	      "%d old_parent=%d -> real_parent=%d parent=%d\n",
	      tsk->pid, old_parent,
	      tsk->task_obj->real_parent, tsk->task_obj->parent);
	update_relatives(tsk);
	BUG_ON(tsk->parent == baby_sitter);
	BUG_ON(tsk->real_parent == baby_sitter);

	remove_from_krg_parent(tsk, old_parent);
	add_parent(tsk);
	DEBUG(DBG_CHILDREN, 2, "%d real_parent=%d, parent=%d\n",
	      tsk->pid, tsk->real_parent->pid, tsk->parent->pid);
}

/* Used by migration
 * Expects write lock on tsk->task_obj object held
 */
void leave_all_relatives(struct task_struct *tsk)
{
	struct task_struct *child, *tmp;

	write_lock_irq(&tasklist_lock);

	tsk->flags |= PF_AWAY;

	/* Update task_obj in case parent exited while
	 * we were chained in its regular children list */
	if (tsk->parent != baby_sitter)
		tsk->task_obj->parent = tsk->parent->pid;
	if (tsk->real_parent != baby_sitter) {
		tsk->task_obj->real_parent = tsk->real_parent->pid;
		tsk->task_obj->real_parent_tgid = tsk->real_parent->tgid;
	}

	/* Make local children act as if tsk were already remote */
	list_for_each_entry_safe(child, tmp, &tsk->children, sibling)
		reparent_to_baby_sitter(child, tsk->pid);

	/* Make parent act as if tsk were already remote */
	if (tsk->parent == baby_sitter) {
		/* parent is remote, but tsk is still linked to the local
		 * children list of its parent */
		remove_from_krg_parent(tsk, tsk->task_obj->parent);
		add_parent(tsk);
	} else
		__reparent_to_baby_sitter(tsk, tsk->parent->pid);

	write_unlock_irq(&tasklist_lock);
}

/* Expects tasklist writelocked
 */
static void kcb_fork_add_parent(struct task_struct *task, pid_t parent_pid)
{
	add_to_krg_parent(task, parent_pid);
}

/* Expects task kddm object write locked and tasklist lock write locked
 */
static void kcb_reparent_to_local_child_reaper(struct task_struct *task)
{
	struct task_kddm_object *task_obj = task->task_obj;
	pid_t parent_pid;

	/* If task is ptraced, the ptracer is local and we can safely set
	 * task_obj->parent to parent->pid.
	 */
	task_obj->real_parent = child_reaper(task)->pid;
	task_obj->real_parent_tgid = child_reaper(task)->tgid;
	parent_pid = task_obj->parent;
	if (likely(task->parent == baby_sitter))
		task_obj->parent = child_reaper(task)->pid;
	else
		BUG_ON(task_obj->parent != task->parent->pid);
	leave_baby_sitter(task, parent_pid);
	BUG_ON(task->real_parent != child_reaper(task));
	BUG_ON(task->parent == baby_sitter);

	if (task->exit_signal != -1)
		task->exit_signal = SIGCHLD;
}

void kcb_unhash_process(struct task_struct *tsk)
{
	pid_t real_parent_tgid;
	struct children_kddm_object *obj = NULL;

	if (unlikely(tsk->exit_state == EXIT_MIGRATION))
		return;

	/* If we are inside de_thread() and tsk is an old thread group leader
	 * being reaped by the new thread group leader, we do not want to remove
	 * tsk->pid from the global children list of tsk's parent. */
	if (tsk->pid != tsk->tgid || thread_group_leader(tsk))
		obj = kcb_parent_children_writelock(tsk, &real_parent_tgid);
		/* After that, obj may still be NULL if real_parent does not
		 * have a children kddm object. */
	write_lock_irq(&tasklist_lock);
	/* Won't do anything if obj is NULL. */
	kcb_remove_child(obj, tsk->pid);
	if (tsk->parent == baby_sitter) {
		remove_from_krg_parent(tsk, tsk->task_obj->parent);
		add_parent(tsk);
	}
	write_unlock_irq(&tasklist_lock);
	if (obj)
		kcb_children_unlock(real_parent_tgid);
}

void register_children_hooks(void)
{
	hook_register(&kh_alloc_children, kcb_alloc_children);
	hook_register(&kh_share_children, kcb_share_children);
	hook_register(&kh_exit_children, kcb_exit_children);
	hook_register(&kh_children_get, kcb_children_get);
	hook_register(&kh_children_put, kcb_children_put);
	hook_register(&kh_children_alive, kcb_children_alive);
	hook_register(&kh_new_child, kcb_new_child);
	hook_register(&kh_set_child_pgid, kcb_set_child_pgid);
	hook_register(&kh_set_child_exit_signal, kcb_set_child_exit_signal);
	hook_register(&kh_set_child_exit_state, kcb_set_child_exit_state);
	hook_register(&kh_set_child_location, kcb_set_child_location);
	hook_register(&kh_remove_child, kcb_remove_child);
	hook_register(&kh_reparent_child, kcb_reparent_child);
	hook_register(&kh_forget_original_remote_parent,
		      kcb_forget_original_remote_parent);
	hook_register(&kh_reparent_to_local_child_reaper,
		      kcb_reparent_to_local_child_reaper);
	hook_register(&kh_get_real_parent_tgid, kcb_get_real_parent_tgid);
	hook_register(&kh_get_parent, kcb_get_parent);
	hook_register(&kh_ptrace_link, kcb_ptrace_link);
	hook_register(&kh_ptrace_unlink, kcb_ptrace_unlink);
	hook_register(&kh_children_writelock, kcb_children_writelock);
	hook_register(&kh_children_writelock_nested,
		      kcb_children_writelock_nested);
	hook_register(&kh_children_readlock, kcb_children_readlock);
	hook_register(&kh_parent_children_writelock,
		      kcb_parent_children_writelock);
	hook_register(&kh_parent_children_readlock,
		      kcb_parent_children_readlock);
	hook_register(&kh_children_unlock, kcb_children_unlock);
	hook_register(&kh_do_wait, kcb_do_wait);
	hook_register(&kh_update_self_exec_id, kcb_update_self_exec_id);
	hook_register(&kh_get_real_parent_self_exec_id,
		      kcb_get_real_parent_self_exec_id);
	hook_register(&kh_fork_add_parent, kcb_fork_add_parent);
}

/**
 * @author Louis Rilling
 */
void proc_children_start(void)
{
	unsigned long cache_flags = SLAB_PANIC;

#ifdef CONFIG_DEBUG_SLAB
	cache_flags |= SLAB_POISON;
#endif
	children_obj_cachep =
		kmem_cache_create("children_kddm_obj",
				  sizeof(struct children_kddm_object),
				  0, cache_flags,
				  NULL, NULL);
	remote_child_cachep = kmem_cache_create("remote_child",
						sizeof(struct remote_child),
						0, cache_flags,
						NULL, NULL);
	krg_parent_head_cachep = kmem_cache_create("krg_parent_head",
						   sizeof(struct list_head),
						   0, cache_flags,
						   NULL, NULL);

	register_io_linker(CHILDREN_LINKER, &children_io_linker);

	children_kddm_set = create_new_kddm_set(kddm_def_ns,CHILDREN_KDDM_ID,
						CHILDREN_LINKER,
						KDDM_CUSTOM_DEF_OWNER,
						0, 0);
	if (IS_ERR(children_kddm_set))
		OOM;
	krg_parent_table = hashtable_new(PROCESS_HASH_TABLE_SIZE);
	if (!krg_parent_table)
		OOM;

	DEBUG(DBG_CHILDREN, 1, "Done\n");
}

/**
 * @author Louis Rilling
 */
void proc_children_exit(void)
{
	hashtable_free(krg_parent_table);
}
