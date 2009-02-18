#ifndef __DEBUG_KDDM_H__

#define __DEBUG_KDDM_H__

#ifdef DEBUG
#undef DEBUG
#endif

#ifdef CONFIG_KRG_DEBUG

#include <tools/debug_tools2.h>
#include <kerrighed/ctnr_headers.h>
#include <kerrighed/unique_id.h>

enum {
	KDDM_LOG_ENTER,
	KDDM_LOG_ENTER_COUNT,
	KDDM_LOG_ENTER_GET,
	KDDM_LOG_ENTER_GRAB,
	KDDM_LOG_ENTER_PUT,
	KDDM_LOG_ENTER_SETS,
	KDDM_LOG_EXIT,
	KDDM_LOG_EXIT_COUNT,
	KDDM_LOG_EXIT_GET,
	KDDM_LOG_EXIT_GRAB,
	KDDM_LOG_EXIT_PUT,
	KDDM_LOG_EXIT_SETS,
	KDDM_LOG_STATE,
	KDDM_LOG_STATE_CHANGED,
	KDDM_LOG_INSERT,
	KDDM_LOG_SEND_RD_REQ,
	KDDM_LOG_SEND_WR_REQ,
	KDDM_LOG_INV_REQ,
	KDDM_LOG_RM_REQ,
	KDDM_LOG_RM_REQ_MGR,
	KDDM_LOG_SLEEP,
	KDDM_LOG_TRY_AGAIN,
	KDDM_LOG_SEND_OBJ,
	KDDM_LOG_DELAY,         // "Object frozen, delay request\n"
	KDDM_LOG_LAST_COPY,     // "Local copy is the last one\n"
	KDDM_LOG_CO_ACK_RECV,   // "Change ownership ack received\n"
	KDDM_LOG_WAIT_SET_MD,   // "Wait for kddm set meta data\n"
	KDDM_LOG_RECV_SET_MD,   // "Received for kddm %ld\n"
	KDDM_LOG_FIND_SET,      // "Try to find kddm_set on node %d\n"
	KDDM_LOG_FOUND_SET,     // "Found kddm set at %p\n"
	KDDM_LOG_LOOK_UP,       // "Lookup for kddm set on node %d request\n"
	KDDM_LOG_OBJ_CREATED,   // "Object created - kddm_set link : %d - prob_owner = %d - State : %s\n"
	KDDM_LOG_WOKEN_UP,      // "Woken up with count %d - frozen %d\n"
	KDDM_LOG_FORWARD,       // "I'm not owner of object: forward to %d\n"
	KDDM_LOG_HANDLE_FROM,
	KDDM_LOG_COPY_REQ,
	KDDM_LOG_QUEUE_REQ,
	KDDM_LOG_TRANS_WA,      // "Transfert write access to node %d\n"
	KDDM_LOG_ACK_SEND,
	KDDM_LOG_SEND_BACK,
};

#define KDDM_LOG_PACK4(a,b,c,d) ((((a)&0xFF)<<24) | (((b)&0xFF)<<16) | (((c)&0xFF)<<8) | ((d)&0xFF))
#define KDDM_LOG_PACK2(a,b) ((((a)&0xFFFF)<<16) | ((b)&0xFFFF))

#define KDDM_LOG_UNPACK4_1(log) (((log->data)>>24) & 0xFF)
#define KDDM_LOG_UNPACK4_2(log) (((log->data)>>16) & 0xFF)
#define KDDM_LOG_UNPACK4_3(log) (((log->data)>>8) & 0xFF)
#define KDDM_LOG_UNPACK4_4(log) ((log->data) & 0xFF)

#define KDDM_LOG_UNPACK2_1(log) (((log->data)>>16) & 0xFFFF)
#define KDDM_LOG_UNPACK2_2(log) ((log->data) & 0xFFFF)

int kddm_filter_debug(kddm_set_id_t set_id);

void kddm_save_log(unsigned long eip, const char* mask, int level, long req_id,
		   int ns_id, kddm_set_id_t set_id, objid_t obj_id, int dbg_id,
		   unsigned long data);

void init_kddm_debug(void);

extern unique_id_root_t kddm_req_id_root;

static inline long get_kddm_req_id(void) {
	return get_unique_id (&kddm_req_id_root);
}

extern pid_t debug_kddm_last_pid;

#define DEBUG(mask, level, ns_id, set_id, obj_id, dbg_id, data) \
        kddm_save_log(_THIS_IP_, mask, level, 0, ns_id, set_id, obj_id, dbg_id, (unsigned long)(data))
#define DEBUG2(mask, level, req_id, ns_id, set_id, obj_id, dbg_id, data) \
        kddm_save_log(_THIS_IP_, mask, level, req_id, ns_id, set_id, obj_id, dbg_id, (unsigned long)(data))


#else // CONFIG_KRG_DEBUG

#define get_kddm_req_id() 0

#define DEBUG(mask, level, ns_id, set_id, obj_id, dbg_id, data) do {} while(0)
#define DEBUG2(mask, level, req_id, ns_id, set_id, obj_id, dbg_id, data) do {} while(0)

#endif // CONFIG_KRG_DEBUG

#define fill_debug_info(msg, _req_id) \
	if (_req_id == 0) \
		msg.req_id = get_kddm_req_id(); \
	else \
		msg.req_id = _req_id; \
	msg.ttl = 2 * kerrighed_nb_nodes

#endif // __DEBUG_KDDM_H__
