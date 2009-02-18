/** KDDM debug system.
 *  @file debug_kddm.c
 *
 *  Copyright (C) 2007, Renaud Lottiaux, Kerlabs.
 */

#include <kerrighed/hashtable.h>
#include <kerrighed/ctnr_headers.h>
#include <linux/kallsyms.h>

#include "kddm.h"
#include <tools/debug_color.h>
#include <rpc/rpc.h>
#include "object_server.h"
#include "debug_kddm.h"

#define MAX_LOG_MSG 100000
#define MAX_SET_FILTER 4

static int log_kddm_debugs = 0;

struct dentry *kddm_debug_dentry;
struct dentry *kddm_filter_dentry;
kddm_set_id_t set_filter[MAX_SET_FILTER];
kddm_set_id_t set_filter_min, set_filter_max;
pid_t debug_kddm_last_pid = 0;
unique_id_root_t kddm_req_id_root;

#define REQ_NODE(log) ((int)(log->req_id >> UNIQUE_ID_NODE_SHIFT) & 0xFF)
#define REQ_ID(log) (log->req_id & UNIQUE_ID_LOCAL_MASK)

static int print_set(krgnodemask_t *set, char *buffer)
{
	int i, n=0;

	__for_each_krgnode_mask (i, set) {
		if (buffer)
			n += sprintf (&buffer[n], "%d ", i);
		else
			printk("%d ", i);
	}
	return n;
}



static void print_wq(wait_queue_head_t *q)
{
        struct list_head *tmp, *next;

        list_for_each_safe(tmp, next, &q->task_list) {
                wait_queue_t *curr = list_entry(tmp, wait_queue_t, task_list);
		struct task_struct *tsk = curr->private;

		printk("%s (%d) ", tsk->comm, tsk->pid);
        }
}


int kddm_filter_debug(kddm_set_id_t set_id)
{
	int i ;

	for (i = 0; i < MAX_SET_FILTER; i++)
		if (set_filter[i] == set_id)
			return 1;

	if ((set_filter_min != 0) &&
	    (set_id < set_filter_min))
	    return 0;

	if ((set_filter_max != 0) &&
	    (set_id > set_filter_max))
	    return 0;

	if ((set_filter_min != 0) || (set_filter_max != 0))
		return 1;

	return 0;
}



