/*
 *  Kerrighed/modules/ipc/semarray_io_linker.c
 *
 *  KDDM SEM array Linker.
 *
 *  Copyright (C) 2007-2008 Matthieu Fertré - INRIA
 */

#include "debug_keripc.h"

#define MODULE_NAME "Sem array linker"

#include <linux/sem.h>
#include <linux/lockdep.h>
#include <linux/security.h>
#include <kerrighed/ipc.h>

#include <rpc/rpc.h>
#include <ctnr/kddm.h>

#include "semarray_io_linker.h"

struct kmem_cache *semarray_object_cachep;

/** Create a local instance of an remotly existing Semaphore.
 *
 *  @author Matthieu Fertré
 */
struct sem_array *create_local_sem(struct sem_array *received_sma)
{
	struct ipc_namespace *ns = &init_ipc_ns; /* TODO: manage namespace */
	struct sem_array *sma;
	int size_sems;
	int id;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
	       "Sem %p %d - nbsems: %ld\n", received_sma, received_sma->sem_id,
	       received_sma->sem_nsems);

	size_sems = received_sma->sem_nsems * sizeof (struct sem);
	sma = ipc_rcu_alloc(sizeof (*sma) + size_sems);
	if (!sma) {
		return ERR_PTR(-ENOMEM);
	}
	*sma = *received_sma;

	sma->sem_base = (struct sem *) &sma[1];
	memcpy(sma->sem_base, received_sma->sem_base, size_sems);

	id = sma->sem_id % SEQ_MULTIPLIER;
	grow_ary (&sem_ids(ns), id + 1);
	sem_ids(ns).in_use++;

	security_sem_alloc(sma);
	mutex_init(&sma->sem_perm.mutex);
	sma->sem_pending = NULL;
	sma->sem_pending_last = &sma->sem_pending;
	sma->undo = NULL;

	sma->remote_sem_pending = NULL;
	sma->remote_sem_pending_last = &sma->remote_sem_pending;

	sem_ids(ns).entries->p[id] = &sma->sem_perm;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Local sem_array created %d (%p)\n",
	       sma->sem_id, sma);

	return sma;
}

#ifdef CONFIG_KRG_DEBUG
static inline void show_pendings(struct sem_array * sma)
{
	struct sem_queue * q, **q_prev;
	printk("local pending on %d: ", sma->sem_id);

	q = sma->sem_pending;

	if (q) {
		BUG_ON(*q->prev != sma->sem_pending);

		q_prev = q->prev;
		while (q) {
			BUG_ON(q->prev != q_prev);

			printk("%d ", q->pid);
			q_prev = &q->next;
			q = q->next;
		}
	} else {
		printk("none");
	}
	printk("\n remote pending of %d: ", sma->sem_id);

	q = sma->remote_sem_pending;
	if (q) {
		BUG_ON(*q->prev != sma->remote_sem_pending);

		q_prev = q->prev;
		while (q) {
			BUG_ON(q->prev != q_prev);

			printk("%d ", q->pid);
			q_prev = &q->next;
			q = q->next;
		}
	} else {
		printk("none");
	}
	printk("\n");
}
#endif

#define IN_WAKEUP 1

static inline void update_sem_queues(struct sem_array *sma,
				     struct sem_array *received_sma)
{
	struct sem_queue *q, *local_q, *next;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
	       "Update %p from %p\n", sma, received_sma);

	BUG_ON(received_sma->sem_pending);
	BUG_ON(received_sma->sem_pending_last);

	/* adding (to local sem) semqueues that are not local */
	for (q = received_sma->remote_sem_pending; q; q = next) {

		int is_local = 0;
		next = q->next;

		DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
		       "Checking if sem_queue (%d %d) is local\n",
		       sma->sem_id, q->pid);

		/* checking if the sem_queue is local */
		for (local_q = sma->sem_pending;
		     local_q; local_q = local_q->next)

			/* comparing local_q->pid to q->pid is not sufficient
			   as they contains tgid and not pid */
			if (local_q->sleeper->pid == remote_sleeper_pid(q)) {
				/* the sem_queue is local */
				is_local = 1;

				BUG_ON(q->undo && !local_q->undo);
				BUG_ON(local_q->undo && !q->undo);
				local_q->undo = q->undo;
				/* No need to update q->status, as it is done when
				   needed in handle_ipcsem_wake_up_process */
				BUG_ON(q->status == IN_WAKEUP);
				BUG_ON(local_q->status != q->status);

				DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
				       "Update local queue (%d %d)\n",
				       sma->sem_id, q->pid);
				goto next;
			}

	next:
		remove_from_remote_queue(received_sma, q, is_local);
		if (!is_local)
			append_to_remote_queue(sma, q);
	}

	BUG_ON(received_sma->remote_sem_pending);
	received_sma->remote_sem_pending_last = NULL;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
	       "Update Sem queues %p %p: DONE\n", sma, received_sma);

