/*
 *  Kerrighed/modules/scheduler_old/analyzer.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 */

/** Mosix probe analyzer.
 *  @file analyzer.c
 *
 *  @author Renaud Lottiaux
 */

#include <linux/jiffies.h>
#include <asm/atomic.h>

#include "krg_scheduler.h"
#include "analyzer.h"

static unsigned long last_migration_raised = 0;
static atomic_t migration_on_going = ATOMIC_INIT(0);

void send_alarm_to_analyzer(void)
{
	int now = jiffies;

	if (atomic_read(&migration_on_going))
		return;

	if (now - last_migration_raised < MIN_TICKS_BETWEEN_ALARM)
		return;

	wake_up_migration_thread();
}

void notify_migration_start_to_analyzer(void)
{
	atomic_inc(&migration_on_going);
}

void notify_migration_end_to_analyzer(void)
{
	atomic_dec(&migration_on_going);

	last_migration_raised = jiffies;
}
