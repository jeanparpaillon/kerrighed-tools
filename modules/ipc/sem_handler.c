/*
 *  Kerrighed/modules/ipc/sem_handler.c
 *
 *  All the code for sharing IPC semaphore accross the cluster
 *
 *  Copyright (C) 2007-2008 Matthieu Fertré - INRIA
 */

#include "debug_keripc.h"

#define MODULE_NAME "System V semaphore"

#include <linux/sem.h>
#include <linux/nsproxy.h>
#include <ctnr/kddm.h>
#include <hotplug/hotplug.h>
#include <kerrighed/sem.h>
#include <kerrighed/unique_id.h>
#include <rpc/rpc.h>
#include "ipc_handler.h"
#include "sem_handler.h"
#include "semarray_io_linker.h"
#include "semundolst_io_linker.h"
#include "ipcmap_io_linker.h"

/* Kddm set of SEM array structures */
struct kddm_set *semarray_struct_kddm_set = NULL;
struct kddm_set *semkey_struct_kddm_set = NULL;

/* Kddm set of sem_undo_list */
struct kddm_set *sem_undo_list_kddm_set = NULL;

/* unique_id generator for sem_undo_list identifier */
unique_id_root_t undo_list_unique_id_root;

/* Kddm set of IPC allocation bitmap structures */
struct kddm_set *semmap_struct_kddm_set = NULL;

/*****************************************************************************/
/*                                                                           */
/*                                KERNEL HOOKS                               */
/*                                                                           */
/*****************************************************************************/

struct sem_array *kcb_ipcsem_lock(struct ipc_namespace *ns, int id)
{
	semarray_object_t *sem_object;
	struct sem_array *sma;
	int index ;

	BUG_ON (ns != &init_ipc_ns);

	index = id % SEQ_MULTIPLIER;

	DEBUG (DBG_KERIPC_SEM_LOCK, 1, "->Lock sem_array %d(%d)\n", id, index);

	sem_object = _kddm_grab_object_no_ft(semarray_struct_kddm_set, index);
	DEBUG (DBG_KERIPC_SEM_LOCK, 5, "End grab %p\n", sem_object);

	if (!sem_object) {
		_kddm_put_object(semarray_struct_kddm_set, index);
		return NULL;
	}
	BUG_ON(!sem_object);
	sma = sem_object->local_sem;

	BUG_ON(!sma);

	DEBUG (DBG_KERIPC_SEM_LOCK, 5, "--> sma->sem_perm.lock\n");
	mutex_lock(&sma->sem_perm.mutex);
	DEBUG (DBG_KERIPC_SEM_LOCK, 5, "<-- sma->sem_perm.lock\n");

	if (sma->sem_perm.deleted) {
		mutex_unlock(&sma->sem_perm.mutex);
		sma = NULL;
	}

	DEBUG (DBG_KERIPC_SEM_LOCK, 1, "<- Lock sem_array %d(%d) done: %p\n",
	       id, index, sma);

	return sma;
}



void kcb_ipcsem_unlock(struct sem_array *sma)
{
	int index ;

	index = sma->sem_id % SEQ_MULTIPLIER;

	DEBUG (DBG_KERIPC_SEM_LOCK, 1, "-> Unlock sem_array %d(%d) - %p\n",
	       sma->sem_id, index, sma);

	if (sma->sem_perm.deleted) {
		DEBUG (DBG_KERIPC_SEM_LOCK, 1,
		       "-> Unlock sem_array %d(%d) - deleted!\n",
		       sma->sem_id, index);
		return;
	}

	_kddm_put_object(semarray_struct_kddm_set, index);

	mutex_unlock(&sma->sem_perm.mutex);

	DEBUG (DBG_KERIPC_SEM_LOCK, 1, "<- Unlock sem_array %d(%d) : done\n",
	       sma->sem_id, index);
}

/** Notify the creation of a new IPC sem_array to Kerrighed.
 *
 *  @author Matthieu Fertré
 */
