/** All the code for IPC messages accross the cluster
 *  @file msg_handler.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2007-2008 Matthieu Fertré - INRIA
 */

#ifndef NO_MSG

#include "debug_keripc.h"

#define MODULE_NAME "IPC MSG"

#include <linux/msg.h>
#include <linux/syscalls.h>

#include <ctnr/kddm.h>
#include <hotplug/hotplug.h>
#include <kerrighed/msg.h>
#include <kerrighed/signal.h>
#include <rpc/rpc.h>
#include "ipc_handler.h"
#include "msg_handler.h"
#include "msg_io_linker.h"
#include "ipcmap_io_linker.h"

/* Kddm set of msg ids structures */
struct kddm_set *msq_struct_kddm_set = NULL;
struct kddm_set *msqkey_struct_kddm_set = NULL;

/* Kddm set of IPC allocation bitmap structures */
struct kddm_set *msgmap_struct_kddm_set = NULL;

/* Kddm set of IPC msg master node */
struct kddm_set *msq_master_kddm_set = NULL;

/*****************************************************************************/
/*                                                                           */
/*                                KERNEL HOOKS                               */
/*                                                                           */
/*****************************************************************************/

struct msg_queue *kcb_ipcmsg_lock(struct ipc_namespace *ns, int id)
{
	msq_object_t *msq_object;
	struct msg_queue *msq;
	int index;

	BUG_ON (ns != &init_ipc_ns);

	index = id % SEQ_MULTIPLIER;

	DEBUG (DBG_KERIPC_MSG_LOCK, 1, "->Lock msg queue %d(%d)\n", id, index);

	msq_object = _kddm_grab_object_no_ft(msq_struct_kddm_set, index);

	if (!msq_object) {
		_kddm_put_object(msq_struct_kddm_set, index);
		return NULL;
	}
	BUG_ON(!msq_object);
	msq = msq_object->local_msq;

	BUG_ON(!msq);

	DEBUG (DBG_KERIPC_MSG_LOCK, 5, "--> msq->q_perm.lock\n");
	mutex_lock(&msq->q_perm.mutex);
	DEBUG (DBG_KERIPC_MSG_LOCK, 5, "<-- msq->q_perm.lock\n");

	if (msq->q_perm.deleted) {
		mutex_unlock(&msq->q_perm.mutex);
		msq = NULL;
	}

	DEBUG (DBG_KERIPC_MSG_LOCK, 1, "<- Lock msg queue %d(%d) done: %p\n",
	       id, index, msq);

	return msq;
}



void kcb_ipcmsg_unlock(struct msg_queue *msq)
{
	int index ;

	index = msq->q_id % SEQ_MULTIPLIER;

	DEBUG (DBG_KERIPC_MSG_LOCK, 1, "-> Unlock msg queue %d(%d) - %p\n",
	       msq->q_id, index, msq);

	if (msq->q_perm.deleted) {
		DEBUG (DBG_KERIPC_MSG_LOCK, 1, "-> Unlock msq %d(%d) deleted\n",
		       msq->q_id, index);
		return;
	}

	_kddm_put_object(msq_struct_kddm_set, index);

	mutex_unlock(&msq->q_perm.mutex);

	DEBUG (DBG_KERIPC_MSG_LOCK, 1, "<- Unlock msg queue %d(%d) : done\n",
	       msq->q_id, index);
}

/** Notify the creation of a new IPC msg queue to Kerrighed.
 *
 *  @author Matthieu Fertré
 */
