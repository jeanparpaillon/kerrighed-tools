#ifndef __KDDM_SET_TYPES__
#define __KDDM_SET_TYPES__

#include "kddm_tree.h"
#include <linux/wait.h>
#include <kerrighed/types.h>
#include <rpc/rpc.h>


/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              OBJECT TYPES                                *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/*********************     Object states    ***********************/
//
//                                            +------------------------------------------- Object state
//                                            |
//                         |-----------------------------------|
// |31 30 29 28 27 26 25 24|23|22|21|20|19 18 17 16 15 14 13 12|11|10|9|8|7 6 5 4 3 2 1 0|
// |-----------------------|--|--|--|--|-----------------------|--|--|-|-|---------------|
//             |             |  |  |  |            |             |  | | |        |
//             |             |  |  |  |            |             |  | | |        +-------- Reserved
//             |             |  |  |  |            |             |  | | +----------------- Pinned flag
//             |             |  |  |  |            |             |  | +------------------- SEND_RM_ACK2 flag
//             |             |  |  |  |            |             |  +--------------------- Failure Flag
//             |             |  |  |  |            |             +------------------------ Unused flag
//             |             |  |  |  |            +-------------------------------------- Object state index
//             |             |  |  |  +--------------------------------------------------- Owner flag
//             |             |  |  +------------------------------------------------------ Read access flag
//             |             |  +--------------------------------------------------------- Write access flag
//             |             +------------------------------------------------------------ Unused
//             +-------------------------------------------------------------------------- Probe Owner

/* Reserved bits. For future short entry usage */
#define OBJ_ENTRY_RESERVED0 0x00000001  /* Bit reserved for external usage */
#define OBJ_ENTRY_RESERVED1 0x00000002  /* Bit reserved for external usage */

/* Various object flags */
#define OBJECT_PINNED       0x00000100  /* Lock the object to give waiting
					   processes a change to access
				           the object before a potential
				           invalidation */
#define SEND_RM_ACK2        0x00000200  /* The default owner need an ack2 after
					   a global remove is done */
#define FAILURE_FLAG        0x00000400

/* Object state */
#define STATE_INDEX_MASK    0x000FF000  /* Mask to extract the state index */
#define STATE_INDEX_SHIFT   12

#define KDDM_OWNER_OBJ      0x00100000  /* Object is the master object */
#define KDDM_READ_OBJ       0x00200000  /* Object can be read */
#define KDDM_WRITE_OBJ      0x00400000  /* Object can be write */

#define OBJECT_STATE_MASK   0x00FFF000

/* Probe owner */
#define PROB_OWNER_MASK     0xFF000000
#define PROB_OWNER_SHIFT    24



/* Helper macros */

#define SET_OBJECT_PINNED(obj_entry) \
        (obj_entry)->flags |= OBJECT_PINNED
#define CLEAR_OBJECT_PINNED(obj_entry) \
        (obj_entry)->flags = (obj_entry)->flags & ~OBJECT_PINNED
#define TEST_OBJECT_PINNED(obj_entry) \
        ((obj_entry)->flags & OBJECT_PINNED)

#define SET_OBJECT_FLAG(obj_entry,flag) \
        (obj_entry)->flags |= (flag)
#define TEST_OBJECT_FLAG(obj_entry,flag) \
        ((obj_entry)->flags & (flag))
#define CLEAR_OBJECT_FLAG(obj_entry,flag) \
        (obj_entry)->flags &= ~(flag)

#define OBJ_STATE(object) \
        (int)((object)->flags & OBJECT_STATE_MASK)
#define OBJ_STATE_INDEX(state) \
        (((state) & STATE_INDEX_MASK) >> STATE_INDEX_SHIFT)
#define STATE_NAME(state) \
        state_name[OBJ_STATE_INDEX(state)]
#define INC_STATE_COUNTER(state) \
        atomic_inc (&nr_OBJ_STATE[OBJ_STATE_INDEX(state)])
#define DEC_STATE_COUNTER(state) \
        atomic_inc (&nr_OBJ_STATE[OBJ_STATE_INDEX(state)])

#define SET_FAILURE_FLAG(obj_entry) \
        (obj_entry)->flags |= FAILURE_FLAG
#define CLEAR_FAILURE_FLAG(obj_entry) \
        (obj_entry)->flags = (obj_entry)->flags & ~FAILURE_FLAG
#define TEST_FAILURE_FLAG(obj_entry) \
        ((obj_entry)->flags & FAILURE_FLAG)


/** Object states used for the coherence protocol */

typedef enum {
	INV_COPY = 0,
	READ_COPY =         1 << STATE_INDEX_SHIFT | KDDM_READ_OBJ,
	
	INV_OWNER =         2 << STATE_INDEX_SHIFT | KDDM_OWNER_OBJ,
	READ_OWNER =        3 << STATE_INDEX_SHIFT | KDDM_OWNER_OBJ | KDDM_READ_OBJ,
	WRITE_OWNER =       4 << STATE_INDEX_SHIFT | KDDM_OWNER_OBJ | KDDM_READ_OBJ | KDDM_WRITE_OBJ,
	WRITE_GHOST =       5 << STATE_INDEX_SHIFT | KDDM_OWNER_OBJ | KDDM_READ_OBJ | KDDM_WRITE_OBJ,
	
	WAIT_ACK_INV =      6 << STATE_INDEX_SHIFT | KDDM_READ_OBJ,
	WAIT_ACK_WRITE =    7 << STATE_INDEX_SHIFT | KDDM_OWNER_OBJ | KDDM_READ_OBJ,
	WAIT_CHG_OWN_ACK =  8 << STATE_INDEX_SHIFT | KDDM_READ_OBJ,
	WAIT_RECEIVED_ACK =    9 << STATE_INDEX_SHIFT,

	WAIT_OBJ_READ =    10 << STATE_INDEX_SHIFT,
	WAIT_OBJ_WRITE =   11 << STATE_INDEX_SHIFT,
	
	INV_NO_COPY =      12 << STATE_INDEX_SHIFT,
	
	WAIT_OBJ_RM_DONE = 13 << STATE_INDEX_SHIFT,
	WAIT_OBJ_RM_ACK =  14 << STATE_INDEX_SHIFT,
	WAIT_OBJ_RM_ACK2 = 15 << STATE_INDEX_SHIFT,
	
	INV_FILLING =      16 << STATE_INDEX_SHIFT,

	NB_OBJ_STATE =     17 /* MUST always be the last one */
} kddm_obj_state_t;