int kcb_ipcsem_newary(struct ipc_namespace *ns, struct sem_array *sma)
{
	semarray_object_t *sem_object;
	long *key_index;
	int index ;

	BUG_ON (ns != &init_ipc_ns); /* TODO: manage IPC namespace */

	index = sma->sem_id % SEQ_MULTIPLIER;

	DEBUG (DBG_KERIPC_SEM, 2, "New sem_array : id %d(%d) (%p)\n",
	       sma->sem_id, index, sma);

	sem_object = _kddm_grab_object_manual_ft(semarray_struct_kddm_set,
						 index);

	BUG_ON(sem_object);

	sem_object = kmem_cache_alloc(semarray_object_cachep, GFP_KERNEL);
	if (!sem_object)
		return -ENOMEM;

	sem_object->local_sem = sma;
	DEBUG (DBG_KERIPC_SEM, 5, "sma->sem_pending : %p\n", sma->sem_pending);
	DEBUG (DBG_KERIPC_SEM, 5, "sma->remote_sem_pending : %p\n",
	       sma->remote_sem_pending);

	sem_object->mobile_sem_base = NULL;
	sem_object->imported_sem = *sma;

	/* there are no pending objects for the moment */
	BUG_ON(sma->sem_pending);
	BUG_ON(sma->remote_sem_pending);
	sem_object->imported_sem.sem_pending = NULL;
	sem_object->imported_sem.sem_pending_last = NULL;
	sem_object->imported_sem.remote_sem_pending = NULL;
	sem_object->imported_sem.remote_sem_pending_last = NULL;

	_kddm_set_object(semarray_struct_kddm_set, index, sem_object);

	if (sma->sem_perm.key != IPC_PRIVATE)
	{
		key_index = _kddm_grab_object(semkey_struct_kddm_set,
					      sma->sem_perm.key);
		*key_index = index;
		_kddm_put_object (semkey_struct_kddm_set, sma->sem_perm.key);
	}

	_kddm_put_object(semarray_struct_kddm_set, index);
	DEBUG (DBG_KERIPC_SEM, 5, "sma->sem_pending : %p\n", sma->sem_pending);
	DEBUG (DBG_KERIPC_SEM, 5, "sma->remote_sem_pending : %p\n",
	       sma->remote_sem_pending);
	DEBUG (DBG_KERIPC_SEM, 2, "New sem_array (id %d(%d)) : done\n",
	       sma->sem_id, index);

	return 0;
}

static inline void __remove_semundo_from_proc_list(struct sem_array *sma,
						   unique_id_t proc_list_id)
{
	struct semundo_id * undo_id, *next, *prev;
	semundo_list_object_t *undo_list = _kddm_grab_object_no_ft(
		sem_undo_list_kddm_set,	proc_list_id);

	if (!undo_list)
		goto exit;

	DEBUG(DBG_KERIPC_SEM, 2, "SemID: %d, ListID: %ld\n",
	      sma->sem_id, proc_list_id);

	prev = NULL;
	for (undo_id = undo_list->list; undo_id; undo_id = next) {
		next = undo_id->next;

		if (undo_id->semid == sma->sem_id) {
			atomic_dec(&undo_list->semcnt);
			kfree(undo_id);
			if (!prev)
				undo_list->list = next;
			else
				prev->next = next;

			goto exit;
		}
		prev = undo_id;
	}
	BUG();

exit:
	DEBUG(DBG_KERIPC_SEM, 5, "Process %ld has %d sem_undos\n",
	      proc_list_id, atomic_read(&undo_list->semcnt));

	_kddm_put_object(sem_undo_list_kddm_set, proc_list_id);
}