int kcb_ipcmsg_newque(struct ipc_namespace *ns, struct msg_queue *msq)
{
	msq_object_t *msq_object;
	kerrighed_node_t *master_node;
	long *key_index;
	int index ;

	BUG_ON (ns != &init_ipc_ns);

	index = msq->q_id % SEQ_MULTIPLIER;

	DEBUG (DBG_KERIPC_MSG, 2, "New msg queue : id %d(%d) (%p)\n", msq->q_id,
	       index, msq);

	msq_object = _kddm_grab_object_manual_ft(msq_struct_kddm_set, index);

	BUG_ON(msq_object);

	msq_object = kmem_cache_alloc(msq_object_cachep, GFP_KERNEL);
	if (!msq_object)
		return -ENOMEM;

	msq_object->local_msq = msq;
	msq_object->local_msq->is_master = 1;
	msq_object->mobile_msq.q_id = -1;

	_kddm_set_object(msq_struct_kddm_set, index, msq_object);

	if (msq->q_perm.key != IPC_PRIVATE)
	{
		key_index = _kddm_grab_object(msqkey_struct_kddm_set,
					      msq->q_perm.key);
		*key_index = index;
		_kddm_put_object (msqkey_struct_kddm_set, msq->q_perm.key);
	}

	master_node = _kddm_grab_object(msq_master_kddm_set, index);
	*master_node = kerrighed_node_id;
	_kddm_put_object(msq_master_kddm_set, index);

	_kddm_put_object(msq_struct_kddm_set, index);

	DEBUG (DBG_KERIPC_MSG, 2, "New msg queue (id %d(%d)) : done\n",
	       msq->q_id, index);

	return 0;
}

long kcb_ipcmsg_rmid(int msqid)
{
	struct rpc_desc * desc;
	kerrighed_node_t* master_node;
	long r;
	int index;

	index = msqid % SEQ_MULTIPLIER;
	DEBUG (DBG_KERIPC_MSG, 2, "Del msg queue %d\n", msqid);

	master_node = _kddm_get_object_no_ft(msq_master_kddm_set, index);
	if (!master_node || *master_node == kerrighed_node_id)
		BUG();
	desc = rpc_begin(IPC_MSG_RMQUEUE, *master_node);
	_kddm_put_object(msq_master_kddm_set, index);

	rpc_pack_type(desc, msqid);
	rpc_unpack_type(desc, r);
	rpc_end(desc, 0);

	return r;
}

static void handle_rm_msg_queue(struct rpc_desc *desc, void *_msg, size_t size)
{
	int* msqid = _msg;
	long r;
	DEBUG (DBG_KERIPC_MSG, 2, " %d\n", *msqid);

	r = sys_msgctl(*msqid, IPC_RMID, NULL);
	rpc_pack_type(desc, r);
}

void kcb_ipcmsg_freeque(struct ipc_namespace *ns, struct msg_queue *msq)
{
	int index;
	key_t key;

	index = msq->q_id % SEQ_MULTIPLIER;
	key = msq->q_perm.key;

	DEBUG (DBG_KERIPC_MSG, 2, "Destroy msg queue %d(%d)\n", msq->q_id,
	       index);

	if (key != IPC_PRIVATE) {
		_kddm_grab_object_no_ft(msqkey_struct_kddm_set, key);
		_kddm_remove_frozen_object(msqkey_struct_kddm_set, key);
	}

	_kddm_grab_object_no_ft(msq_master_kddm_set, index);
	_kddm_remove_frozen_object(msq_master_kddm_set, index);

	DEBUG (DBG_KERIPC_MSG, 2, "REALLY Destroy msg queue %d(%d)\n", msq->q_id,
	       index);
	local_msg_unlock(msq);

	_kddm_remove_frozen_object(msq_struct_kddm_set, index);

	free_ipc_id (&msg_ids(ns), index);
}

/*****************************************************************************/

struct msgsnd_msg
{
	kerrighed_node_t requester;
	int msqid;
	long mtype;
	int msgflg;
	pid_t tgid;
};

