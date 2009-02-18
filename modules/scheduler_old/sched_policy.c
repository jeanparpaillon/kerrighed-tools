/*
 *  Kerrighed/modules/scheduler_old/sched_policy.c
 *
 *  Copyright (C) 2006-2007 Louis Rilling - Kerlabs
 */

/** Generic definitions for global scheduling policies.
 *  @file sched_policy.c
 *
 *  @author Louis Rilling
 */

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/rwsem.h>
#include <linux/types.h>
#include <linux/module.h>

#include "scheduler.h"
#include "sched_policy.h"


#define to_policy(obj) container_of(obj, struct sched_policy, kobj)
#define to_policy_attr(_attr) container_of(_attr,	\
					   struct sched_policy_attribute, attr)


static ssize_t policy_attr_show(struct kobject *kobj,
				struct attribute *attr,
				char *buf)
{
	struct sched_policy_attribute *policy_attr = to_policy_attr(attr);
	struct sched_policy *policy = to_policy(kobj);
	ssize_t ret = 0;

	if (policy_attr->show)
		ret = policy_attr->show(policy, buf);

	return ret;
}

static ssize_t policy_attr_store(struct kobject *kobj,
				 struct attribute *attr,
				 const char *buf, size_t count)
{
	struct sched_policy_attribute *policy_attr = to_policy_attr(attr);
	struct sched_policy *policy = to_policy(kobj);
	ssize_t ret = 0;

	if (policy_attr->store)
		ret = policy_attr->store(policy, buf, count);

	return ret;
}


static struct sysfs_ops sched_policy_sysfs_ops = {
	.show  = policy_attr_show,
	.store = policy_attr_store,
};


static void policy_release(struct kobject *kobj)
{
	struct sched_policy *policy = to_policy(kobj);

	if (policy->release)
		policy->release(policy);
}


struct attribute *sched_policy_default_attrs[];

static struct kobj_type ktype_sched_policy = {
	.release       = policy_release,
	.sysfs_ops     = &sched_policy_sysfs_ops,
	.default_attrs = sched_policy_default_attrs,
};

decl_subsys(sched_policies, &ktype_sched_policy, NULL);


static ssize_t active_show(struct sched_policy *policy, char * buf)
{
	return sprintf(buf, "%u\n", policy->active);
}


static ssize_t active_store(struct sched_policy *policy,
			    const char * buf, size_t n)
{
	unsigned long state;
	int error = 0;

	state = simple_strtoul(buf, NULL, 10);
	if (state > 1)
		return -EINVAL;

	down_write(&sched_policies_subsys.rwsem);
	if (state && policy->activate)
		error = policy->activate(policy);
	else if (!state && policy->deactivate)
		error = policy->deactivate(policy);
	if (!error)
		policy->active = (int) state;
	up_write(&sched_policies_subsys.rwsem);

	return error ? error : n;
}


static SCHED_POLICY_ATTR(active_state, 0644, active_show, active_store);

struct attribute *sched_policy_default_attrs[] = {
	&sched_policy_attr_active_state.attr,
	NULL,
};


int sched_policy_create_file(struct sched_policy *policy,
			     struct sched_policy_attribute *attr)
{
	int error = 0;

	if (get_sched_policy(policy)) {
		error = sysfs_create_file(&policy->kobj, &attr->attr);
		put_sched_policy(policy);
	}

	return error;
}


void sched_policy_remove_file(struct sched_policy *policy,
			      struct sched_policy_attribute *attr)
{
	if (get_sched_policy(policy)) {
		sysfs_remove_file(&policy->kobj, &attr->attr);
		put_sched_policy(policy);
	}
}


void sched_policy_initialize(struct sched_policy *policy)
{
	kobj_set_kset_s(policy, sched_policies_subsys);
	kobject_init(&policy->kobj);
	policy->active = 1;
}


int sched_policy_add(struct sched_policy *policy)
{
	int error = -EINVAL;

	policy = get_sched_policy(policy);
	if (!policy)
		goto Error;

	kobject_set_name(&policy->kobj, "%s", policy->name);
	if ((error = kobject_add(&policy->kobj)))
		goto Error;

 Done:
	put_sched_policy(policy);
	return error;

 Error:
	goto Done;
}


int sched_policy_register(struct sched_policy *policy)
{
	sched_policy_initialize(policy);
	return sched_policy_add(policy);
}


struct sched_policy *get_sched_policy(struct sched_policy *policy)
{
	return policy ? to_policy(kobject_get(&policy->kobj)) : NULL;
}


void put_sched_policy(struct sched_policy *policy)
{
	if (policy)
		kobject_put(&policy->kobj);
}


void sched_policy_del(struct sched_policy *policy)
{
	kobject_del(&policy->kobj);
}


void sched_policy_unregister(struct sched_policy *policy)
{
	sched_policy_del(policy);
	put_sched_policy(policy);
}


int init_sched_policies(void)
{
	kset_set_kset_s(&sched_policies_subsys, scheduler_subsys);
	return subsystem_register(&sched_policies_subsys);
}


void cleanup_sched_policies(void)
{
	subsystem_unregister(&sched_policies_subsys);
}