void kddm_print_log(struct kddm_log *log, char *buffer)
{
	char *modname;
	const char *name;
	unsigned long offset, size;
	char namebuf[KSYM_NAME_LEN+1];
	krgnodemask_t set;
	int n = 0;

	name = kallsyms_lookup(log->fct_addr, &size, &offset, &modname,
			       namebuf);

	if (log->obj_id == -1L)
		n += sprintf (&buffer[n], "%6d - %30.30s (%lu;-) ", log->pid,
			      name, log->set_id);
	else
		n += sprintf (&buffer[n], "%6d - %30.30s (%lu;%lu) ", log->pid,
			      name, log->set_id, log->obj_id);

	switch (log->dbg_id) {
	case KDDM_LOG_ENTER:
		n += sprintf (&buffer[n], "Enter\n");
		break;

	case KDDM_LOG_EXIT:
		n += sprintf (&buffer[n], "Done (err %d)\n", (int)log->data);
		break;

	case KDDM_LOG_ENTER_COUNT:
		n += sprintf (&buffer[n], "Enter - State %s - frozen count "
			      "%ld - sleeper count %ld\n",
			      STATE_NAME(KDDM_LOG_UNPACK4_1(log)),
			      KDDM_LOG_UNPACK4_2(log),
			      KDDM_LOG_UNPACK4_3(log));
		break;

	case KDDM_LOG_ENTER_GET:
		n += sprintf (&buffer[n], "Enter GET - State %s - frozen "
			      "count %ld - sleeper count %ld\n",
			      STATE_NAME(KDDM_LOG_UNPACK4_1(log)),
			      KDDM_LOG_UNPACK4_2(log),
			      KDDM_LOG_UNPACK4_3(log));
		break;

	case KDDM_LOG_ENTER_GRAB:
		n += sprintf (&buffer[n], "Enter GRAB - State %s - frozen "
			      "count %ld - sleeper count %ld\n",
			      STATE_NAME(KDDM_LOG_UNPACK4_1(log)),
			      KDDM_LOG_UNPACK4_2(log),
			      KDDM_LOG_UNPACK4_3(log));
		break;

	case KDDM_LOG_ENTER_PUT:
		n += sprintf (&buffer[n], "Enter PUT - State %s - frozen "
			      "count %ld - sleeper count %ld\n",
			      STATE_NAME(KDDM_LOG_UNPACK4_1(log)),
			      KDDM_LOG_UNPACK4_2(log),
			      KDDM_LOG_UNPACK4_3(log));
		break;

	case KDDM_LOG_EXIT_COUNT:
		n += sprintf (&buffer[n], "Done - State %s - frozen count %ld "
			      "- sleeper count %ld\n",
			      STATE_NAME(KDDM_LOG_UNPACK4_1(log)),
			      KDDM_LOG_UNPACK4_2(log),
			      KDDM_LOG_UNPACK4_3(log));
		break;

	case KDDM_LOG_EXIT_GET:
		n += sprintf (&buffer[n], "GET: done - State %s - frozen "
			      "count %ld - sleeper count %ld\n",
			      STATE_NAME(KDDM_LOG_UNPACK4_1(log)),
			      KDDM_LOG_UNPACK4_2(log),
			      KDDM_LOG_UNPACK4_3(log));
		break;

	case KDDM_LOG_EXIT_GRAB:
		n += sprintf (&buffer[n], "GRAB: done - State %s - frozen "
			      "count %ld - sleeper count %ld\n",
			      STATE_NAME(KDDM_LOG_UNPACK4_1(log)),
			      KDDM_LOG_UNPACK4_2(log),
			      KDDM_LOG_UNPACK4_3(log));
		break;

	case KDDM_LOG_EXIT_PUT:
		n += sprintf (&buffer[n], "PUT: done - State %s - frozen "
			      "count %ld - sleeper count %ld\n",
			      STATE_NAME(KDDM_LOG_UNPACK4_1(log)),
			      KDDM_LOG_UNPACK4_2(log),
			      KDDM_LOG_UNPACK4_3(log));
		break;

	case KDDM_LOG_ENTER_SETS:
		n += sprintf (&buffer[n], "Copyset (");
		goto print_sets;

	case KDDM_LOG_EXIT_SETS:
		n += sprintf (&buffer[n], "Done - Copyset (");
print_sets:
		CLEAR_SET(&set);
		set.bits[0] = log->data;
		n += print_set(&set, &buffer[n]);
		n += sprintf (&buffer[n], ") - Rmset (");
		set.bits[0] = log->req_id;
		n += print_set(&set, &buffer[n]);
		n += sprintf (&buffer[n], ")\n");
		break;

	case KDDM_LOG_STATE:
		n += sprintf (&buffer[n], "Object state %s\n",
			      STATE_NAME(log->data));
		break;

	case KDDM_LOG_STATE_CHANGED:
		n += sprintf (&buffer[n], "Object state changed to %s\n",
			      STATE_NAME(log->data));
		break;

	case KDDM_LOG_SEND_RD_REQ:
		n += sprintf (&buffer[n], "Send read copy request [%d;%ld] to "
			      "node %d\n", REQ_NODE(log), REQ_ID(log),
			      (int)log->data);
		break;

	case KDDM_LOG_SEND_WR_REQ:
		n += sprintf (&buffer[n], "Send write copy request [%d;%ld] to "
			      "node %d\n", REQ_NODE(log), REQ_ID(log),
			      (int)log->data);
		break;

	case KDDM_LOG_RM_REQ_MGR:
		n += sprintf (&buffer[n], "Send remove request [%d;%ld] to "
			      "manager on node %d\n", REQ_NODE(log),
			      REQ_ID(log), (int)log->data);
		break;

	case KDDM_LOG_INV_REQ:
		n += sprintf (&buffer[n], "Send invalidation requests [%d;%ld] "
			      "to nodes ", REQ_NODE(log), REQ_ID(log));
print_set:
		CLEAR_SET(&set);
		set.bits[0] = log->data;
		n += print_set(&set, &buffer[n]);
		n += sprintf (&buffer[n], "\n");
		break;

	case KDDM_LOG_RM_REQ:
		n += sprintf (&buffer[n], "Send remove requests [%d;%ld] to "
			      "nodes ",	REQ_NODE(log), REQ_ID(log));
		goto print_set;

	case KDDM_LOG_INSERT:
		n += sprintf (&buffer[n], "Insert object in master copy - "
			      "Remove set: ");
		goto print_set;

	case KDDM_LOG_SLEEP:
		n += sprintf (&buffer[n], "Sleep on wait object - State %s - "
			      "frozen count %ld - sleeper count %ld\n",
			      STATE_NAME(KDDM_LOG_UNPACK4_1(log)),
			      KDDM_LOG_UNPACK4_2(log),
			      KDDM_LOG_UNPACK4_3(log));
		break;

	case KDDM_LOG_TRY_AGAIN:
		n += sprintf (&buffer[n], "Object has state %s: try again\n",
			      STATE_NAME(log->data));
		break;

	case KDDM_LOG_SEND_OBJ:
		n += sprintf (&buffer[n], "Send object to node %d\n",
			      (int)log->data);
		break;

	case KDDM_LOG_DELAY:
		n += sprintf (&buffer[n], "Object frozen, delay request\n");
		break;

	case KDDM_LOG_LAST_COPY:
		n += sprintf (&buffer[n], "Local copy is the last one\n");
		break;

	case KDDM_LOG_CO_ACK_RECV:
		n += sprintf (&buffer[n], "Change ownership ack received\n");
		break;

	case KDDM_LOG_WAIT_SET_MD:
		n += sprintf (&buffer[n], "Wait for kddm set meta data\n");
		break;

	case KDDM_LOG_RECV_SET_MD:
		n += sprintf (&buffer[n], "Set meta-data received\n");
		break;

	case KDDM_LOG_FIND_SET:
		n += sprintf (&buffer[n], "Try to find kddm_set on node %d\n",
			(int)log->data);
		break;

	case KDDM_LOG_FOUND_SET:
		n += sprintf (&buffer[n], "Found kddm set at %p\n",
			      (void *)log->data);
		break;

	case KDDM_LOG_LOOK_UP:
		n += sprintf (&buffer[n], "Lookup for kddm set on node %d "
			      "request\n", (int)log->data);
		break;

	case KDDM_LOG_OBJ_CREATED:
		n += sprintf (&buffer[n], "Object created - prob_owner = %ld "
			      "- State : %s\n", KDDM_LOG_UNPACK2_1(log),
			      STATE_NAME(KDDM_LOG_UNPACK2_2(log)));
		break;

	case KDDM_LOG_WOKEN_UP:
		n += sprintf (&buffer[n], "Woken up with state %s - frozen "
			      "count %ld - sleeper count %ld\n",
			      STATE_NAME(KDDM_LOG_UNPACK4_1(log)),
			      KDDM_LOG_UNPACK4_2(log),
			      KDDM_LOG_UNPACK4_3(log));
		break;

	case KDDM_LOG_FORWARD:
		n += sprintf (&buffer[n], "Forward request [%d;%ld] to node "
			      "%d\n", REQ_NODE(log), REQ_ID(log),
			      (int)log->data);
		break;

	case KDDM_LOG_HANDLE_FROM:
		n += sprintf (&buffer[n], "Handle request [%d;%ld] received "
			      "from node %d\n", REQ_NODE(log), REQ_ID(log),
			      (int)log->data);
		break;

	case KDDM_LOG_COPY_REQ:
		if (KDDM_LOG_UNPACK4_3(log) == KDDM_OBJ_COPY_ON_WRITE)
			n += sprintf (&buffer[n], "Handle WRITE copy request "
				      "[%d;%ld] from node %ld\n",
				      REQ_NODE(log), REQ_ID(log),
				      KDDM_LOG_UNPACK4_1(log));
		else
			n += sprintf (&buffer[n], "Handle READ copy request "
				      "from node %ld on node %ld request\n",
				      KDDM_LOG_UNPACK4_1(log),
				      KDDM_LOG_UNPACK4_2(log));
		break;

	case KDDM_LOG_QUEUE_REQ:
		n += sprintf (&buffer[n], "Object has state %s: queue "
			      "request\n", STATE_NAME(log->data));
		break;

	case KDDM_LOG_TRANS_WA:
		n += sprintf (&buffer[n], "Transfert write access to node "
			      "%d\n", (int)log->data);
		break;

	case KDDM_LOG_ACK_SEND:
		n += sprintf (&buffer[n], "Send ");
		switch (KDDM_LOG_UNPACK2_1(log)) {
		case INVALIDATION_ACK:
			n += sprintf (&buffer[n], "INVALIDATION_ACK");
			break;
		case REMOVE_ACK:
			n += sprintf (&buffer[n], "REMOVE_ACK:");
			break;
		case REMOVE_ACK2:
			n += sprintf (&buffer[n], "REMOVE_ACK2");
			break;
		default:
			BUG();
		}
		n += sprintf (&buffer[n], " to node %ld\n",
			      KDDM_LOG_UNPACK2_2(log));
		break;

	case KDDM_LOG_SEND_BACK:
		n += sprintf (&buffer[n], "Send back first touch to node %d\n",
			      (int)log->data);
		break;

	default:
		BUG();
	}
}



