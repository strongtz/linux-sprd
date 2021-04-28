/*
 * This function include:
 * 1. register reset callback
 * 2. notify BT FM WIFI GNSS CP2 Assert
 */

#include <linux/debug_locks.h>
#include <linux/sched/debug.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/vt_kern.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>

#include "wcn_glb.h"

ATOMIC_NOTIFIER_HEAD(wcn_reset_notifier_list);
EXPORT_SYMBOL(wcn_reset_notifier_list);

void wcn_reset_cp2(void)
{
	wcn_chip_power_off();
	atomic_notifier_call_chain(&wcn_reset_notifier_list, 0, NULL);
}
EXPORT_SYMBOL(wcn_reset_cp2);