long kcb_ipc_msgsnd(int msqid, long mtype, void __user *mtext,
		    size_t msgsz, int msgflg, struct ipc_namespace *ns,
		    pid_t tgid)
{
	struct rpc_desc * desc;
	kerrighed_node_t* master_node;
	void *buffer;
	long r;
	enum rpc_error err;
	int index;
	struct msgsnd_msg msg;

	DEBUG (DBG_KERIPC_MSG, 2, "-> Begin - msqid: %d, mtype: %ld, "
	       "tgid: %d, size %zd\n",
	       msqid, mtype, tgid, msgsz);

	msg.requester = kerrighed_node_id;
	msg.msqid = msqid;
	msg.mtype = mtype;
	msg.msgflg = msgflg;
	msg.tgid = tgid;

	buffer = kmalloc(msgsz, GFP_KERNEL);
	r = copy_from_user(buffer, mtext, msgsz);
	if (r)
		goto exit;

	/* TODO: manage ipc namespace */
	index = msqid % SEQ_MULTIPLIER;
	master_node = _kddm_get_object_no_ft(msq_master_kddm_set, index);
	if (!master_node) {
		_kddm_put_object(msq_master_kddm_set, index);
		r = -EINVAL;
		goto exit;
	}

	if (*master_node == kerrighed_node_id) {
		/* inverting the following 2 lines can conduct to deadlock
		 * if the send is blocked */
		_kddm_put_object(msq_master_kddm_set, index);
		r = __do_msgsnd(msqid, mtype, mtext, msgsz,
				msgflg, ns, tgid);
		goto exit;
	}

	desc = rpc_begin(IPC_MSG_SEND, *master_node);
	_kddm_put_object(msq_master_kddm_set, index);

	DEBUG (DBG_KERIPC_MSG, 2, "contact node %d for msgq %d(%d)\n",
	       *master_node, msqid, index);

	rpc_pack_type(desc, msg);
	rpc_pack_type(desc, msgsz);
	rpc_pack(desc, 0, buffer, msgsz);

	DEBUG (DBG_KERIPC_MSG, 2, "Waiting answer for msgq %d(%d)\n",
	       msqid, index);

	err = rpc_unpack(desc, RPC_FLAGS_INTR, &r, sizeof(r));
	if (err == RPC_EINTR) {
		rpc_signal(desc, next_signal(&current->pending,
					     &current->blocked));
		r = -EINTR;
	}

	DEBUG (DBG_KERIPC_MSG, 2, "<- End with result %ld\n", r);

	rpc_end(desc, 0);

exit:
	kfree(buffer);
	return r;
}

static void handle_do_msg_send(struct rpc_desc *desc, void *_msg, size_t size)
{
	size_t msgsz;
	void *mtext;
	long r;
	sigset_t sigset, oldsigset;
	struct msgsnd_msg *msg = _msg;

	DEBUG (DBG_KERIPC_MSG, 2, "-> Begin msqid: %d, msgtyp: %ld, tgid: %d\n",
	       msg->msqid, msg->mtype, msg->tgid);

	rpc_unpack_type(desc, msgsz);

	DEBUG (DBG_KERIPC_MSG, 5, "size: %zd\n", msgsz);

	mtext = kmalloc(msgsz, GFP_KERNEL);

	rpc_unpack(desc, 0, mtext, msgsz);

	DEBUG (DBG_KERIPC_MSG, 3, "** calling __do_msgsnd\n");

	sigfillset(&sigset);
	sigprocmask(SIG_UNBLOCK, &sigset, &oldsigset);

	r = __do_msgsnd(msg->msqid, msg->mtype, mtext, msgsz, msg->msgflg,
			&init_ipc_ns, /* TODO: replace by correct namespace */
			msg->tgid);

	sigprocmask(SIG_SETMASK, &oldsigset, NULL);
	flush_signals(current);

	DEBUG (DBG_KERIPC_MSG, 3, "** Sending answer %ld for msgq %d\n",
	       r, msg->msqid);

	rpc_pack_type(desc, r);

	DEBUG (DBG_KERIPC_MSG, 2, "<- End \n");

	kfree(mtext);
}

struct msgrcv_msg
{
	kerrighed_node_t requester;
	int msqid;
	long msgtyp;
	int msgflg;
	pid_t tgid;
};