noinline void kddm_save_log(unsigned long eip,
			    const char* mask,
			    int level,
 			    long req_id,
			    int ns_id,
			    kddm_set_id_t set_id,
			    objid_t obj_id,
			    int dbg_id,
			    unsigned long data)
{
	struct kddm_log *log;
	struct kddm_obj *obj_entry = (struct kddm_obj *) data;
	krgnodemask_t *copyset;
	char buffer[256];

	if (!kddm_filter_debug(set_id) || (!match_debug("kddm", mask, level)))
		return ;

	log = kmalloc (sizeof(*log), GFP_ATOMIC);
	if (log == NULL) {
		printk ("Out of memory...\n");
		BUG();
	}

	log->next = NULL;
	log->pid = current->pid;
	log->req_id = req_id;
	log->set_id = set_id;
	log->obj_id = obj_id;
	log->fct_addr = eip;
	log->dbg_id = dbg_id;

	switch (log->dbg_id) {
	  case KDDM_LOG_ENTER_COUNT:
	  case KDDM_LOG_ENTER_GET:
	  case KDDM_LOG_ENTER_GRAB:
	  case KDDM_LOG_ENTER_PUT:
	  case KDDM_LOG_EXIT_COUNT:
	  case KDDM_LOG_EXIT_GET:
	  case KDDM_LOG_EXIT_GRAB:
	  case KDDM_LOG_EXIT_PUT:
	  case KDDM_LOG_WOKEN_UP:
	  case KDDM_LOG_SLEEP:
		  log->data = KDDM_LOG_PACK4(OBJ_STATE(obj_entry),
				     atomic_read(&obj_entry->frozen_count),
				     atomic_read(&obj_entry->sleeper_count),
				     0);
		  break;

	  case KDDM_LOG_STATE:
	  case KDDM_LOG_TRY_AGAIN:
	  case KDDM_LOG_QUEUE_REQ:
		  log->data = OBJ_STATE(obj_entry);
		  break;

	  case KDDM_LOG_OBJ_CREATED:
		  log->data = KDDM_LOG_PACK2(get_prob_owner(obj_entry),
					     OBJ_STATE(obj_entry));
		  break;

	  case KDDM_LOG_INV_REQ:
	  case KDDM_LOG_RM_REQ:
	  case KDDM_LOG_INSERT:
		  copyset = (krgnodemask_t *) data;
		  log->data = copyset->bits[0];
		  break;

	  case KDDM_LOG_ENTER_SETS:
	  case KDDM_LOG_EXIT_SETS:
		  copyset = COPYSET(obj_entry);
		  log->data = copyset->bits[0];
		  copyset = RMSET(obj_entry);
		  log->req_id = copyset->bits[0];
		  break;

	  default:
		  log->data = data;
		  break;
	}

	if (log_kddm_debugs) {
		spin_lock(&kddm_log_lock);
		if (unlikely(kddm_log_head == NULL))
			kddm_log_head = log;
		else
			kddm_log_tail->next = log;
		kddm_log_tail = log;
		if (atomic_read(&nr_kddm_logs) < MAX_LOG_MSG) {
			atomic_inc (&nr_kddm_logs);
			log = NULL;
		}
		else {
			log = kddm_log_head;
			kddm_log_head = kddm_log_head->next;
		}
		spin_unlock(&kddm_log_lock);
		kfree (log);
	}
	else {
		kddm_print_log(log, buffer);
		printk ("%s", buffer);
	}
}



