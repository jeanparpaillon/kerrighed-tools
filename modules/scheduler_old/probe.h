/** Generic definitions for scheduling probes.
 *  @file probe.h
 *
 *  @author Louis Rilling
 */

#ifdef CONFIG_KRG_SCHED_LEGACY

#ifndef __PROBE_H__
#define __PROBE_H__

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/types.h>


struct probe {
	struct kobject kobj;
	void (*release)(struct probe *probe);
	int (*activate)(struct probe *probe);
	int (*deactivate)(struct probe *probe);
	char *name;
	int active;
};


extern int probe_register(struct probe *probe);
extern void probe_unregister(struct probe *probe);

extern int probe_register(struct probe *probe);
extern void probe_unregister(struct probe *probe);
extern void probe_initialize(struct probe *probe);
extern int probe_add(struct probe *probe);
extern void probe_del(struct probe *probe);


struct probe_attribute {
	struct attribute attr;
	ssize_t (*show)(struct probe *probe, char *buf);
	ssize_t (*store)(struct probe *probe, const char *buf, size_t count);
};

#define PROBE_ATTR(_name,_mode,_show,_store)		\
	struct probe_attribute probe_attr_##_name =	\
		__ATTR(_name,_mode,_show,_store)


extern int probe_create_file(struct probe *probe, struct probe_attribute *attr);
extern void probe_remove_file(struct probe *probe,
			      struct probe_attribute *attr);

extern struct probe *get_probe(struct probe *probe);
extern void put_probe(struct probe *probe);

#endif /* __PROBE_H__ */

#endif /* CONFIG_KRG_SCHED_LEGACY */