#ifdef CONFIG_KRG_DEBUG
	if (match_debug("ipc", DBG_KERIPC_SEMARRAY_LINKER, 1)) {
		printk("===========================\n");
		show_pendings(sma);
		printk("===========================\n");
	}
#endif
}

/** Update a local instance of a remotly existing IPC semaphore.
 *
 *  @author Matthieu Fertré
 */
static void update_local_sem(struct sem_array *local_sma,
			     struct sem_array *received_sma)
{
	int size_sems;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
	       "Update Sem %p %p\n",
	       received_sma, local_sma);

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 1,
	       "Update Sem %d - nb_sems : %ld %ld\n",
	       received_sma->sem_id, local_sma->sem_nsems,
	       received_sma->sem_nsems);

	size_sems = local_sma->sem_nsems * sizeof (struct sem);

	/* getting new values from received semaphore */
	local_sma->sem_otime = received_sma->sem_otime;
	local_sma->sem_ctime = received_sma->sem_ctime;
	memcpy(local_sma->sem_base, received_sma->sem_base, size_sems);

	/* updating sem_undos list */
	local_sma->undo = received_sma->undo;
	received_sma->undo = NULL;

	/* updating semqueues list */
	update_sem_queues(local_sma, received_sma);

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 1,
	       "Update Sem %d %p: DONE\n",
	       received_sma->sem_id, local_sma);
}

static inline void free_remote_semqueue(struct sem_queue *q)
{
	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Free remote semqueue %d %d\n",
	       q->id, q->pid);

	if (q->sops)
		kfree(q->sops);
	kfree(q);
}

/*****************************************************************************/
/*                                                                           */
/*                         SEM Array KDDM IO FUNCTIONS                       */
/*                                                                           */
/*****************************************************************************/



int semarray_alloc_object (struct kddm_obj * obj_entry,
			   struct kddm_set * set,
			   objid_t objid)
{
	semarray_object_t *sem_object;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 1,
	       "Alloc object (%ld;%ld), obj_entry %p\n",
	       set->id, objid, obj_entry);

	sem_object = kmem_cache_alloc(semarray_object_cachep, GFP_KERNEL);
	if (!sem_object)
		return -ENOMEM;

	sem_object->local_sem = NULL;
	sem_object->mobile_sem_base = NULL;
	obj_entry->object = sem_object;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Alloc object (%ld;%ld): done %p\n",
	       set->id, objid, sem_object);

	return 0;
}



/** Handle a kddm set sem_array first touch
 *  @author Matthieu Fertré, Renaud Lottiaux
 *
 *  @param  obj_entry  Kddm object descriptor.
 *  @param  set       Kddm set descriptor
 *  @param  objid     Id of the object to create.
 *
 *  @return  0 if everything is ok. Negative value otherwise.
 */
int semarray_first_touch (struct kddm_obj * obj_entry,
			  struct kddm_set * set,
			  objid_t objid,
			  int flags)
{
	BUG(); // I should never get here !

	return 0;
}



/** Insert a new sem_array in local structures.
 *  @author Matthieu Fertré, Renaud Lottiaux
 *
 *  @param  obj_entry  Descriptor of the object to insert.
 *  @param  set       Kddm set descriptor
 *  @param  objid     Id of the object to insert.
 */