void kcb_ipcsem_freeary(struct ipc_namespace *ns, struct sem_array *sma, int id)
{
	int index;
	key_t key;
	struct sem_undo* undo, *next;

	index = sma->sem_id % SEQ_MULTIPLIER;
	key = sma->sem_perm.key;

	DEBUG (DBG_KERIPC_SEM, 2, "Destroy sem_array %d(%d)\n", sma->sem_id,
	       index);

	/* removing the related semundo from the list per process */
	for (undo = sma->undo; undo; undo = next) {
		next = undo->id_next;
		__remove_semundo_from_proc_list(sma, undo->proc_list_id);
		kfree(undo);
	}
	sma->undo = NULL;

	if (key != IPC_PRIVATE) {
		_kddm_grab_object(semkey_struct_kddm_set,
				  sma->sem_perm.key);
		_kddm_remove_frozen_object (semkey_struct_kddm_set, key);
		/* _kddm_remove_object (semkey_struct_kddm_set, key); */
	}

	DEBUG (DBG_KERIPC_SEM, 4, "Really Destroy sem_array %d(%d)\n",
	       sma->sem_id, index);

	local_sem_unlock(sma);
	_kddm_remove_frozen_object (semarray_struct_kddm_set, index);

	free_ipc_id (&sem_ids(ns), index);

	DEBUG (DBG_KERIPC_SEM, 2, "Destroy sem_array %d: done\n",
	       index);
}

struct ipcsem_wakeup_msg {
	kerrighed_node_t requester;
	int sem_id;
	pid_t pid;
	int error;
};

void handle_ipcsem_wakeup_process(struct rpc_desc *desc, void *_msg,
				  size_t size)
{
	struct ipcsem_wakeup_msg *msg = _msg;
	struct sem_array *sma;
	struct sem_queue *q;
	struct ipc_namespace *ns = &init_ipc_ns; /* TODO: manage IPC namespace */

	DEBUG (DBG_KERIPC_SEM, 2,
	       "try to wake up process %d blocked on sem %d\n",
	       msg->pid, msg->sem_id);

	/* take only a local lock because the requester node has the kddm lock
	   on the semarray */
	sma = local_sem_lock(ns, msg->sem_id);
	BUG_ON(!sma);

       for (q = sma->sem_pending; q; q = q->next) {
               /* compare to q->sleeper->pid instead of q->pid
                  because q->pid == q->sleeper->tgid */
               if (q->sleeper->pid == msg->pid)
                       goto found;
       }

       BUG();
found:
       __remove_from_queue(sma, q);
       q->status = 1; /* IN_WAKEUP; */
       BUG_ON(!q->sleeper);
       BUG_ON(q->pid != q->sleeper->tgid);
       wake_up_process(q->sleeper);
       smp_wmb();
       q->status = msg->error;

       local_sem_unlock(sma);

       DEBUG (DBG_KERIPC_SEM, 2,
	      "try to wake up process %d blocked on sem %d (error: %d): done\n",
	      msg->pid, msg->sem_id, msg->error);

       rpc_pack_type(desc, msg->error);
}

void kcb_ipcsem_wakeup_process(struct sem_queue *q, int error)
{
	struct ipcsem_wakeup_msg msg;
	struct rpc_desc * desc;

	msg.requester = kerrighed_node_id;
	msg.sem_id = q->sma->sem_id;
	msg.pid = remote_sleeper_pid(q); /* q->pid contains the tgid */
	msg.error = error;

	DEBUG (DBG_KERIPC_SEM, 2,
	       "Ask node %d to wake up process %d (error: %d) blocked on sem %d\n",
	       q->node, msg.pid, error, q->sma->sem_id);

	desc = rpc_begin(IPC_SEM_WAKEUP, q->node);
	rpc_pack_type(desc, msg);
	rpc_unpack_type(desc, msg.error);
	rpc_end(desc, 0);
}


