/** Root Kerrighed sysfs entry.
 *  @file krg_sysfs.c
 *
 *  Copyright (C) 2006-2007, Louis Rilling, Kerlabs.
 *  Copyright (C) 2009 Cyril Brulebois <cyril.brulebois@kerlabs.com>
 */

#define MODULE_NAME "Krg Sys FS "

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <kerrighed/krginit.h>


struct kerrighed_sysfs_attribute {
        struct attribute        attr;
        ssize_t (*show)(struct kerrighed_sysfs_attribute *, char *);
        ssize_t (*store)(struct kerrighed_sysfs_attribute *, const char *, size_t);
};

static ssize_t node_id_show(struct kerrighed_sysfs_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%u\n", kerrighed_node_id);
}

static ssize_t session_id_show(struct kerrighed_sysfs_attribute *attr,
			       char *buf)
{
	return sprintf(buf, "%u\n", kerrighed_session_id);
}


static struct kerrighed_sysfs_attribute node_id_attr =
  __ATTR_RO(node_id);

static struct kerrighed_sysfs_attribute session_id_attr =
  __ATTR_RO(session_id);



decl_subsys(kerrighed, NULL, NULL);


int init_sysfs(void)
{
	int err = 0;

	err = subsystem_register(&kerrighed_subsys);
	if (err)
		panic("Couldn't register kerrrighed sysfs subsystem\n");

	err = sysfs_create_file(&kerrighed_subsys.kset.kobj,
				&node_id_attr.attr);
	if (err)
		panic("Couldn't create /sys/kerrighed/node_id\n");

	err = sysfs_create_file(&kerrighed_subsys.kset.kobj,
				&session_id_attr.attr);
	if (err)
		panic("Couldn't create /sys/kerrighed/session_id\n");

	return 0;
}


void cleanup_sysfs(void)
{
	subsystem_unregister(&kerrighed_subsys);
}
