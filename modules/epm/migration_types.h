#ifndef __MIGRATION_TYPES_H__
#define __MIGRATION_TYPES_H__

#include <linux/types.h>
#include <kerrighed/sys/types.h>

struct migration_infos_struct {
	kerrighed_node_t destination_node_id;
	union {
		pid_t process_to_migrate;
		pid_t thread_to_migrate;
	};
};

typedef struct migration_infos_struct migration_infos_t;

#endif /* __MIGRATION_TYPES_H__ */
