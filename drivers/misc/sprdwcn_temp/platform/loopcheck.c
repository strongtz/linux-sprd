#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include "wcn_glb.h"

#define LOOPCHECK_TIMER_INTERVAL      8
#define USERDEBUG 1

static struct timer_list loopcheck_timer;
static struct completion loopcheck_completion;
static struct completion atcmd_completion;
struct work_struct loopcheck_work;
struct workqueue_struct *loopcheck_workqueue;
unsigned int (*cp_assert_cb_matrix[2])(unsigned int type) = {0};

int loopcheck_send(char *buf, unsigned int len)
{
	unsigned char *send_buf = NULL;
	struct mbuf_t *head, *tail;
	int num = 1;

	WCN_INFO("%s len=%d\n", __func__, len);
	if (unlikely(!marlin_get_module_status())) {
		WCN_ERR("WCN module have not open\n");
		return -EIO;
	}
	send_buf = kzalloc(len + PUB_HEAD_RSV + 1, GFP_KERNEL);
	if (!send_buf)
		return -ENOMEM;
	memcpy(send_buf + PUB_HEAD_RSV, buf, len);

	if (!sprdwcn_bus_list_alloc(mdbg_proc_ops[MDBG_AT_TX_OPS].channel,
				    &head, &tail, &num)) {
		head->buf = send_buf;
		head->len = len;
		head->next = NULL;
		sprdwcn_bus_push_list(mdbg_proc_ops[MDBG_AT_TX_OPS].channel,
				      head, tail, num);
	}
	return len;
}

static void loopcheck_work_queue(struct work_struct *work)
{
	unsigned long timeleft;

	char a[] = "at+loopcheck\r\n";

	loopcheck_send(a, sizeof(a));

	timeleft = wait_for_completion_timeout(&loopcheck_completion, (3 * HZ));
	if (!timeleft) {
		WCN_ERR("didn't get loopcheck ack\n");
		WCN_INFO("start dump CP2 mem\n");
#ifdef CONFIG_WCN_USER
		wcn_reset_cp2();
#else
		mdbg_dump_mem();
#endif

		stop_loopcheck_timer();
	}
}

static void loopcheck_timer_expire(unsigned long data)
{
	WCN_INFO("%s\n", __func__);
	mod_timer(&loopcheck_timer, jiffies + (LOOPCHECK_TIMER_INTERVAL * HZ));

	if (!work_pending(&loopcheck_work))
		queue_work(loopcheck_workqueue, &loopcheck_work);
}

void close_cp2_log(void)
{
	unsigned long timeleft;
	char a[] = "at+armlog=0\r\n";

	msleep(20);
	loopcheck_send(a, sizeof(a));

	timeleft = wait_for_completion_timeout(&atcmd_completion, (3 * HZ));
	if (!timeleft)
		WCN_ERR("didn't get close CP2 log ack\n");
}

void start_loopcheck_timer(void)
{
	WCN_INFO("%s\n", __func__);

#ifdef CONFIG_WCN_USER
	close_cp2_log();
#endif

	mod_timer(&loopcheck_timer, jiffies + (1 * HZ));

}

void stop_loopcheck_timer(void)
{
	WCN_INFO("%s\n", __func__);
	del_timer(&loopcheck_timer);
}

void complete_kernel_loopcheck(void)
{
	complete(&loopcheck_completion);
}

void complete_kernel_atcmd(void)
{
	complete(&atcmd_completion);
}

unsigned int loopcheck_register_cb(unsigned int type, void *func)
{
	if (type == 0)
		cp_assert_cb_matrix[0] = func;
	if (type == 1)
		cp_assert_cb_matrix[1] = func;

	return 0;
}

int loopcheck_init(void)
{
	WCN_INFO("%s\n", __func__);
	init_timer(&loopcheck_timer);
	loopcheck_timer.function = loopcheck_timer_expire;
	loopcheck_timer.data = 0;
	init_completion(&loopcheck_completion);
	loopcheck_workqueue =
		create_singlethread_workqueue("SPRD_LOOPCHECK_QUEUE");
	if (!loopcheck_workqueue) {
		WCN_ERR("%s SPRD_LOOPCHECK_QUEUE create failed", __func__);
		return -ENOMEM;
	}
	INIT_WORK(&loopcheck_work, loopcheck_work_queue);
	init_completion(&atcmd_completion);

	return 0;
}

int loopcheck_deinit(void)
{
	stop_loopcheck_timer();
	flush_workqueue(loopcheck_workqueue);
	destroy_workqueue(loopcheck_workqueue);

	return 0;
}