static inline semundo_list_object_t * __create_semundo_proc_list(unique_id_t * undo_list_id)
{
	semundo_list_object_t *undo_list;

	/* get a random id */
	*undo_list_id = get_unique_id(&undo_list_unique_id_root);

	DEBUG(DBG_KERIPC_SEM, 5, "Unique id: %lu\n", *undo_list_id);

	undo_list = _kddm_grab_object_manual_ft(
		sem_undo_list_kddm_set,
		*undo_list_id);

#ifdef CONFIG_KRG_DEBUG
	if (undo_list)
		DEBUG(DBG_KERIPC_SEM, 5, "Unique id: %lu - %lu\n",
		      *undo_list_id, undo_list->id);
#endif
	BUG_ON(undo_list);

	undo_list = kzalloc(sizeof(semundo_list_object_t), GFP_KERNEL);

	if (!undo_list)
		undo_list = ERR_PTR(-ENOMEM);

	if (IS_ERR(undo_list))
		goto exit;

	undo_list->id = *undo_list_id;
	atomic_inc(&undo_list->refcnt);

	_kddm_set_object(sem_undo_list_kddm_set, *undo_list_id, undo_list);
exit:
	return undo_list;
}

int create_semundo_proc_list(struct task_struct *tsk)
{
	int r = 0;
	semundo_list_object_t *undo_list;

	BUG_ON(tsk->sysvsem.undo_list_id != UNIQUE_ID_NONE);
	undo_list = __create_semundo_proc_list(
		&tsk->sysvsem.undo_list_id);

	if (!undo_list)
		undo_list = ERR_PTR(-ENOMEM);

	if (IS_ERR(undo_list)) {
		r = PTR_ERR(undo_list);
		goto exit;
	}

	BUG_ON(atomic_read(&undo_list->refcnt) != 1);

	_kddm_put_object(sem_undo_list_kddm_set,
			 tsk->sysvsem.undo_list_id);

	DEBUG(DBG_KERIPC_SEM, 3,
	      "UNDO: Lazy creation of semundo (export) %lu for process %d\n",
	      tsk->sysvsem.undo_list_id, tsk->pid);
exit:
	return r;
}


static int __share_new_semundo(struct task_struct *tsk)
{
	int r = 0;
	semundo_list_object_t *undo_list;

	BUG_ON(krg_current);
	BUG_ON(current->sysvsem.undo_list_id != UNIQUE_ID_NONE);

	undo_list = __create_semundo_proc_list(
		&current->sysvsem.undo_list_id);

	if (!undo_list)
		undo_list = ERR_PTR(-ENOMEM);

	if (IS_ERR(undo_list)) {
		r = PTR_ERR(undo_list);
		goto exit;
	}

	tsk->sysvsem.undo_list_id = current->sysvsem.undo_list_id;
	atomic_inc(&undo_list->refcnt);

	BUG_ON(atomic_read(&undo_list->refcnt) != 2);

	_kddm_put_object(sem_undo_list_kddm_set,
			 current->sysvsem.undo_list_id);
exit:
	return r;
}

int share_existing_semundo_proc_list(struct task_struct *tsk,
				     unique_id_t undo_list_id)
{
	int r = 0;
	semundo_list_object_t *undo_list;

	BUG_ON(undo_list_id == UNIQUE_ID_NONE);

	undo_list = _kddm_grab_object_no_ft(sem_undo_list_kddm_set,
					    undo_list_id);

	if (!undo_list)
		undo_list = ERR_PTR(-ENOMEM);

	if (IS_ERR(undo_list)) {
		r = PTR_ERR(undo_list);
		goto exit;
	}

	tsk->sysvsem.undo_list_id = undo_list_id;
	atomic_inc(&undo_list->refcnt);

	DEBUG(DBG_KERIPC_SEM, 3,
	      "UNDO: Process %d share semundo list %lu with %d processes\n",
	      tsk->pid, undo_list_id, atomic_read(&undo_list->refcnt));

	_kddm_put_object(sem_undo_list_kddm_set,
			 undo_list_id);

exit:
	return r;
}

int kcb_ipcsem_copy_semundo(unsigned long clone_flags,
			    struct task_struct *tsk)
{
	int r = 0;

	BUG_ON(!tsk);

	if (krg_current)
		goto exit;

