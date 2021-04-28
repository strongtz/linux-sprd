#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/ftrace.h>
#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <uapi/linux/sched/types.h>
#include "../../sprd_debugfs.h"

#define DEFAULT_SAMPLE_TIMEVALE 1000
#define DEFAULT_THRESHOLD_IRQ   3000
#define MAX_TIMEVALE 4294967296
#define BRUST_ARRAY_SIZE 10

struct irq_monitor_s {
	struct irq_domain *domain;
	unsigned long hwirq;
	int brust_value;
	int brust_times;
	int scale_brust_value;
	unsigned int prev_kstat_irq;
	bool mark;
	int history_brust_value[BRUST_ARRAY_SIZE];
};

static struct hrtimer irq_monitor_timer;
static int  monitor_enable = 1;
static unsigned int  time_interval = DEFAULT_SAMPLE_TIMEVALE;
static unsigned int  threshold_irq = DEFAULT_THRESHOLD_IRQ;
static spinlock_t irq_monitor_lock;
static struct irq_monitor_s *irq_monitor;
static int save_nr_irqs;
static struct task_struct *irqs_change_task;
static bool Processing;

static enum hrtimer_restart scan_burst_irq(struct hrtimer *hr)
{
	int i, j, irq_occur_value, index;
	unsigned int tmp_kstat_irq;
	struct irq_desc *desc;
	struct irqaction *action;
	int stat = 0;

	spin_lock(&irq_monitor_lock);
	for (i = 0; i < save_nr_irqs; i++) {
		desc = irq_to_desc(i);
		if (!desc)
			continue;

		raw_spin_lock(&desc->lock);
		action = desc->action;
		if (action == NULL) {
			raw_spin_unlock(&desc->lock);
			continue;
		}

		tmp_kstat_irq = 0;
		if (likely(irq_monitor[i].prev_kstat_irq != 0)) {
			if ((irq_monitor[i].domain != desc->irq_data.domain) ||
			    (irq_monitor[i].hwirq != desc->irq_data.hwirq)) {
				stat = 1;
				raw_spin_unlock(&desc->lock);
				continue;
			}

			for_each_present_cpu(j)
				tmp_kstat_irq += kstat_irqs_cpu(i, j);

			irq_occur_value =
			(int)(tmp_kstat_irq-irq_monitor[i].prev_kstat_irq);
			if (irq_occur_value != 0) {
				if (irq_occur_value > threshold_irq)
					pr_warning("Irq_monitor:Irq %45s[%d]occur %11d times per %d ms\n",
					action->name, i,
					irq_occur_value,
					time_interval);

				if (irq_monitor[i].mark == true &&
				(irq_occur_value > irq_monitor[i].brust_value)) {
					index =
					irq_monitor[i].brust_times % BRUST_ARRAY_SIZE;
					irq_monitor[i].history_brust_value[index] =
					irq_occur_value;
					irq_monitor[i].brust_times++;
				}

				irq_monitor[i].prev_kstat_irq = tmp_kstat_irq;
			}
		} else {
			for_each_present_cpu(j)
				irq_monitor[i].prev_kstat_irq += kstat_irqs_cpu(i, j);

			irq_monitor[i].domain = desc->irq_data.domain;
			irq_monitor[i].hwirq = desc->irq_data.hwirq;
		}
		raw_spin_unlock(&desc->lock);
	}

	if ((save_nr_irqs != nr_irqs || stat == 1) && Processing == false)
		wake_up_process(irqs_change_task);

	spin_unlock(&irq_monitor_lock);

	hrtimer_forward_now(&irq_monitor_timer, ms_to_ktime(time_interval));

	return HRTIMER_RESTART;
}