/** kddm object identifier */
typedef unsigned long objid_t;


/** Master object type.
 *  Type used to store the copy set.
 */
typedef struct {
	krgnodemask_t copyset;   /**< Machines owning an object to invalidate */
	krgnodemask_t rmset;     /**< Machines owning an object to remove */
} masterObj_t;



/** Kddm object type.
 *  Used to store local informations on objects.
 */
typedef struct kddm_obj {
	/* flags field must be kept first in the structure */
	long flags;                    /* Flags, state, prob_owner, etc... */
	masterObj_t master_obj;        /* Object informations handled by the
					  manager */
	void *object;                  /* Kernel physical object struct */
	int countx;                    /* Object count debugging */
	atomic_t frozen_count;         /* Number of task freezing the object */
	atomic_t sleeper_count;        /* Nunmber of task waiting on the
					  object */
	wait_queue_head_t waiting_tsk; /* Process waiting for the object */
} kddm_obj_t;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                               KDDM SET TYPES                             *
 *                                                                          *
 *--------------------------------------------------------------------------*/

/** KDDM set flags */
#define KDDM_LOCAL_EXCLUSIVE  0x00000001
#define KDDM_FT_LINKED        0x00000002


#define NR_OBJ_ENTRY_LOCKS 16


struct kddm_set;
typedef struct kddm_set_ops {
	void *(*obj_set_alloc) (struct kddm_set *set, void *data);
	void (*obj_set_free) (void *tree,
			      int (*f)(unsigned long, void *data,void *priv),
			      void *priv);
	struct kddm_obj *(*lookup_obj_entry)(struct kddm_set *set,
					     objid_t objid);
	struct kddm_obj *(*get_obj_entry)(struct kddm_set *set,
					  objid_t objid, struct kddm_obj *obj);
	void (*insert_object)(struct kddm_set * set, objid_t objid,
			      struct kddm_obj *obj_entry);
	void (*remove_obj_entry) (struct kddm_set *set, objid_t objid);
	void (*for_each_obj_entry)(struct kddm_set *set,
				   int(*f)(unsigned long, void *, void*),
				   void *data);
	void (*export) (struct rpc_desc* desc, struct kddm_set *set);
	void *(*import) (struct rpc_desc* desc, int *free_data);
} kddm_set_ops_t;



typedef unique_id_t kddm_set_id_t;   /**< Kddm set identifier */

typedef int iolinker_id_t;           /**< IO Linker identifier */

/** KDDM set structure */

typedef struct kddm_set {
	void *obj_set;               /**< Structure hosting the set objects */
	spinlock_t table_lock;       /**< Object table lock */
	struct kddm_ns *ns;          /**< kddm set name space */
	struct kddm_set_ops *ops;    /**< kddm set operations */
	kddm_set_id_t id;            /**< kddm set identifier */
	spinlock_t lock;             /**< Structure lock */
	unsigned int obj_size;       /**< size of objects in the set */
	atomic_t nr_objects;         /**< Number of objects locally present */
	unsigned long flags;         /**< Kddm set flags */
	int state;                   /**< State of the set (locked, ...) */
	wait_queue_head_t create_wq; /**< Process waiting for set creation */
	atomic_t usage_count;
	atomic_t count;              /**< Global usage counter */
	unsigned int last_ra_start;  /**< Start of the last readahead window */
	int ra_window_size;          /**< Size of the readahead window */
	kerrighed_node_t def_owner;  /**< Id of default owner node */
	struct iolinker_struct *iolinker;    /**< IO linker ops */
	struct proc_dir_entry *procfs_entry; /**< entry in /proc/kerrighed/kddm */
	
	void *private_data;                  /**< Data used to instantiate */
	int private_data_size;               /**< Size of private data... */

	spinlock_t obj_lock[NR_OBJ_ENTRY_LOCKS];    /**< Objects lock */
	
	atomic_t nr_masters;
	atomic_t nr_copies;
	atomic_t nr_entries;
	event_counter_t get_object_counter;
	event_counter_t grab_object_counter;
	event_counter_t remove_object_counter;
	event_counter_t flush_object_counter;
	void *private;
} kddm_set_t;



struct kddm_info_struct {
	event_counter_t get_object_counter;
	event_counter_t grab_object_counter;
	event_counter_t remove_object_counter;
	event_counter_t flush_object_counter;

	wait_queue_t object_wait_queue_entry;
	struct kddm_obj *wait_obj;
	int ns_id;
	kddm_set_id_t set_id;
	objid_t obj_id;
};

#endif // __KDDM_SET_TYPES__