	if (clone_flags & CLONE_SYSVSEM) {

		/* Do not support fork of process which had used semaphore
		   before Kerrighed was loaded */
		if (current->sysvsem.undo_list) {
			printk("ERROR: Do not support fork of process (%d) "
			       "that had used semaphore before Kerrighed was "
			       "started\n", tsk->tgid);
			r = -EPERM;
			goto exit;
		}

		DEBUG (DBG_KERIPC_SEM, 3,
		       "UNDO: Processes %d and %d will share "
		       "their undos list (%lu)\n",
		       current->pid, tsk->pid, current->sysvsem.undo_list_id);

		if (current->sysvsem.undo_list_id != UNIQUE_ID_NONE)
			r = share_existing_semundo_proc_list(
				tsk, current->sysvsem.undo_list_id);
		else
			r = __share_new_semundo(tsk);

		DEBUG(DBG_KERIPC_SEM, 3,
		      "UNDO: Processes %d and %d share semundo list %lu\n",
		      tsk->pid, current->pid, current->sysvsem.undo_list_id);

	} else
		/* undolist will be only created when needed */
		tsk->sysvsem.undo_list_id = UNIQUE_ID_NONE;

	/* pointer to undo_list is useless in KRG implementation of semaphores */
	tsk->sysvsem.undo_list = NULL;

exit:
	return r;
}

static inline int __add_semundo_to_proc_list(semundo_list_object_t *undo_list,
					     int semid)
{
	struct semundo_id * undo_id;
	int r = 0;
	BUG_ON(!undo_list);

#ifdef CONFIG_KRG_DEBUG
	DEBUG(DBG_KERIPC_SEM, 5,
	      "Undo list pointer: %p, nb: %d, PID: %d, Semid: %d\n",
	      undo_list, atomic_read(&undo_list->semcnt),
	      current->pid, semid);

	/* WARNING: this is a paranoiac checking */
	for (undo_id = undo_list->list; undo_id; undo_id = undo_id->next) {
		DEBUG(DBG_KERIPC_SEM, 5,
		      "* Undo list pointer: %p, PID: %d, Semid: %d\n",
		      undo_list, current->pid, undo_id->semid);

		if (undo_id->semid == semid) {
			printk("%p %p %d %d\n", undo_id,
			       undo_list, semid,
			       atomic_read(&undo_list->semcnt));
			BUG();
		}
	}
#endif

	undo_id = kmalloc(sizeof(struct semundo_id), GFP_KERNEL);
	if (!undo_id) {
		r = -ENOMEM;
		goto exit;
	}

	atomic_inc(&undo_list->semcnt);
	undo_id->semid = semid;
	undo_id->next = undo_list->list;
	undo_list->list = undo_id;

	DEBUG(DBG_KERIPC_SEM, 5, "Add %p (%d) in sem_undos list of Process %d\n",
	      undo_id, undo_id->semid, current->pid);

	DEBUG(DBG_KERIPC_SEM, 5, "Process %d has %d sem_undos\n",
	      current->pid, atomic_read(&undo_list->semcnt));

exit:
	return r;
}

struct sem_undo * kcb_ipcsem_find_undo(struct sem_array* sma)
{
	struct sem_undo * undo;
	int r = 0;
	semundo_list_object_t *undo_list = NULL;
	unique_id_t undo_list_id = current->sysvsem.undo_list_id;

