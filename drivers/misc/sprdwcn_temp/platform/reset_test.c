#include <linux/notifier.h>

#include "wcn_glb.h"

static int wcn_reset(struct notifier_block *this, unsigned long ev, void *ptr)
{
	WCN_INFO("%s: reset callback coming\n", __func__);

	return NOTIFY_DONE;
}

static struct notifier_block wcn_reset_block = {
	.notifier_call = wcn_reset,
};

int reset_test_init(void)
{
	atomic_notifier_chain_register(&wcn_reset_notifier_list,
				       &wcn_reset_block);

	return 0;
}