static inline int write_set_id(char *buf,
			       int max,
			       kddm_set_id_t set_id)
{
	switch (set_id) {
	  case 0:
		  return snprintf(buf, max, "-\n");

	  case MIN_KDDM_ID:
		  return snprintf(buf, max, "U\n");
	}

	return snprintf(buf, max, "%ld\n", set_id);
}



static ssize_t read_file_filter(struct file *file,
				char __user *user_buf,
				size_t count,
				loff_t *ppos)
{
	char buffer[1024];
	int i, len = 0;

	len += snprintf(buffer + len, 1024, "log: ");
	len += snprintf(buffer + len, 1024 - len, "%d\n", log_kddm_debugs);

	for (i = 0; i < MAX_SET_FILTER; i++) {
		len += snprintf(buffer + len, 1024 - len, "%d: ", i);
		len += write_set_id(buffer + len, 1024 - len, set_filter[i]);
	}

	len += snprintf(buffer + len, 1024, "min: ");
	len += write_set_id(buffer + len, 1024 - len, set_filter_min);

	len += snprintf(buffer + len, 1024, "max: ");
	len += write_set_id(buffer + len, 1024 - len, set_filter_max);

	return simple_read_from_buffer(user_buf, count, ppos, buffer, len);
}



static inline kddm_set_id_t read_set_id(char *buf)
{
	char **endp = NULL;

	switch (buf[0]) {
	  case '-':
		  return 0;

	  case 'u':
	  case 'U':
		  return MIN_KDDM_ID;
	}
	return simple_strtoul(buf, endp, 0);
}