	if (undo_list_id == UNIQUE_ID_NONE) {
		/* create a undolist if not yet allocated */
		undo_list = __create_semundo_proc_list(
			&undo_list_id);
		DEBUG(DBG_KERIPC_SEM, 3,
		      "UNDO: Create undolist for process %d: %lu - %p\n",
		      current->pid, undo_list_id, undo_list);

		if (IS_ERR(undo_list)) {
			undo = ERR_PTR(PTR_ERR(undo_list));
			goto exit;
		}

		BUG_ON(atomic_read(&undo_list->semcnt) != 0);

		current->sysvsem.undo_list_id = undo_list_id;
	} else {
		/* check in the undo list of the sma */
		DEBUG(DBG_KERIPC_SEM, 5, "sma:%d, undolist_id:%ld\n",
		      sma->sem_id, undo_list_id);

		for (undo = sma->undo; undo; undo = undo->id_next)
			if (undo->proc_list_id == undo_list_id) {
				DEBUG(DBG_KERIPC_SEM, 5,
				      "Allready in sma list: DONE\n");
				goto exit;
			}
	}

#ifdef CONFIG_KRG_DEBUG
	/* there's currently no undo for the couple sma/task */
	/* only to debug */
	for (undo = sma->undo; undo; undo = undo->id_next)
		DEBUG(DBG_KERIPC_SEM, 5, "Sma: %d, undolist_id: %ld\n",
		      sma->sem_id, undo->proc_list_id);
#endif

	/* allocate one */
	undo = kzalloc(sizeof(struct sem_undo) +
		       sizeof(short)*(sma->sem_nsems), GFP_KERNEL);
	if (!undo) {
		undo = ERR_PTR(-ENOMEM);
		goto exit_put_kddm;
	}

	undo->proc_next = NULL;
	undo->proc_list_id = undo_list_id;
	undo->semid = sma->sem_id;
	undo->semadj = (short *) &undo[1];
	undo->id_next = sma->undo;
	sma->undo = undo;

	/* reference it in the undo_list per process*/
	BUG_ON(undo_list_id == UNIQUE_ID_NONE);

	if (!undo_list)
		undo_list = _kddm_grab_object_no_ft(sem_undo_list_kddm_set,
						    undo_list_id);

	if (!undo_list) {
		r = -ENOMEM;
		goto exit_free_undo;
	}

	r = __add_semundo_to_proc_list(undo_list, undo->semid);

exit_free_undo:
	if (r) {
		sma->undo = undo->id_next;
		kfree(undo);
		undo = ERR_PTR(r);
	}

exit_put_kddm:
	if (undo_list && !IS_ERR(undo_list))
		_kddm_put_object(sem_undo_list_kddm_set,
				 undo_list_id);
exit:
	return undo;
}

static inline void __remove_semundo_from_sem_list(struct ipc_namespace *ns,
						  int semid,
						  unique_id_t undo_list_id)
{
	struct sem_array *sma;
	struct sem_undo *undo, *prev;

	sma = sem_lock(ns, semid);
	if (!sma)
		return;

	DEBUG(DBG_KERIPC_SEM, 5, "Removing semundo (%d, %ld)\n",
	      semid, undo_list_id);

	prev = NULL;
	for (undo = sma->undo; undo; undo = undo->id_next) {
		if (undo->proc_list_id == undo_list_id) {
			if (prev)
				prev->id_next = undo->id_next;
			else
				sma->undo = undo->id_next;

			__exit_sem_found(sma, undo);

			kfree(undo);
			goto exit_unlock;
		}
		prev = undo;
	}
	BUG();

exit_unlock:
	sem_unlock(sma);
}

void leave_semundo_proc_list(unique_id_t undo_list_id)
{
	semundo_list_object_t * undo_list;
	struct semundo_id * undo_id, *next;

	BUG_ON(undo_list_id == UNIQUE_ID_NONE);

	undo_list = _kddm_grab_object_no_ft(sem_undo_list_kddm_set,
					    undo_list_id);
	if (!undo_list) {
		printk("undo_list_id: %lu\n", undo_list_id);
		BUG();
	}
	if (!atomic_dec_and_test(&undo_list->refcnt))
		goto exit_wo_action;

	DEBUG(DBG_KERIPC_SEM, 5, "Semundo list %ld contains %d sem_undos\n",
	      undo_list_id, atomic_read(&undo_list->semcnt));

	for (undo_id = undo_list->list; undo_id; undo_id = next) {
		next = undo_id->next;
		__remove_semundo_from_sem_list(&init_ipc_ns, undo_id->semid,
					       undo_list_id);
		kfree(undo_id);
	}
	undo_list->list = NULL;
	atomic_set(&undo_list->semcnt, 0);

	_kddm_remove_frozen_object(sem_undo_list_kddm_set,
				   undo_list_id);

	return;

exit_wo_action:
	_kddm_put_object(sem_undo_list_kddm_set,
			 undo_list_id);
}

