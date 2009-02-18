/** Generic definitions for global scheduling policies.
 *  @file sched_policy.h
 *
 *  @author Louis Rilling
 */

#ifdef CONFIG_KRG_SCHED_LEGACY

#ifndef __SCHED_POLICY_H__
#define __SCHED_POLICY_H__

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/types.h>


struct sched_policy {
	struct kobject kobj;
	void (*release)(struct sched_policy *policy);
	int (*activate)(struct sched_policy *policy);
	int (*deactivate)(struct sched_policy *policy);
	char *name;
	int active;
};


extern int sched_policy_register(struct sched_policy *policy);
extern void sched_policy_unregister(struct sched_policy *policy);

extern int sched_policy_register(struct sched_policy *policy);
extern void sched_policy_unregister(struct sched_policy *policy);
extern void sched_policy_initialize(struct sched_policy *policy);
extern int sched_policy_add(struct sched_policy *policy);
extern void sched_policy_del(struct sched_policy *policy);


struct sched_policy_attribute {
	struct attribute attr;
	ssize_t (*show)(struct sched_policy *policy, char *buf);
	ssize_t (*store)(struct sched_policy *policy,
			 const char *buf, size_t count);
};

#define SCHED_POLICY_ATTR(_name,_mode,_show,_store)		\
struct sched_policy_attribute sched_policy_attr_##_name =	\
	__ATTR(_name,_mode,_show,_store)


extern int sched_policy_create_file(struct sched_policy *policy,
				    struct sched_policy_attribute *attr);
extern void sched_policy_remove_file(struct sched_policy *policy,
				     struct sched_policy_attribute *attr);

extern struct sched_policy *get_sched_policy(struct sched_policy *policy);
extern void put_sched_policy(struct sched_policy *policy);

#endif /* __SCHED_POLICY_H__ */

#endif /* CONFIG_KRG_SCHED_LEGACY */