int semarray_insert_object (struct kddm_obj * obj_entry,
			    struct kddm_set * set,
			    objid_t objid)
{
	semarray_object_t *sem_object;
	struct sem_array *sem;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 1,
	       "Insert object (%ld;%ld), obj_entry %p, sem_obj %p\n",
	       set->id, objid, obj_entry, obj_entry->object);

	sem_object = obj_entry->object;
	BUG_ON(!sem_object);

	if (!sem_object->local_sem) {
		/* This is the first time the object is inserted locally.
		 * We need to allocate kernel sem_array structure.
		 */
		sem = create_local_sem(&sem_object->imported_sem);
		sem_object->local_sem = sem;
	}

	update_local_sem(sem_object->local_sem,
			 &sem_object->imported_sem);

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Insert object (%ld;%ld) : done %p\n",
	       set->id, objid, sem_object);

	return 0;
}



/** Invalidate a kddm object semarray.
 *  @author Matthieu Fertré, Renaud Lottiaux
 *
 *  @param  obj_entry  Descriptor of the object to invalidate.
 *  @param  set       Kddm set descriptor
 *  @param  objid     Id of the object to invalidate
 */
int semarray_invalidate_object (struct kddm_obj * obj_entry,
				struct kddm_set * set,
				objid_t objid)
{
	semarray_object_t *sem_object = obj_entry->object;
	struct sem_array *sma = sem_object->local_sem;
	struct sem_undo *un, *u_next;
	struct sem_queue *q, *q_next;

	BUG_ON(sem_object->imported_sem.undo);
	BUG_ON(sem_object->imported_sem.sem_pending);
	BUG_ON(sem_object->imported_sem.sem_pending_last);
	BUG_ON(sem_object->imported_sem.remote_sem_pending);
	BUG_ON(sem_object->imported_sem.remote_sem_pending_last);

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 1, "Invalidate object (%ld;%ld)\n",
	       set->id, objid);

	/* freeing the semundo list */
	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3, "Free all the semundos\n");
	for (un = sma->undo; un; un = u_next) {
		u_next = un->id_next;
		DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3, "Free semundo %d %ld\n",
		       un->semid, un->proc_list_id);
		BUG_ON(un->proc_next);
		kfree(un);
	}
	sma->undo = NULL;

	/* freeing the remote semqueues */
	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3, "Free all the remote semqueues\n");
	for (q = sma->remote_sem_pending; q; q = q_next) {
		q_next = q->next;
		free_remote_semqueue(q);
	}
	sma->remote_sem_pending = NULL;
	sma->remote_sem_pending_last = &sma->remote_sem_pending;

	return KDDM_IO_KEEP_OBJECT;
}



/** Handle a kddm semaphore remove.
 *  @author Matthieu Fertré, Renaud Lottiaux
 *
 *  @param  obj_entry  Descriptor of the object to remove.
 *  @param  set       Kddm set descriptor.
 *  @param  padeid    Id of the object to remove.
 */
int semarray_remove_object (void *object,
			    struct kddm_set * set,
			    objid_t objid)
{
	struct ipc_namespace *ns = &init_ipc_ns; /* TODO: manage namespace */
	semarray_object_t *sem_object;
	struct sem_array *sma;


	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 1, "remove object (%ld;%ld)\n",
	       set->id, objid);

	sem_object = object;

	if (sem_object) {
		sma = sem_object->local_sem;

		local_sem_lock(ns, sma->sem_id);
		__freeary(ns, sma, sma->sem_id);

		kfree(sem_object->mobile_sem_base);
		sem_object->mobile_sem_base = NULL;
		kmem_cache_free (semarray_object_cachep, sem_object);
	}

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "remove object (%ld;%ld) : done\n",
	       set->id, objid);

	return 0;
}

static inline void __export_semarray(struct rpc_desc *desc,
				     const semarray_object_t *sem_object,
				     const struct sem_array* sma)
{
	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 2,
	       "Exporting semarray %d\n", sma->sem_id);

	rpc_pack(desc, 0, sma, sizeof(struct sem_array));
	rpc_pack(desc, 0, sma->sem_base, sma->sem_nsems * sizeof (struct sem));

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Exporting semarray %d: DONE\n", sma->sem_id);
}