long kcb_ipc_msgrcv(int msqid, long *pmtype, void __user *mtext,
		    size_t msgsz, long msgtyp, int msgflg,
		    struct ipc_namespace *ns, pid_t tgid)
{
	struct rpc_desc * desc;
	enum rpc_error err;

	kerrighed_node_t *master_node;
	void * buffer;
	long r;
	int retval;
	int index;
	struct msgrcv_msg msg;
	msg.requester = kerrighed_node_id;
	msg.msqid = msqid;
	msg.msgtyp = msgtyp;
	msg.msgflg = msgflg;
	msg.tgid = tgid;

	DEBUG (DBG_KERIPC_MSG, 2, "-> Begin msqid: %d, msgtyp: %ld, tgid: %d\n",
	       msqid, msgtyp, tgid);

	/* TODO: manage ipc namespace */
	index = msqid % SEQ_MULTIPLIER;

	master_node = _kddm_get_object_no_ft(msq_master_kddm_set, index);
	if (!master_node) {
		_kddm_put_object(msq_master_kddm_set, index);
		return -EINVAL;
	}

	if (*master_node == kerrighed_node_id) {
		/*inverting the following 2 lines can conduct to deadlock
		 * if the receive is blocked */
		_kddm_put_object(msq_master_kddm_set, index);
		r = __do_msgrcv(msqid, pmtype, mtext, msgsz, msgtyp,
				msgflg, ns, tgid);
		return r;
	}

	desc = rpc_begin(IPC_MSG_RCV, *master_node);
	_kddm_put_object(msq_master_kddm_set, index);

	DEBUG (DBG_KERIPC_MSG, 2, "contact node %d for msgq %d(%d)\n",
	       *master_node, msqid, index);

	rpc_pack_type(desc, msg);
	rpc_pack_type(desc, msgsz);

	DEBUG (DBG_KERIPC_MSG, 2, "** Waiting answer (max size=%zd)\n", msgsz);

	err = rpc_unpack(desc, RPC_FLAGS_INTR, &r, sizeof(r));
	if (!err) {
		if (r > 0) {
			/* get the real msg type */
			rpc_unpack(desc, 0, pmtype, sizeof(long));

			buffer = kmalloc(r, GFP_KERNEL);
			rpc_unpack(desc, 0, buffer, r);
			retval = copy_to_user(mtext, buffer, r);
			kfree(buffer);
			if (retval)
				r = retval;
		}
	} else if (err == RPC_EINTR) {
		/* If we have been interrupted by a signal, we forward it
		   to the rpc handler */
		rpc_signal(desc, next_signal(&current->pending,
				     &current->blocked));
		r = -EINTR;
	}

	DEBUG (DBG_KERIPC_MSG, 2, "<- End %ld\n", r);
	rpc_end(desc, 0);
	return r;
}

static void handle_do_msg_rcv(struct rpc_desc *desc, void *_msg, size_t size)
{
	size_t msgsz;
	void *mtext;
	long r;
	long pmtype;
	struct msgrcv_msg *msg = _msg;
	sigset_t sigset, oldsigset;

	DEBUG (DBG_KERIPC_MSG, 2, "-> Begin - msqid: %d, msgtyp: %ld, tgid: %d\n",
	       msg->msqid, msg->msgtyp, msg->tgid);

	rpc_unpack_type(desc, msgsz);

	DEBUG (DBG_KERIPC_MSG, 2, "** msqid: %d, msgtyp: %ld, tgid: %d, max size: %zd\n",
	       msg->msqid, msg->msgtyp, msg->tgid, msgsz);

	mtext = kmalloc(msgsz, GFP_KERNEL);

	sigfillset(&sigset);
	sigprocmask(SIG_UNBLOCK, &sigset, &oldsigset);

	r = __do_msgrcv(msg->msqid, &pmtype, mtext, msgsz,
			msg->msgtyp, msg->msgflg,
			&init_ipc_ns, /* TODO: support namespace */
			msg->tgid);

	sigprocmask(SIG_SETMASK, &oldsigset, NULL);
	flush_signals(current);

	DEBUG (DBG_KERIPC_MSG, 2, "** sending %ld bytes\n", r);
	rpc_pack_type(desc, r);
	if (r > 0) {
		rpc_pack_type(desc, pmtype); /* send the real type of msg */
		rpc_pack(desc, 0, mtext, r);
	}

	DEBUG (DBG_KERIPC_MSG, 2, "<- End\n");
	kfree(mtext);
}