static ssize_t write_file_filter(struct file *file,
				 const char __user *user_buf,
				 size_t count,
				 loff_t *ppos)
{
	char buf[32];
	int buf_size, index;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (buf[1] == ':') {
		index = buf[0] - '0';

		if ((index < 0) || (index >= MAX_SET_FILTER))
			return -EINVAL;

		set_filter[index] = read_set_id (&buf[2]);
		goto done;
	}

	if (buf[3] != ':')
		return -EINVAL;

	if ((strncmp(buf, "MIN", 3) == 0) ||
	    (strncmp(buf, "min", 3) == 0)) {
		set_filter_min = read_set_id (&buf[4]);
		goto done;
	}
	if ((strncmp(buf, "MAX", 3) == 0) ||
	    (strncmp(buf, "max", 3) == 0)) {
		set_filter_max = read_set_id (&buf[4]);
		goto done;
	}
	if ((strncmp(buf, "LOG", 3) == 0) ||
	    (strncmp(buf, "log", 3) == 0))
		log_kddm_debugs = (read_set_id (&buf[4]) != 0);

done:
	return count;
}



static int default_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}



static const struct file_operations fops_filter = {
	.read =         read_file_filter,
	.write =        write_file_filter,
	.open =         default_open
};



/** Read function for /proc/<pid>/kddm entry.
 *  @author Renaud Lottiaux
 *
 *  @return  Number of bytes written.
 */