static int irq_init(int irq, int max_irq_num)
{
	u64 mid_value;
	u32 res;
	unsigned long flags;

	if (irq >= save_nr_irqs || irq < 0)
		return -EINVAL;

	if (max_irq_num < 0 || max_irq_num > MAX_TIMEVALE)
		return -EINVAL;

	spin_lock_irqsave(&irq_monitor_lock, flags);
	irq_monitor[irq].scale_brust_value = max_irq_num;
	mid_value = time_interval*max_irq_num;
	res = do_div(mid_value, DEFAULT_SAMPLE_TIMEVALE);
	irq_monitor[irq].brust_value = (res == 0 ? mid_value:mid_value+1);
	irq_monitor[irq].prev_kstat_irq = 0;
	irq_monitor[irq].brust_times = 0;
	irq_monitor[irq].mark = true;
	memset(irq_monitor[irq].history_brust_value, 0, BRUST_ARRAY_SIZE * sizeof(int));
	spin_unlock_irqrestore(&irq_monitor_lock, flags);

	return 0;
}

static int  irq_show(struct seq_file *m, void *v)
{
	struct irqaction *action;
	struct irq_desc *desc;
	unsigned long  flags, any_count;
	int i, j, index, tmp;

	spin_lock_irqsave(&irq_monitor_lock, flags);
	for (i = 0; i < save_nr_irqs; ++i) {
		if (irq_monitor[i].mark) {
			desc = irq_to_desc(i);
			if (!desc)
				continue;

			any_count = 0;
			raw_spin_lock(&desc->lock);
			for_each_present_cpu(j)
				any_count |= kstat_irqs_cpu(i, j);

			action = desc->action;
			if (!action || !any_count) {
				raw_spin_unlock(&desc->lock);
				continue;
			}

			if (desc->name)
				seq_printf(m, "-%-8s", desc->name);

			if (action) {
				seq_printf(m, "  %s ", action->name);
				while ((action = action->next) != NULL)
					seq_printf(m, ", %s ", action->name);
			}

			raw_spin_unlock(&desc->lock);

			seq_printf(m, "brust_value=%d ", irq_monitor[i].brust_value);
			seq_printf(m, "scale_brust_value=%d ", irq_monitor[i].scale_brust_value);
			seq_printf(m, "prev_kstat_irq=%u ", irq_monitor[i].prev_kstat_irq);

			if (irq_monitor[i].brust_times != 0) {
				tmp = 0;
				for (index = 0; index < BRUST_ARRAY_SIZE; index++) {
					if (irq_monitor[i].history_brust_value[index] > tmp)
						tmp = irq_monitor[i].history_brust_value[index];
				}
				seq_printf(m, "max history_brust_value=%d ", tmp);
				seq_printf(m, "brust_times=%d ", irq_monitor[i].brust_times);
				seq_printf(m, "this irq maybe cause irq storm!!!");
			}
			seq_putc(m, '\n');
		}
	}
	spin_unlock_irqrestore(&irq_monitor_lock, flags);

	return 0;
}

static int  irq_open(struct inode *inodep, struct file *filep)
{
	single_open(filep, irq_show, NULL);
	return 0;
}

static ssize_t irq_write(struct file *filep,
			const char __user *ubuf,
			size_t cnt,
			loff_t *ppos)
{
	char *buf, *sptr, *token;
	int irq;
	int max_irq_num;
	ssize_t ret = cnt;

	if (!cnt)
		return 0;

	if (cnt >= PAGE_SIZE)
		return -EINVAL;

	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, ubuf, cnt)) {
		free_page((unsigned long)buf);
		return -EFAULT;
	}

	buf[cnt] = '\0';
	strim(buf);
	sptr = buf;
	token = strsep(&sptr, " ");
	if ((!token) ||  kstrtouint(token, 0, &irq)) {
		ret = -EINVAL;
		goto error;
	}

	token = strsep(&sptr, " ");
	if ((!token) || kstrtouint(token, 0, &max_irq_num)) {
		ret = -EINVAL;
		goto error;
	}

	if (irq_init(irq, max_irq_num))
		ret = -EINVAL;

error:
	free_page((unsigned long) buf);

	return ret;
}


