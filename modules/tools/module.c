/** Main file for the tools module.
 *  @file module.c
 *
 *  Copyright (C) 2006-2007, Pascal Gallard, Kerlabs.
 */

#define MODULE_NAME "Iluv. mod "

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/workqueue.h>

#include <kerrighed/krginit.h>
#include <kerrighed/krgflags.h>

#include <rpc/rpc.h>
#include <rpc/rpcid.h>
#include "debug_tools2.h"

MODULE_AUTHOR("Kerrighed Research team");
MODULE_DESCRIPTION("Kerrighed core system");
MODULE_LICENSE("GPL");

int __krg_panic__ = 0;

struct workqueue_struct *krg_wq;
struct workqueue_struct *krg_nb_wq;

#define deffct(p) extern int init_##p(void); extern void cleanup_##p(void)

deffct(tools);
#ifdef CONFIG_KRG_HOTPLUG
deffct(hotplug);
#endif
#ifdef CONFIG_KRG_COMMUNICATION_FRAMEWORK
deffct(rpc);
#endif
deffct(arch);
#ifdef CONFIG_KRG_STREAM
deffct(stream);
#endif
deffct(kddm);
deffct(kermm);
#ifdef CONFIG_KRG_DVFS
deffct(dvfs);
#endif
#ifdef CONFIG_KRG_IPC
deffct(keripc);
#endif
#ifdef CONFIG_KRG_CAP
deffct(krg_cap);
#endif
#ifdef CONFIG_KRG_PROCFS
deffct(procfs);
#endif
#ifdef CONFIG_KRG_PROC
deffct(proc);
#endif
#ifdef CONFIG_KRG_EPM
deffct(ghost);
deffct(epm);
#endif
#ifdef CONFIG_KRG_SCHED
deffct(scheduler);
#endif

int init_kerrighed_communication_system(void)
{
	printk("Init Kerrighed worker(s)...");
	krg_wq = create_workqueue("krg");
	krg_nb_wq = create_workqueue("krgNB");
	BUG_ON(krg_wq == NULL);
	BUG_ON(krg_nb_wq == NULL);

	printk("Init Kerrighed low-level framework...\n");

	if (init_tools())
		goto err_tools;

	kerrighed_nb_nodes = 0;

#ifdef CONFIG_KRG_COMMUNICATION_FRAMEWORK
	if (init_rpc())
		goto err_rpc;
#endif

#ifdef CONFIG_KRG_HOTPLUG
	if (init_hotplug())
		goto err_hotplug;
#endif

	printk("Init Kerrighed low-level framework (nodeid %d) : done\n", kerrighed_node_id);

	return 0;

#ifdef CONFIG_KRG_HOTPLUG
      err_hotplug:
	cleanup_hotplug();
#endif
#ifdef CONFIG_KRG_COMMUNICATION_FRAMEWORK
      err_rpc:
#endif
	cleanup_tools();
      err_tools:
	return -1;
}

int init_kerrighed_upper_layers(void)
{
	printk("Init Kerrighed distributed services...\n");

	if (init_arch())
		goto err_arch;

#ifdef CONFIG_KRG_CTNR
	if (init_kddm())
		goto err_kddm;
#endif

#ifdef CONFIG_KRG_EPM
	if (init_ghost())
		goto err_ghost;
#endif

#ifdef CONFIG_KRG_STREAM
	if (init_stream())
		goto err_palantir;
#endif

#ifdef CONFIG_KRG_MM
	if (init_kermm())
		goto err_kermm;
#endif

#ifdef CONFIG_KRG_DVFS
	if (init_dvfs())
		goto err_dvfs;
#endif

#ifdef CONFIG_KRG_IPC
	if (init_keripc())
		goto err_keripc;
#endif

#ifdef CONFIG_KRG_CAP
	if (init_krg_cap())
		goto err_krg_cap;
#endif

#ifdef CONFIG_KRG_PROC
	if (init_proc())
		goto err_proc;
#endif

#ifdef CONFIG_KRG_PROCFS
	if (init_procfs())
		goto err_procfs;
#endif

#ifdef CONFIG_KRG_EPM
	if (init_epm())
		goto err_epm;
#endif

	printk("Init Kerrighed distributed services: done\n");

#ifdef CONFIG_KRG_SCHED
	if (init_scheduler())
		goto err_sched;
#endif

	return 0;

#ifdef CONFIG_KRG_SCHED
	cleanup_scheduler();
      err_sched:
#endif
#ifdef CONFIG_KRG_EPM
	cleanup_epm();
      err_epm:
#endif
#ifdef CONFIG_KRG_IPC
	cleanup_keripc();
      err_keripc:
#endif
#ifdef CONFIG_KRG_DVFS
	cleanup_dvfs();
      err_dvfs:
#endif
#ifdef CONFIG_KRG_PROCFS
	cleanup_procfs();
      err_procfs:
#endif
#ifdef CONFIG_KRG_PROC
	cleanup_proc();
      err_proc:
#endif
#ifdef CONFIG_KRG_CAP
	cleanup_krg_cap();
      err_krg_cap:
#endif
#ifdef CONFIG_KRG_MM
	cleanup_kermm();
      err_kermm:
#endif
#ifdef CONFIG_KRG_CTNR
	cleanup_kddm();
      err_kddm:
#endif
	cleanup_arch();
      err_arch:
#ifdef CONFIG_KRG_STREAM
	cleanup_stream();
      err_palantir:
#endif
#ifdef CONFIG_KRG_EPM
	cleanup_ghost();
      err_ghost:
#endif
#ifdef CONFIG_KRG_COMMUNICATION_FRAMEWORK
	cleanup_rpc();
#endif
	return -1;
}

int init_kerrighed(void)
{
	printk("Start loading Kerrighed...\n");

	debug_init("kerrighed");

	if (init_kerrighed_communication_system())
		return -1;

	if (init_kerrighed_upper_layers())
		return -1;

	SET_KERRIGHED_CLUSTER_FLAGS(KRGFLAGS_LOADED);
	SET_KERRIGHED_NODE_FLAGS(KRGFLAGS_LOADED);

	printk("Kerrighed... loaded!\n");

	rpc_enable(CLUSTER_START);
	rpc_connect();

	return 0;
};

void cleanup_kerrighed(void)
{
	printk("cleanup_kerrighed: TODO\n");

	debug_cleanup();
};

module_init(init_kerrighed);
module_exit(cleanup_kerrighed);
