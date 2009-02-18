/*
 *  Kerrighed/modules/scheduler_old/probe.c
 *
 *  Copyright (C) 2006-2007 Louis Rilling - Kerlabs
 */

/** Generic definitions for scheduling probes.
 *  @file probe.c
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
#include "probe.h"


#define to_probe(obj) container_of(obj, struct probe, kobj)
#define to_probe_attr(_attr) container_of(_attr,	\
					  struct probe_attribute, attr)


static ssize_t
probe_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct probe_attribute *probe_attr = to_probe_attr(attr);
	struct probe *probe = to_probe(kobj);
	ssize_t ret = 0;

	if (probe_attr->show)
		ret = probe_attr->show(probe, buf);

	return ret;
}


static ssize_t
probe_attr_store(struct kobject *kobj, struct attribute *attr,
		  const char *buf, size_t count)
{
	struct probe_attribute *probe_attr = to_probe_attr(attr);
	struct probe *probe = to_probe(kobj);
	ssize_t ret = 0;

	if (probe_attr->store)
		ret = probe_attr->store(probe, buf, count);

	return ret;
}


static struct sysfs_ops probe_sysfs_ops = {
	.show  = probe_attr_show,
	.store = probe_attr_store,
};


static void probe_release(struct kobject *kobj)
{
	struct probe *probe = to_probe(kobj);

	if (probe->release)
		probe->release(probe);
}


struct attribute *probe_default_attrs[];

static struct kobj_type ktype_probe = {
	.release       = probe_release,
	.sysfs_ops     = &probe_sysfs_ops,
	.default_attrs = probe_default_attrs,
};

decl_subsys(probes, &ktype_probe, NULL);


static ssize_t active_show(struct probe *probe, char * buf)
{
	return sprintf(buf, "%u\n", probe->active);
}


static ssize_t active_store(struct probe *probe, const char * buf, size_t n)
{
	unsigned long state;
	int error = 0;

	state = simple_strtoul(buf, NULL, 10);
	if (state > 1)
		return -EINVAL;

	down_write(&probes_subsys.rwsem);
	if (state && probe->activate)
		error = probe->activate(probe);
	else if (!state && probe->deactivate)
		error = probe->deactivate(probe);
	if (!error)
		probe->active = (int) state;
	up_write(&probes_subsys.rwsem);

	return error ? error : n;
}


static PROBE_ATTR(active_state, 0644, active_show, active_store);

struct attribute *probe_default_attrs[] = {
	&probe_attr_active_state.attr,
	NULL,
};


int probe_create_file(struct probe *probe, struct probe_attribute *attr)
{
	int error = 0;

	if (get_probe(probe)) {
		error = sysfs_create_file(&probe->kobj, &attr->attr);
		put_probe(probe);
	}

	return error;
}


void probe_remove_file(struct probe *probe, struct probe_attribute *attr)
{
	if (get_probe(probe)) {
		sysfs_remove_file(&probe->kobj, &attr->attr);
		put_probe(probe);
	}
}


void probe_initialize(struct probe *probe)
{
	kobj_set_kset_s(probe, probes_subsys);
	kobject_init(&probe->kobj);
	probe->active = 0;
}


int probe_add(struct probe *probe)
{
	int error = -EINVAL;

	probe = get_probe(probe);
	if (!probe)
		goto Error;

	kobject_set_name(&probe->kobj, "%s", probe->name);
	if ((error = kobject_add(&probe->kobj)))
		goto Error;

 Done:
	put_probe(probe);
	return error;

 Error:
	goto Done;
}


int probe_register(struct probe *probe)
{
	probe_initialize(probe);
	return probe_add(probe);
}


struct probe *get_probe(struct probe *probe)
{
	return probe ? to_probe(kobject_get(&probe->kobj)) : NULL;
}


void put_probe(struct probe *probe)
{
	if (probe)
		kobject_put(&probe->kobj);
}


void probe_del(struct probe *probe)
{
	kobject_del(&probe->kobj);
}


void probe_unregister(struct probe *probe)
{
	probe_del(probe);
	put_probe(probe);
}


int init_probes(void)
{
	kset_set_kset_s(&probes_subsys, scheduler_subsys);
	return subsystem_register(&probes_subsys);
}


void cleanup_probes(void)
{
	subsystem_unregister(&probes_subsys);
}