const struct file_operations irq_fops = {
	.open    = irq_open,
	.read    = seq_read,
	.write   = irq_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int monitor_enable_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", monitor_enable);
	return 0;
}

static int monitor_enable_open(struct inode *inodep,
				struct file *filep)
{
	single_open(filep, monitor_enable_show, NULL);
	return 0;
}

static ssize_t monitor_enable_write(struct file *filep,
					const char __user *ubuf,
					size_t cnt,
					loff_t *ppos)
{
	int enable;
	int err;

	err = kstrtoint_from_user(ubuf, cnt, 0, &enable);
	if (err)
		return -EINVAL;

	if (enable == 1) {
		hrtimer_cancel(&irq_monitor_timer);
		hrtimer_init(&irq_monitor_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		irq_monitor_timer.function = scan_burst_irq;
		hrtimer_start(&irq_monitor_timer, ms_to_ktime(time_interval),
			      HRTIMER_MODE_REL);
	} else if (enable == 0) {
		hrtimer_cancel(&irq_monitor_timer);
	} else
		return -EINVAL;

	monitor_enable = enable;

	return cnt;
}


const struct file_operations monitor_enable_fops = {
	.open    = monitor_enable_open,
	.read    = seq_read,
	.write   = monitor_enable_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int time_interval_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", time_interval);
	return 0;
}

static int time_interval_open(struct inode *inodep,
				struct file *filep)
{
	single_open(filep, time_interval_show, NULL);
	return 0;
}

static ssize_t time_interval_write(struct file *filep,
				const char __user *buf,
				size_t len,
				loff_t *ppos)
{
	unsigned long long interval;
	u64 mid_value;
	u32 res;
	int err, i;

	err = kstrtoull_from_user(buf, len, 0, &interval);
	if (err)
		return -EINVAL;

	if (interval < 100 || interval > MAX_TIMEVALE)
		return -EINVAL;

	if (time_interval != interval) {
		hrtimer_cancel(&irq_monitor_timer);
		spin_lock(&irq_monitor_lock);
		for (i = 0; i < save_nr_irqs; ++i) {
			if (irq_monitor[i].mark) {
				mid_value = interval*irq_monitor[i].scale_brust_value;
				res = do_div(mid_value, DEFAULT_SAMPLE_TIMEVALE);
				irq_monitor[i].brust_value = (res == 0 ? mid_value:mid_value+1);
				irq_monitor[i].brust_times = 0;
				irq_monitor[i].prev_kstat_irq = 0;
				memset(irq_monitor[i].history_brust_value, 0, BRUST_ARRAY_SIZE * sizeof(int));
			}
		}
		mid_value = interval*DEFAULT_THRESHOLD_IRQ;
		res = do_div(mid_value, DEFAULT_SAMPLE_TIMEVALE);
		threshold_irq = (unsigned int)mid_value;
		spin_unlock(&irq_monitor_lock);
		hrtimer_init(&irq_monitor_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		irq_monitor_timer.function = scan_burst_irq;
		hrtimer_start(&irq_monitor_timer, ms_to_ktime(interval), HRTIMER_MODE_REL);
	}

	time_interval = interval;

	return len;
}

const struct file_operations time_interval_fops = {
	.open    = time_interval_open,
	.read    = seq_read,
	.write   = time_interval_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int threshold_irq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", threshold_irq);
	return 0;
}

static int threshold_irq_open(struct inode *inodep,
				struct file *filep)
{
	single_open(filep, threshold_irq_show, NULL);
	return 0;
}

static ssize_t threshold_irq_write(struct file *filep,
					const char __user *ubuf,
					size_t cnt,
					loff_t *ppos)
{
	int err;
	unsigned int threshold;

	err = kstrtouint_from_user(ubuf, cnt, 0, &threshold);
	if (err)
		return -EINVAL;

	threshold_irq = threshold;

	return cnt;
}


const struct file_operations threshold_irq_fops = {
	.open    = threshold_irq_open,
	.read    = seq_read,
	.write   = threshold_irq_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int monitor_irqs_change(void *data)
{
	int tmp_nr_irqs, i, j;
	unsigned long  flags, hwirq;
	struct irq_desc *desc;
	struct irq_domain *domain;
	struct irq_monitor_s *tmp_irq_monitor;
	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);
	while (!kthread_should_stop()) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();

		Processing = true;
		do {
			tmp_nr_irqs = max(save_nr_irqs, nr_irqs);
			tmp_irq_monitor = kzalloc(sizeof(struct irq_monitor_s)*
						  (tmp_nr_irqs), GFP_KERNEL);
			if (!tmp_irq_monitor)
				msleep(50);
		} while (!tmp_irq_monitor);

		spin_lock_irqsave(&irq_monitor_lock, flags);
		for (i = 0; i < save_nr_irqs; i++) {
			desc = irq_to_desc(i);
			if (!desc)
				continue;

			raw_spin_lock(&desc->lock);
			domain = desc->irq_data.domain;
			hwirq = desc->irq_data.hwirq;
			raw_spin_unlock(&desc->lock);
			if ((irq_monitor[i].domain != domain) ||
			    (irq_monitor[i].hwirq != hwirq)) {

				for (j = 0; j < save_nr_irqs; j++) {
					if ((irq_monitor[j].domain == domain) &&
					    (irq_monitor[j].hwirq == hwirq)) {
						memcpy(&tmp_irq_monitor[i],
						       &irq_monitor[j],
						       sizeof(struct irq_monitor_s));
						irq_monitor[i].hwirq = hwirq;
						break;
					}
				}
			} else
				memcpy(&tmp_irq_monitor[i],
				       &irq_monitor[i],
				       sizeof(struct irq_monitor_s));
		}

		kfree(irq_monitor);
		save_nr_irqs = tmp_nr_irqs;
		irq_monitor = tmp_irq_monitor;
		spin_unlock_irqrestore(&irq_monitor_lock, flags);
		Processing = false;
	}

	return 0;
}

static int __init irq_monitor_init(void)
{
	struct dentry *irq_burst_monitor;

	save_nr_irqs = nr_irqs;
	Processing = false;
	irq_monitor = kzalloc(sizeof(struct irq_monitor_s)*
			      (save_nr_irqs), GFP_KERNEL);
	if (!irq_monitor)
		return -ENOMEM;

	spin_lock_init(&irq_monitor_lock);

	irqs_change_task =
	kthread_create(monitor_irqs_change, NULL, "irqs_change");
	if (IS_ERR(irqs_change_task)) {
		pr_err("create irqs_change_task failed");
		kfree(irq_monitor);
		return -ENOMEM;
	}

	irq_burst_monitor = debugfs_create_dir("irq_burst_monitor",
					       sprd_debugfs_entry(IRQ));

	if (irq_burst_monitor) {
		debugfs_create_file("irq", (S_IRUGO | S_IWUSR | S_IWGRP),
				    irq_burst_monitor, NULL, &irq_fops);
		debugfs_create_file("time_interval", (S_IRUGO | S_IWUSR | S_IWGRP),
				    irq_burst_monitor, NULL, &time_interval_fops);
		debugfs_create_file("monitor_enable", (S_IRUGO | S_IWUSR | S_IWGRP),
				    irq_burst_monitor, NULL, &monitor_enable_fops);
		debugfs_create_file("threshold_irq", (S_IRUGO | S_IWUSR | S_IWGRP),
				    irq_burst_monitor, NULL, &threshold_irq_fops);
	}

	hrtimer_init(&irq_monitor_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	irq_monitor_timer.function = scan_burst_irq;
	hrtimer_start(&irq_monitor_timer, ms_to_ktime(time_interval), HRTIMER_MODE_REL);

	return 0;
}
fs_initcall(irq_monitor_init);