void kcb_ipcsem_exit_sem(struct task_struct * tsk)
{
	if (tsk->sysvsem.undo_list_id == UNIQUE_ID_NONE)
		return;

	leave_semundo_proc_list(tsk->sysvsem.undo_list_id);
}

/*****************************************************************************/
/*                                                                           */
/*                              INITIALIZATION                               */
/*                                                                           */
/*****************************************************************************/



void sem_handler_init (void)
{
	DEBUG (DBG_KERIPC_INITS, 1, "Start\n");

	init_unique_id_root(&undo_list_unique_id_root);

	semarray_object_cachep = kmem_cache_create("semarray_object",
						   sizeof(semarray_object_t),
						   0, SLAB_PANIC, NULL, NULL);

	register_io_linker(SEMARRAY_LINKER, &semarray_linker);
	register_io_linker(SEMKEY_LINKER, &semkey_linker);
	register_io_linker(SEMUNDO_LINKER, &semundo_linker);

	semarray_struct_kddm_set = create_new_kddm_set(kddm_def_ns,
						       SEMARRAY_KDDM_ID,
						       SEMARRAY_LINKER,
						       KDDM_RR_DEF_OWNER,
						       sizeof(semarray_object_t),
						       KDDM_LOCAL_EXCLUSIVE);

	BUG_ON (IS_ERR (semarray_struct_kddm_set));

	semkey_struct_kddm_set = create_new_kddm_set (kddm_def_ns,
						      SEMKEY_KDDM_ID,
						      SEMKEY_LINKER,
						      KDDM_RR_DEF_OWNER,
						      sizeof(long), 0);

	BUG_ON (IS_ERR (semkey_struct_kddm_set));

	semmap_struct_kddm_set = create_new_kddm_set (kddm_def_ns,
						      SEMMAP_KDDM_ID,
						      IPCMAP_LINKER,
						      KDDM_RR_DEF_OWNER,
						      sizeof(ipcmap_object_t),
						      0);

	BUG_ON (IS_ERR (semmap_struct_kddm_set));

	sem_undo_list_kddm_set = create_new_kddm_set (kddm_def_ns,
						      SEMUNDO_KDDM_ID,
						      SEMUNDO_LINKER,
						      KDDM_RR_DEF_OWNER,
						      sizeof(semundo_list_object_t),
						      KDDM_LOCAL_EXCLUSIVE);

	sem_ids(&init_ipc_ns).id_ctnr = SEMARRAY_KDDM_ID;
	sem_ids(&init_ipc_ns).key_ctnr = SEMKEY_KDDM_ID;
	sem_ids(&init_ipc_ns).map_ctnr = SEMMAP_KDDM_ID;

	hook_register(&kh_ipcsem_newary, kcb_ipcsem_newary);
	hook_register(&kh_ipcsem_freeary, kcb_ipcsem_freeary);
	hook_register(&kh_ipcsem_lock, kcb_ipcsem_lock);
	hook_register(&kh_ipcsem_unlock, kcb_ipcsem_unlock);
	hook_register(&kh_ipcsem_wakeup_process, kcb_ipcsem_wakeup_process);
	hook_register(&kh_ipcsem_copy_semundo, kcb_ipcsem_copy_semundo);
	hook_register(&kh_ipcsem_find_undo, kcb_ipcsem_find_undo);
	hook_register(&kh_ipcsem_exit_sem, kcb_ipcsem_exit_sem);

	rpc_register_void(IPC_SEM_WAKEUP, handle_ipcsem_wakeup_process, 0);

	DEBUG (DBG_KERIPC_INITS, 1, "Sem Server configured\n");
}



void sem_handler_finalize (void)
{
}