static inline void __export_semundos(struct rpc_desc *desc,
				     const struct sem_array* sma)
{
	long nb_semundo = 0;
	struct sem_undo *un;
	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 2,
	       "->  sma: %d\n", sma->sem_id);

	for (un = sma->undo; un; un = un->id_next)
		nb_semundo++;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Exporting %ld semundos for sem %d\n", nb_semundo, sma->sem_id);

	rpc_pack_type(desc, nb_semundo);

	for (un = sma->undo; un; un = un->id_next) {
		DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
		       "* Exporting semundo %d,%ld\n",
		       sma->sem_id, un->proc_list_id);
		BUG_ON(un->proc_next);

		rpc_pack(desc, 0, un, sizeof(struct sem_undo) +
			 sma->sem_nsems * sizeof(short));
	}
	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Exporting semundos for sem %d: DONE\n", sma->sem_id);
}

static inline void __export_one_local_semqueue(struct rpc_desc *desc,
					       const struct sem_queue* q)
{
	/* Fill q2->sleeper with the pid (and not tgid) of q->sleeper
	   (needed to be thread aware) */
	struct sem_queue q2 = *q;

	/* Make remote_sleeper_pid(q2) equal to q->sleeper->pid */
	q2.sleeper = (void*)((long)(q->sleeper->pid));


	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
	       "sma: %d, pid %d\n",
	       q->id, q->pid);

	rpc_pack_type(desc, q2);
	if (q->nsops) {
		rpc_pack(desc, 0, q->sops,
			 q->nsops * sizeof(struct sembuf));
	} else {
		DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
		       "no sops to export\n");
	}

	if (q->undo) {
		DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
		       "sma: %d, proc_list_id: %ld\n",
		       q->id, q->undo->proc_list_id);
		BUG_ON(q->undo->proc_next);

		rpc_pack_type(desc, q->undo->proc_list_id);
	} else {
		DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
		       "no undo to export\n");
	}

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
	       "sma: %d, pid %d: done\n",
	       q->id, q->pid);
}

static inline void __export_one_remote_semqueue(struct rpc_desc *desc,
						const struct sem_queue* q)
{
	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
	       "-> export remote sem_queue (sma: %d, tgid: %d, pid: %d)\n",
	       q->id, q->pid, remote_sleeper_pid(q));

	rpc_pack(desc, 0, q, sizeof(struct sem_queue));
	if (q->nsops)
		rpc_pack(desc, 0, q->sops,
			 q->nsops * sizeof(struct sembuf));

	if (q->undo) {
		DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
		       "sma: %d, proc_list_id: %ld",
		       q->id, q->undo->proc_list_id);
		BUG_ON(q->undo->proc_next);

		rpc_pack_type(desc, q->undo->proc_list_id);
	}

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
	       "<- export remote sem_queue (sma: %d, "
	       "tgid: %d, pid: %d): done\n",
	       q->id, q->pid, remote_sleeper_pid(q));
}

static inline void __export_semqueues(struct rpc_desc *desc,
				      const struct sem_array* sma)
{
	struct sem_queue *q;
	long nb_sem_pending = 0;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 2,
	       "Exporting semqueues for sem %d\n", sma->sem_id);

	/* count local sem_pending */
	for (q = sma->sem_pending; q; q = q->next)
		nb_sem_pending++;

	/* count remote sem_pending */
	for (q = sma->remote_sem_pending; q; q = q->next)
		nb_sem_pending++;

	rpc_pack_type(desc, nb_sem_pending);

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Exporting %ld semqueues for sem %d\n",
	       nb_sem_pending, sma->sem_id);

	/* send local sem_queues */
	for (q = sma->sem_pending; q; q = q->next)
		__export_one_local_semqueue(desc, q);

	/* send remote sem_queues */
	for (q = sma->remote_sem_pending; q; q = q->next)
		__export_one_remote_semqueue(desc, q);

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Exporting semqueues for sem %d: done\n", sma->sem_id);
}

/** Export an object
 *  @author Matthieu Fertré, Renaud Lottiaux
 *
 *  @param  buffer    Buffer to export object data in.
 *  @param  object    The object to export data from.
 */