/*****************************************************************************/
/*                                                                           */
/*                              INITIALIZATION                               */
/*                                                                           */
/*****************************************************************************/



void msg_handler_init (void)
{
	DEBUG (DBG_KERIPC_INITS, 1, "Start\n");

	msq_object_cachep = kmem_cache_create("msg_queue_object",
					      sizeof(msq_object_t),
					      0, SLAB_PANIC, NULL, NULL);

	register_io_linker(MSG_LINKER, &msq_linker);
	register_io_linker(MSGKEY_LINKER, &msqkey_linker);
	register_io_linker(MSGMASTER_LINKER, &msqmaster_linker);

	msq_struct_kddm_set = create_new_kddm_set (kddm_def_ns,
						   MSG_KDDM_ID,
						   MSG_LINKER,
						   KDDM_RR_DEF_OWNER,
						   sizeof(msq_object_t),
						   KDDM_LOCAL_EXCLUSIVE);

	BUG_ON (IS_ERR (msq_struct_kddm_set));

	msqkey_struct_kddm_set = create_new_kddm_set (kddm_def_ns,
						      MSGKEY_KDDM_ID,
						      MSGKEY_LINKER,
						      KDDM_RR_DEF_OWNER,
						      sizeof(long), 0);

	BUG_ON (IS_ERR (msqkey_struct_kddm_set));

	msgmap_struct_kddm_set = create_new_kddm_set (kddm_def_ns,
						      MSGMAP_KDDM_ID,
						      IPCMAP_LINKER,
						      KDDM_RR_DEF_OWNER,
						      sizeof(ipcmap_object_t),
						      0);

	BUG_ON (IS_ERR (msgmap_struct_kddm_set));

	msq_master_kddm_set = create_new_kddm_set(kddm_def_ns,
						  MSGMASTER_KDDM_ID,
						  MSGMASTER_LINKER,
						  KDDM_RR_DEF_OWNER,
						  sizeof(kerrighed_node_t),
						  KDDM_LOCAL_EXCLUSIVE);

	msg_ids(&init_ipc_ns).id_ctnr = MSG_KDDM_ID;
	msg_ids(&init_ipc_ns).key_ctnr = MSGKEY_KDDM_ID;
	msg_ids(&init_ipc_ns).map_ctnr = MSGMAP_KDDM_ID;

	hook_register(&kh_ipcmsg_lock, kcb_ipcmsg_lock);
	hook_register(&kh_ipcmsg_unlock, kcb_ipcmsg_unlock);
	hook_register(&kh_ipcmsg_newque, kcb_ipcmsg_newque);
	hook_register(&kh_ipcmsg_freeque, kcb_ipcmsg_freeque);
	hook_register(&kh_ipcmsg_rmid, kcb_ipcmsg_rmid);
	hook_register(&kh_ipc_msgsnd, kcb_ipc_msgsnd);
	hook_register(&kh_ipc_msgrcv, kcb_ipc_msgrcv);

	rpc_register_void(IPC_MSG_SEND, handle_do_msg_send, 0);
	rpc_register_void(IPC_MSG_RCV, handle_do_msg_rcv, 0);
	rpc_register_void(IPC_MSG_RMQUEUE, handle_rm_msg_queue, 0);

	DEBUG (DBG_KERIPC_INITS, 1, "IPC Msg Server configured\n");
}



void msg_handler_finalize (void)
{
}

#endif