void print_proc_kddm_info (struct task_struct *task)
{
	struct kddm_info_struct *kddm_info = task->kddm_info;
	struct kddm_obj *obj_entry;

	if (! kddm_info)
		return;

	printk ("Get Object:          %ld\n",
		kddm_info->get_object_counter);

	printk ("Grab Object:         %ld\n",
		kddm_info->grab_object_counter);

	printk ("Remove Object:       %ld\n",
		kddm_info->remove_object_counter);

	printk ("Flush Object:        %ld\n",
		kddm_info->flush_object_counter);

	obj_entry = kddm_info->wait_obj;

	if (!obj_entry)
		return;

	printk ("Process wait on object "
		"(%d;%lu;%lu) %p with state %s\n",
		kddm_info->ns_id, kddm_info->set_id,
		kddm_info->obj_id, obj_entry,
		STATE_NAME (OBJ_STATE(obj_entry)));

	printk ("  * Probe owner:   %d\n", get_prob_owner(obj_entry));
	printk ("  * Frozen count:  %d\n",
		atomic_read(&obj_entry->frozen_count));
	printk ("  * Sleeper count: %d\n",
		atomic_read(&obj_entry->sleeper_count));
	printk ("  * Object:        %p\n",
		obj_entry->object);
	printk ("  * Copy set: ");
	print_set(&obj_entry->master_obj.copyset, NULL);
	printk ("\n  * Remove set: ");
	print_set(&obj_entry->master_obj.rmset, NULL);
	printk ("\n  * Waiting processes: ");
	print_wq (&obj_entry->waiting_tsk);
	printk ("\n");
}

/** Display information about on objentry
 *  @author Renaud Lottiaux, Matthieu Fertré
 */
static inline void __display_kddm_obj (struct kddm_obj *obj_entry)
{
	if (!obj_entry)
		return;

	printk("object %p with state %s\n",
		    obj_entry, STATE_NAME (OBJ_STATE(obj_entry)));

	printk("  * Probe owner:   %d\n", get_prob_owner(obj_entry));
	printk("  * Frozen count:  %d\n",
		    atomic_read(&obj_entry->frozen_count));
	printk("  * Sleeper count: %d\n",
		    atomic_read(&obj_entry->sleeper_count));
	printk("  * Object:        %p\n",
		    obj_entry->object);
	printk("  * Copy set: ");
	print_set(&obj_entry->master_obj.copyset, NULL);
	printk("\n  * Remove set: ");
	print_set(&obj_entry->master_obj.rmset, NULL);
	printk("\n  * Waiting processes: ");
	print_wq (&obj_entry->waiting_tsk);
	printk("\n");
}

int kcb_kddm_display_obj(int ns_id,
                         long set_id,
                         long objid)
{
       struct kddm_set *set = NULL;
       struct kddm_obj *obj = NULL;

       set = local_get_kddm_set(ns_id, (kddm_set_id_t)set_id);
       if (!set)
               return -21; /* KDB_BADADDR; */

       obj = __get_kddm_obj_entry(set, (objid_t)objid);
       if (!obj)
               return -21; /* KDB_BADADR; */

       __display_kddm_obj(obj);

       kddm_obj_unlock(set, (objid_t)objid);
       put_kddm_set(set);

       return 0;
}

void init_kddm_debug(void)
{
	kddm_debug_dentry = debug_define("kddm", 0);
	DEBUG_MASK("kddm", "creation");
	DEBUG_MASK("kddm", "find_object");
	DEBUG_MASK("kddm", "getgrab");
	DEBUG_MASK("kddm", "flush");
	DEBUG_MASK("kddm", "remove");
	DEBUG_MASK("kddm", "sync");
	DEBUG_MASK("kddm", "linker");
	DEBUG_MASK("kddm", "kddm_lookup");
	DEBUG_MASK("kddm", "usage");
	DEBUG_MASK("kddm", "io_linker");
	DEBUG_MASK("kddm", "object");
	DEBUG_MASK("kddm", "invalidation");

	kddm_filter_dentry = debugfs_create_file("filter", S_IRWXU,
						 kddm_debug_dentry,
						 0, &fops_filter);

	init_and_set_unique_id_root(&kddm_req_id_root, 1);

	kh_kddm_print_log = kddm_print_log;
	kh_print_proc_kddm_info = print_proc_kddm_info;
	kh_kddm_display_obj = kcb_kddm_display_obj;
}