int semarray_export_object (struct rpc_desc *desc,
			    struct kddm_obj *obj_entry)
{
	semarray_object_t *sem_object;
	struct sem_array *sma;

	sem_object = obj_entry->object;
	sma = sem_object->local_sem;

	BUG_ON(!sma);

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3, "export semaphore %d\n",
	       sma->sem_id);

	__export_semarray(desc, sem_object, sma);
	__export_semundos(desc, sma);
	__export_semqueues(desc, sma);

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3, "export semaphore %d: done\n",
	       sma->sem_id);

	return 0;
}

static inline int __import_semarray(struct rpc_desc *desc,
				    semarray_object_t *sem_object)
{
	struct sem_array buffer;
	int size_sems;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 2,
	       "Importing semarray...\n");

	rpc_unpack_type(desc, buffer);
	sem_object->imported_sem = buffer;

	size_sems = sem_object->imported_sem.sem_nsems * sizeof(struct sem);
	if (!sem_object->mobile_sem_base)
		sem_object->mobile_sem_base = kmalloc(size_sems, GFP_KERNEL);
	if (!sem_object->mobile_sem_base)
		return -ENOMEM;

	rpc_unpack(desc, 0, sem_object->mobile_sem_base, size_sems);
	sem_object->imported_sem.sem_base = sem_object->mobile_sem_base;

	sem_object->imported_sem.sem_pending = NULL;
	sem_object->imported_sem.sem_pending_last = NULL;

	sem_object->imported_sem.remote_sem_pending = NULL;
	sem_object->imported_sem.remote_sem_pending_last =
		&sem_object->imported_sem.remote_sem_pending;

	sem_object->imported_sem.undo = NULL;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 2,
	       "Importing semarray for sem %d\n", sem_object->imported_sem.sem_id);
	return 0;
}

static inline int __import_semundos(struct rpc_desc *desc,
				    struct sem_array *sma)
{
	struct sem_undo* undo;
	long nb_semundo, i;
	int size_undo;
	size_undo = sizeof(struct sem_undo) +
		sma->sem_nsems * sizeof(short);

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 1, "Import semundos for sem %d\n",
	       sma->sem_id);

	rpc_unpack_type(desc, nb_semundo);
	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Waiting to receive %ld semundos for sem %d\n",
	       nb_semundo, sma->sem_id);

	sma->undo = NULL;
	for (i=0; i < nb_semundo; i++) {
		undo = kzalloc(size_undo, GFP_KERNEL);
		if (!undo)
			goto unalloc_undos;

		rpc_unpack(desc, 0, undo, size_undo);
		DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
		       "* Importing semundo %d,%ld\n",
		       sma->sem_id, undo->proc_list_id);
		BUG_ON(undo->proc_next);

		undo->semadj = (short *) &undo[1];
		undo->id_next = sma->undo;
		sma->undo = undo;
	}

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
	       "Importing semundos for sem %d\n", sma->sem_id);
	return 0;

unalloc_undos:
	return -ENOMEM;
}

static inline void __unimport_semundos(struct sem_array *sma)
{
	struct sem_undo * undo, *next;
	for (undo = sma->undo; undo; undo = next) {
		next = undo->id_next;
		kfree(undo);
	}
	sma->undo = NULL;
}

static inline int import_one_semqueue(struct rpc_desc *desc,
				      struct sem_array *sma)
{
	unique_id_t undo_proc_list_id;
	struct sem_undo* undo;
	int r = -ENOMEM;
	struct sem_queue *q = kmalloc(sizeof(struct sem_queue), GFP_KERNEL);
	if (!q)
		goto exit;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 4,
	       "-> import sem_queue (sma: %d, pid ?)\n",
	       sma->sem_id);

	rpc_unpack(desc, 0, q, sizeof(struct sem_queue));

	if (q->nsops) {
		q->sops = kzalloc(q->nsops * sizeof(struct sembuf),
				  GFP_KERNEL);
		if (!q->sops)
			goto unalloc_q;
		rpc_unpack(desc, 0, q->sops, q->nsops * sizeof(struct sembuf));
	} else {
		DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
		       "no sops to import\n");
	}

	if (q->undo) {
		rpc_unpack_type(desc, undo_proc_list_id);
		DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5, "proc_list_id: %ld\n",
		       undo_proc_list_id);
		for (undo = sma->undo; undo; undo = undo->id_next)
			if (undo->proc_list_id == undo_proc_list_id) {
				BUG_ON(undo->proc_next);
				q->undo = undo;
				goto undo_found;
			}
		BUG();
	} else {
		DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 5,
		       "no undo to import\n");
	}
undo_found:
	r = 0;
	q->prev = NULL;
	q->next = NULL;
	append_to_remote_queue(sma, q); /* split between remote and local
					   queues is done in update_local_sem */
	BUG_ON(!q->sleeper);
	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 4,
	       "<- import sem_queue (sma: %d, tgid: %d, pid: %d): done\n",
	       sma->sem_id, q->pid, remote_sleeper_pid(q));
	return r;

unalloc_q:
	kfree(q);

exit:
	return r;
}

static inline int __import_semqueues(struct rpc_desc *desc,
				     struct sem_array *sma)
{
	long nb_sempending, i;
	int r;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 2,
	       "Importing semqueues for sem %d\n", sma->sem_id);

	r = rpc_unpack_type(desc, nb_sempending);
	if (r)
		goto err;

	BUG_ON(sma->remote_sem_pending);

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Importing %ld semqueues for sem %d\n",
	       nb_sempending, sma->sem_id);

	for (i=0; i < nb_sempending; i++) {
		r = import_one_semqueue(desc, sma);
		if (r)
			goto err;
	}

#ifdef CONFIG_KRG_DEBUG
	{
		struct sem_queue *q;
		i=0;
		for (q = sma->remote_sem_pending;
		     q ; q = q->next)
			i++;
		BUG_ON(nb_sempending != i);
	}
#endif

err:
	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Importing semqueues for sem %d: DONE - r=%d\n", sma->sem_id, r);

	return r;
}

static inline void __unimport_semqueues(struct sem_array *sma)
{
	struct sem_queue *q, *next;

	for (q = sma->remote_sem_pending; q; q = next) {
		next = q->next;
		remove_from_remote_queue(sma, q, 1);
	}
}

/** Import an object
 *  @author Matthieu Fertré, Renaud Lottiaux
 *
 *  @param  object    The object to import data in.
 *  @param  buffer    Data to import in the object.
 */
int semarray_import_object (struct kddm_obj *obj_entry,
			    struct rpc_desc *desc)
{
	semarray_object_t *sem_object;
	int r = 0;
	sem_object = obj_entry->object;

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 2,
	       "Importing sem (%p)\n", sem_object);

	r = __import_semarray(desc, sem_object);
	if (r)
		goto err;

	r = __import_semundos(desc, &sem_object->imported_sem);
	if (r)
		goto unimport_semundos;

	r = __import_semqueues(desc, &sem_object->imported_sem);
	if (r)
		goto unimport_semqueues;

	goto err;

unimport_semqueues:
	__unimport_semqueues(&sem_object->imported_sem);

unimport_semundos:
	__unimport_semundos(&sem_object->imported_sem);

err:
	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 3,
	       "Importing sem %d (%p - %p): DONE - r=%d\n",
	       sem_object->imported_sem.sem_id, sem_object,
	       obj_entry->object, r);
	return r;
}



/****************************************************************************/

/* Init the semarray IO linker */

struct iolinker_struct semarray_linker = {
	first_touch:       semarray_first_touch,
	remove_object:     semarray_remove_object,
	invalidate_object: semarray_invalidate_object,
	insert_object:     semarray_insert_object,
	linker_name:       "semarray",
	linker_id:         SEMARRAY_LINKER,
	alloc_object:      semarray_alloc_object,
	export_object:     semarray_export_object,
	import_object:     semarray_import_object
};



/*****************************************************************************/
/*                                                                           */
/*                         SEMKEY KDDM IO FUNCTIONS                          */
/*                                                                           */
/*****************************************************************************/



/****************************************************************************/

/* Init the sem key IO linker */

struct iolinker_struct semkey_linker = {
	linker_name:       "semkey",
	linker_id:         SEMKEY_LINKER,
};
