/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/sipc.h>
#include <asm/pgtable.h>
#include <uapi/linux/sched/types.h>

#include "sipc_priv.h"
#include "sbuf.h"

#if defined(CONFIG_DEBUG_FS)
#include "sipc_debugfs.h"
#endif

#define VOLA_SBUF_SMEM volatile struct sbuf_smem_header
#define VOLA_SBUF_RING volatile struct sbuf_ring_header

struct name_node {
	struct list_head list;
	char comm[TASK_COMM_LEN];
	pid_t pid;
	u8 latest;
};

union sbuf_buf {
	void *buf;
	void __user *ubuf;
};

enum task_type {
	TASK_RXWAIT = 0,
	TASK_TXWAIT,
	TASK_SELECT
};

static struct sbuf_mgr *sbufs[SIPC_ID_NR][SMSG_VALID_CH_NR];

static bool sbuf_is_task_pointer(const void *ptr)
{
	struct task_struct *task;
	struct thread_info *thread_info;

	task = (struct task_struct *)ptr;
	if (IS_ERR_OR_NULL(task) || !virt_addr_valid(task))
		return false;

#ifndef CONFIG_THREAD_INFO_IN_TASK
	/* in this case thread_info is in the same addres with stack thread_union*/
	if (IS_ERR_OR_NULL(task->stack) || !virt_addr_valid(task->stack))
		return false;
#endif

	thread_info = task_thread_info(task);
	if (IS_ERR_OR_NULL(thread_info) || !virt_addr_valid(thread_info))
		return false;

	return (thread_info->addr_limit == KERNEL_DS ||
		thread_info->addr_limit == USER_DS);
}

static struct task_struct *sbuf_wait_get_task(wait_queue_entry_t *pos, u32 *b_select)
{
	struct task_struct *task;
	struct poll_wqueues *table;

	if (!pos->private)
		return NULL;

	/* if the private is put into wait list by sbuf_read, the struct of
	 * pos->private is struct task_struct
	 * if the private is put into list by sbuf_poll, the struct of
	 * pos->private is struct poll_wqueues
	 */

	/* firstly, try struct poll_wqueues */
	table = (struct poll_wqueues *)pos->private;
	task = table->polling_task;
	if (sbuf_is_task_pointer(task)) {
		*b_select = 1;
		return task;
	}

	/* firstly, try convert it with the struct task_struct */
	task = (struct task_struct *)pos->private;

	if (sbuf_is_task_pointer(task)) {
		*b_select = 0;
		return task;
	}

	return NULL;
}

#if defined(SIPC_DEBUG_SBUF_RDWT_OWNER)
static void sbuf_record_rdwt_owner(struct sbuf_ring *ring, int b_rx)
{
	int b_add;
	struct name_node *pos = NULL;
	struct list_head *owner_list;
	unsigned long flags;

	b_add = 1;
	owner_list = b_rx ? (&ring->rx_list) : (&ring->tx_list);

	spin_lock_irqsave(&ring->rxwait.lock, flags);
	list_for_each_entry(pos, owner_list, list) {
		if (pos->pid == current->pid) {
			b_add = 0;
			pos->latest = 1;
			break;
		}
		if (pos->latest)
			pos->latest = 0;
	}
	spin_unlock_irqrestore(&ring->rxwait.lock, flags);

	if (b_add) {
		pos = kzalloc(sizeof(*pos), GFP_KERNEL);
		if (pos) {
			memcpy(pos->comm, current->comm, TASK_COMM_LEN);
			pos->pid = current->pid;
			pos->latest = 1;
			spin_lock_irqsave(&ring->rxwait.lock, flags);
			list_add_tail(&pos->list, owner_list);
			spin_unlock_irqrestore(&ring->rxwait.lock, flags);
		}
	}
}

static void sbuf_destroy_rdwt_owner(struct sbuf_ring *ring)
{
	struct name_node *pos, *temp;
	unsigned long flags;

	spin_lock_irqsave(&ring->rxwait.lock, flags);
	/* free task node */
	list_for_each_entry_safe(pos,
				 temp,
				 &ring->rx_list,
				 list) {
		kfree(pos);
	}

	list_for_each_entry_safe(pos,
				 temp,
				 &ring->tx_list,
				 list) {
		kfree(pos);
	}
	spin_unlock_irqrestore(&ring->rxwait.lock, flags);
}
#endif

static void sbuf_skip_old_data(struct sbuf_mgr *sbuf)
{
	struct sbuf_ring *ring = NULL;
	VOLA_SBUF_RING *ringhd = NULL;
	u32 i;

	for (i = 0; i < sbuf->ringnr; i++) {
		ring = &sbuf->rings[i];
		ringhd = ring->header;

		/* clean sbuf tx ring , sbuf tx ring no need to clear */
		/* ringhd->txbuf_wrptr = ringhd->txbuf_rdptr; */
		/* clean sbuf rx ring */
		ringhd->rxbuf_rdptr = ringhd->rxbuf_wrptr;
	}
}

static int sbuf_thread(void *data)
{
	struct sbuf_mgr *sbuf = data;
	struct sbuf_ring *ring;
	struct smsg mcmd, mrecv;
	int rval, bufid;
	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	/* since the channel open may hang, we call it in the sbuf thread */
	rval = smsg_ch_open(sbuf->dst, sbuf->channel, -1);
	if (rval != 0) {
		pr_err("Failed to open channel %d\n", sbuf->channel);
		/* assign NULL to thread poniter as failed to open channel */
		sbuf->thread = NULL;
		return rval;
	}

	/* sbuf init done, handle the ring rx events */
	while (!kthread_should_stop()) {
		/* monitor sbuf rdptr/wrptr update smsg */
		smsg_set(&mrecv, sbuf->channel, 0, 0, 0);
		rval = smsg_recv(sbuf->dst, &mrecv, -1);

		if (rval == -EIO) {
			/* channel state is free */
			msleep(20);
			continue;
		}

		pr_debug("sbuf thread recv msg: dst=%d, channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			 sbuf->dst,
			 sbuf->channel,
			 mrecv.type,
			 mrecv.flag,
			 mrecv.value);

		switch (mrecv.type) {
		case SMSG_TYPE_OPEN:
			/* if channel state is already reay, reopen it
			 * (such as modem reset), we must skip the old
			 * buf data , than give open ack and reset state
			 * to idle
			 */
			if (sbuf->state == SBUF_STATE_READY) {
				sbuf_skip_old_data(sbuf);
				sbuf->state = SBUF_STATE_IDLE;
			}
			/* handle channel open */
			smsg_open_ack(sbuf->dst, sbuf->channel);
			break;
		case SMSG_TYPE_CLOSE:
			/* handle channel close */
			sbuf_skip_old_data(sbuf);
			smsg_close_ack(sbuf->dst, sbuf->channel);
			sbuf->state = SBUF_STATE_IDLE;
			break;
		case SMSG_TYPE_CMD:
			/* respond cmd done for sbuf init only state is idle */
			if (sbuf->state == SBUF_STATE_IDLE) {
				smsg_set(&mcmd,
					 sbuf->channel,
					 SMSG_TYPE_DONE,
					 SMSG_DONE_SBUF_INIT,
					 sbuf->mapped_smem_addr);
				smsg_send(sbuf->dst, &mcmd, -1);
				sbuf->state = SBUF_STATE_READY;
			} else {
				pr_err("sbuf thread recv msg err: dst=%d, channel=%d, type=%d\n",
				       sbuf->dst,
				       sbuf->channel,
				       mrecv.type);
			}
			break;
		case SMSG_TYPE_EVENT:
			bufid = mrecv.value;
			WARN_ON(bufid >= sbuf->ringnr);
			ring = &sbuf->rings[bufid];
			switch (mrecv.flag) {
			case SMSG_EVENT_SBUF_RDPTR:
				wake_up_interruptible_all(&ring->txwait);
				if (ring->handler)
					ring->handler(SBUF_NOTIFY_WRITE,
						      ring->data);
				if (ring->need_wake_lock) {
					__pm_wakeup_event(&ring->tx_wake_lock, jiffies_to_msecs(HZ / 2));
					ring->tx_wakelock_state = 1;
				}
				break;
			case SMSG_EVENT_SBUF_WRPTR:
				wake_up_interruptible_all(&ring->rxwait);
				if (ring->handler)
					ring->handler(SBUF_NOTIFY_READ,
						      ring->data);
				if (ring->need_wake_lock) {
					__pm_wakeup_event(&ring->rx_wake_lock, jiffies_to_msecs(HZ / 2));
					ring->rx_wakelock_state = 1;
				}
				break;
			default:
				rval = 1;
				break;
			}
			break;
		default:
			rval = 1;
			break;
		};

		if (rval) {
			pr_info("non-handled sbuf msg: %d-%d, %d, %d, %d\n",
				sbuf->dst,
				sbuf->channel,
				mrecv.type,
				mrecv.flag,
				mrecv.value);
			rval = 0;
		}
		/* unlock sipc channel wake lock */
		smsg_ch_wake_unlock(sbuf->dst, sbuf->channel);
	}

	return 0;
}

int sbuf_create_ex(u8 dst, u8 channel, u32 smem_idx,
		   u32 bufnum, u32 txbufsize, u32 rxbufsize)
{
	struct sbuf_mgr *sbuf;
	struct sipc_device *sdev;
	VOLA_SBUF_SMEM *smem;
	VOLA_SBUF_RING *ringhd;
	int hsize, i, result;
	u8 ch_index;
	struct sbuf_ring *ring;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	pr_debug("sbuf_create dst=%d, chanel=%d, bufnum=%d, txbufsize=0x%x, rxbufsize=0x%x\n",
		 dst,
		 channel,
		 bufnum,
		 txbufsize,
		 rxbufsize);

	if (dst >= SIPC_ID_NR) {
		pr_err("sbuf_create: dst = %d is invalid\n", dst);
		return -EINVAL;
	}

	sdev = sipc_ap.sipc_dev[dst];
	if (!sdev) {
		pr_err("sbuf_create: sdev is null, dst = %d\n", dst);
		return -ENODEV;
	}

	sbuf = kzalloc(sizeof(*sbuf), GFP_KERNEL);
	if (!sbuf)
		return -ENOMEM;

	sbuf->state = SBUF_STATE_IDLE;
	sbuf->dst = dst;
	sbuf->channel = channel;
	sbuf->smem = smem_idx;
	sbuf->ringnr = bufnum;

	/* allocate smem */
	hsize = sizeof(struct sbuf_smem_header) +
		sizeof(struct sbuf_ring_header) * bufnum;
	sbuf->smem_size = hsize + (txbufsize + rxbufsize) * bufnum;
	sbuf->smem_addr = smem_alloc_ex(sbuf->smem_size,
					sdev->pdata->smem_ptr[sbuf->smem].base);
	if (!sbuf->smem_addr) {
		pr_err("Failed to allocate smem for sbuf\n");
		kfree(sbuf);
		return -ENOMEM;
	}

	sbuf->mapped_smem_addr = sbuf->smem_addr -
				sdev->pdata->smem_ptr[sbuf->smem].base +
				sdev->pdata->smem_ptr[sbuf->smem].mapped_base;

	pr_debug("sbuf_create dst=%d, chanel=%d,bufnum=%d, smem_addr=0x%x, smem_size=0x%x, mapped_smem_addr=0x%x\n",
		 dst,
		 channel,
		 bufnum,
		 sbuf->smem_addr,
		 sbuf->smem_size,
		 sbuf->mapped_smem_addr);

	sbuf->smem_virt = shmem_ram_vmap_nocache(sbuf->smem_addr,
						sbuf->smem_size);
	if (!sbuf->smem_virt) {
		pr_err("Failed to map smem for sbuf\n");
		smem_free(sbuf->smem_addr, sbuf->smem_size);
		kfree(sbuf);
		return -EFAULT;
	}

	/* allocate rings description */
	sbuf->rings = kcalloc(bufnum, sizeof(struct sbuf_ring), GFP_KERNEL);
	if (!sbuf->rings) {
		shmem_ram_unmap(sbuf->smem_virt);
		smem_free(sbuf->smem_addr, sbuf->smem_size);
		kfree(sbuf);
		return -ENOMEM;
	}

	/* initialize all ring bufs */
	smem = (VOLA_SBUF_SMEM *)sbuf->smem_virt;
	smem->ringnr = bufnum;
	for (i = 0; i < bufnum; i++) {
		ringhd = (VOLA_SBUF_RING *)&smem->headers[i];
		ringhd->txbuf_addr = sbuf->mapped_smem_addr + hsize +
				(txbufsize + rxbufsize) * i;
		ringhd->txbuf_size = txbufsize;
		ringhd->txbuf_rdptr = 0;
		ringhd->txbuf_wrptr = 0;
		ringhd->rxbuf_addr = smem->headers[i].txbuf_addr + txbufsize;
		ringhd->rxbuf_size = rxbufsize;
		ringhd->rxbuf_rdptr = 0;
		ringhd->rxbuf_wrptr = 0;

		ring = &sbuf->rings[i];
		ring->header = ringhd;
		ring->txbuf_virt = sbuf->smem_virt + hsize +
				(txbufsize + rxbufsize) * i;
		ring->rxbuf_virt = ring->txbuf_virt + txbufsize;
		init_waitqueue_head(&ring->txwait);
		init_waitqueue_head(&ring->rxwait);
#if defined(SIPC_DEBUG_SBUF_RDWT_OWNER)
		INIT_LIST_HEAD(&ring->tx_list);
		INIT_LIST_HEAD(&ring->rx_list);
#endif
		mutex_init(&ring->txlock);
		mutex_init(&ring->rxlock);
		ring->need_wake_lock = 1;
		ring->tx_wakelock_state = 0;
		sprintf(ring->tx_wakelock_name,
			"sbuf-%d-%d-%d-tx",
			dst, channel, i);
		wakeup_source_init(&ring->tx_wake_lock,
			       ring->tx_wakelock_name);
		ring->rx_wakelock_state = 0;
		sprintf(ring->rx_wakelock_name,
			"sbuf-%d-%d-%d-rx",
			dst, channel, i);
		wakeup_source_init(&ring->rx_wake_lock,
			       ring->rx_wakelock_name);
	}

	sbuf->thread = kthread_create(sbuf_thread, sbuf,
			"sbuf-%d-%d", dst, channel);
	if (IS_ERR(sbuf->thread)) {
		pr_err("Failed to create kthread: sbuf-%d-%d\n", dst, channel);
		for (i = 0; i < bufnum; i++) {
			ring = &sbuf->rings[i];
			wakeup_source_trash(&ring->tx_wake_lock);
			wakeup_source_trash(&ring->rx_wake_lock);
		}
		kfree(sbuf->rings);
		shmem_ram_unmap(sbuf->smem_virt);
		smem_free(sbuf->smem_addr, sbuf->smem_size);
		result = PTR_ERR(sbuf->thread);
		kfree(sbuf);

		return result;
	}

	sbufs[dst][ch_index] = sbuf;
	wake_up_process(sbuf->thread);

	return 0;
}
EXPORT_SYMBOL_GPL(sbuf_create_ex);

void sbuf_set_no_need_wake_lock(u8 dst, u8 channel, u32 bufnum)
{
	u8 ch_index;
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return;
	}

	sbuf = sbufs[dst][ch_index];
	if (!sbuf || sbuf->ringnr <= bufnum)
		return;

	ring = &sbuf->rings[bufnum];
	ring->need_wake_lock = 0;
}
EXPORT_SYMBOL_GPL(sbuf_set_no_need_wake_lock);

void sbuf_destroy(u8 dst, u8 channel)
{
	struct sbuf_mgr *sbuf;
	int i;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return;
	}

	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return;

	sbuf->state = SBUF_STATE_IDLE;
	smsg_ch_close(dst, channel, -1);

	/* stop sbuf thread if it's created successfully and still alive */
	if (!IS_ERR_OR_NULL(sbuf->thread))
		kthread_stop(sbuf->thread);

	if (sbuf->rings) {
		for (i = 0; i < sbuf->ringnr; i++) {
			wake_up_interruptible_all(&sbuf->rings[i].txwait);
			wake_up_interruptible_all(&sbuf->rings[i].rxwait);
#if defined(SIPC_DEBUG_SBUF_RDWT_OWNER)
			sbuf_destroy_rdwt_owner(&sbuf->rings[i]);
#endif
			wakeup_source_trash(&sbuf->rings[i].tx_wake_lock);
			wakeup_source_trash(&sbuf->rings[i].rx_wake_lock);
		}
		kfree(sbuf->rings);
	}

	if (sbuf->smem_virt)
		shmem_ram_unmap(sbuf->smem_virt);
	smem_free(sbuf->smem_addr, sbuf->smem_size);
	kfree(sbuf);

	sbufs[dst][ch_index] = NULL;
}
EXPORT_SYMBOL_GPL(sbuf_destroy);

int sbuf_write(u8 dst, u8 channel, u32 bufid,
	       void *buf, u32 len, int timeout)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	VOLA_SBUF_RING *ringhd = NULL;
	struct smsg mevt;
	void *txpos;
	int rval, left, tail, txsize;
	u8 ch_index;
	union sbuf_buf u_buf;

	u_buf.buf = buf;
	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &sbuf->rings[bufid];
	ringhd = ring->header;
	if (sbuf->state != SBUF_STATE_READY) {
		pr_info("sbuf-%d-%d not ready to write!\n",
			dst, channel);
		return -ENODEV;
	}

	pr_debug("sbuf_write: dst=%d, channel=%d, bufid=%d, len=%d, timeout=%d\n",
		 dst,
		 channel,
		 bufid,
		 len,
		 timeout);
	pr_debug("sbuf_write: channel=%d, wrptr=%d, rdptr=%d",
		 channel,
		 ringhd->txbuf_wrptr,
		 ringhd->txbuf_rdptr);

	rval = 0;
	left = len;

	if (timeout) {
		mutex_lock(&ring->txlock);
	} else {
		if (!mutex_trylock(&ring->txlock)) {
			pr_debug("sbuf_read busy, dst=%d, channel=%d, bufid=%d\n",
				 dst, channel, bufid);
			return -EBUSY;
		}
	}

#if defined(SIPC_DEBUG_SBUF_RDWT_OWNER)
	sbuf_record_rdwt_owner(ring, 0);
#endif

	if (timeout == 0) {
		/* no wait */
		if ((int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) >=
				ringhd->txbuf_size) {
			pr_info("sbuf %d-%d ring %d txbuf is full!\n",
				dst, channel, bufid);
			rval = -EBUSY;
		}
	} else if (timeout < 0) {
		/* wait forever */
		rval = wait_event_interruptible(
			ring->txwait,
			(int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) <
			ringhd->txbuf_size ||
			sbuf->state == SBUF_STATE_IDLE);
		if (rval < 0)
			pr_debug("sbuf_write wait interrupted!\n");

		if (sbuf->state == SBUF_STATE_IDLE) {
			pr_err("sbuf_write sbuf state is idle!\n");
			rval = -EIO;
		}
	} else {
		/* wait timeout */
		rval = wait_event_interruptible_timeout(
			ring->txwait,
			(int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) <
			ringhd->txbuf_size ||
			sbuf->state == SBUF_STATE_IDLE,
			timeout);
		if (rval < 0) {
			pr_debug("sbuf_write wait interrupted!\n");
		} else if (rval == 0) {
			pr_info("sbuf_write wait timeout!\n");
			rval = -ETIME;
		}

		if (sbuf->state == SBUF_STATE_IDLE) {
			pr_err("sbuf_write sbuf state is idle!\n");
			rval = -EIO;
		}
	}

	while (left && (int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) <
	       ringhd->txbuf_size && sbuf->state == SBUF_STATE_READY) {
		/* calc txpos & txsize */
		txpos = ring->txbuf_virt +
			ringhd->txbuf_wrptr % ringhd->txbuf_size;
		txsize = ringhd->txbuf_size -
			(int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr);
		txsize = min(txsize, left);

		tail = txpos + txsize - (ring->txbuf_virt + ringhd->txbuf_size);
		if (tail > 0) {
			/* ring buffer is rounded */
			if ((uintptr_t)u_buf.buf > TASK_SIZE) {
				unalign_memcpy(txpos, u_buf.buf, txsize - tail);
				unalign_memcpy(ring->txbuf_virt,
					       u_buf.buf + txsize - tail, tail);
			} else {
				if (unalign_copy_from_user(
					txpos,
					u_buf.ubuf,
					txsize - tail) ||
				   unalign_copy_from_user(
					ring->txbuf_virt,
					u_buf.ubuf + txsize - tail,
					tail)) {
					pr_err("sbuf_write: failed to copy from user!\n");
					rval = -EFAULT;
					break;
				}
			}
		} else {
			if ((uintptr_t)u_buf.buf > TASK_SIZE) {
				unalign_memcpy(txpos, u_buf.buf, txsize);
			} else {
				/* handle the user space address */
				if (unalign_copy_from_user(
						txpos,
						u_buf.ubuf,
						txsize)) {
					pr_err("sbuf_write: failed to copy from user!\n");
					rval = -EFAULT;
					break;
				}
			}
		}

		pr_debug("sbuf_write: channel=%d, txpos=%p, txsize=%d\n",
			 channel, txpos, txsize);

		/* update tx wrptr */
		ringhd->txbuf_wrptr = ringhd->txbuf_wrptr + txsize;
		/* tx ringbuf is empty, so need to notify peer side */
		if (ringhd->txbuf_wrptr - ringhd->txbuf_rdptr == txsize) {
			smsg_set(&mevt, channel,
				 SMSG_TYPE_EVENT,
				 SMSG_EVENT_SBUF_WRPTR,
				 bufid);
			smsg_send(dst, &mevt, -1);
		}

		left -= txsize;
		u_buf.buf += txsize;
	}

	/* if write all data by user , release wake lock time to 20ms*/
	if (ring->tx_wakelock_state)
		__pm_wakeup_event(&ring->tx_wake_lock, jiffies_to_msecs(HZ / 50));

	ring->tx_wakelock_state = 0;

	mutex_unlock(&ring->txlock);

	pr_debug("sbuf_write done: channel=%d, len=%d\n", channel, len - left);

	if (len == left)
		return rval;
	else
		return (len - left);
}
EXPORT_SYMBOL_GPL(sbuf_write);

int sbuf_read(u8 dst, u8 channel, u32 bufid,
	      void *buf, u32 len, int timeout)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	VOLA_SBUF_RING *ringhd = NULL;
	struct smsg mevt;
	void *rxpos;
	int rval, left, tail, rxsize;
	u8 ch_index;
	union sbuf_buf u_buf;

	u_buf.buf = buf;
	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &sbuf->rings[bufid];
	ringhd = ring->header;

	if (sbuf->state != SBUF_STATE_READY) {
		pr_info("sbuf-%d-%d not ready to read!\n", dst, channel);
		return -ENODEV;
	}

	pr_debug("sbuf_read:dst=%d, channel=%d, bufid=%d, len=%d, timeout=%d\n",
		 dst, channel, bufid, len, timeout);
	pr_debug("sbuf_read: channel=%d, wrptr=%d, rdptr=%d",
		 channel,
		 ringhd->rxbuf_wrptr,
		 ringhd->rxbuf_rdptr);

	rval = 0;
	left = len;

	if (timeout) {
		mutex_lock(&ring->rxlock);
	} else {
		if (!mutex_trylock(&ring->rxlock)) {
			pr_debug("sbuf_read busy!,dst=%d, channel=%d, bufid=%d\n",
				 dst, channel, bufid);
			return -EBUSY;
		}
	}

#if defined(SIPC_DEBUG_SBUF_RDWT_OWNER)
	sbuf_record_rdwt_owner(ring, 1);
#endif

	if (ringhd->rxbuf_wrptr == ringhd->rxbuf_rdptr) {
		if (timeout == 0) {
			/* no wait */
			pr_debug("sbuf %d-%d ring %d rxbuf is empty!\n",
				 dst, channel, bufid);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(
				ring->rxwait,
				ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr ||
				sbuf->state == SBUF_STATE_IDLE);
			if (rval < 0)
				pr_debug("sbuf_read wait interrupted!\n");

			if (sbuf->state == SBUF_STATE_IDLE) {
				pr_err("sbuf_read sbuf state is idle!\n");
				rval = -EIO;
			}
		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(
				ring->rxwait,
				ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr ||
				sbuf->state == SBUF_STATE_IDLE, timeout);
			if (rval < 0) {
				pr_debug("sbuf_read wait interrupted!\n");
			} else if (rval == 0) {
				pr_info("sbuf_read wait timeout!\n");
				rval = -ETIME;
			}

			if (sbuf->state == SBUF_STATE_IDLE) {
				pr_err("sbuf_read sbuf state is idle!\n");
				rval = -EIO;
			}
		}
	}

	while (left &&
	       (ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr) &&
	       sbuf->state == SBUF_STATE_READY) {
		/* calc rxpos & rxsize */
		rxpos = ring->rxbuf_virt +
			ringhd->rxbuf_rdptr % ringhd->rxbuf_size;
		rxsize = (int)(ringhd->rxbuf_wrptr - ringhd->rxbuf_rdptr);
		/* check overrun */
		if (rxsize > ringhd->rxbuf_size)
			pr_err("sbuf_read: bufid = %d, channel= %d rxsize=0x%x, rdptr=%d, wrptr=%d",
			       bufid,
			       channel,
			       rxsize,
			       ringhd->rxbuf_wrptr,
			       ringhd->rxbuf_rdptr);

		rxsize = min(rxsize, left);

		pr_debug("sbuf_read: channel=%d, buf=%p, rxpos=%p, rxsize=%d\n",
			 channel, u_buf.buf, rxpos, rxsize);

		tail = rxpos + rxsize - (ring->rxbuf_virt + ringhd->rxbuf_size);

		if (tail > 0) {
			/* ring buffer is rounded */
			if ((uintptr_t)u_buf.buf > TASK_SIZE) {
				unalign_memcpy(u_buf.buf, rxpos, rxsize - tail);
				unalign_memcpy(u_buf.buf + rxsize - tail,
					       ring->rxbuf_virt, tail);
			} else {
				/* handle the user space address */
				if (unalign_copy_to_user(u_buf.ubuf,
							 rxpos,
							 rxsize - tail) ||
				    unalign_copy_to_user(u_buf.ubuf
							 + rxsize - tail,
							 ring->rxbuf_virt,
							 tail)) {
					pr_err("sbuf_read: failed to copy to user!\n");
					rval = -EFAULT;
					break;
				}
			}
		} else {
			if ((uintptr_t)u_buf.buf > TASK_SIZE) {
				unalign_memcpy(u_buf.buf, rxpos, rxsize);
			} else {
				/* handle the user space address */
				if (unalign_copy_to_user(u_buf.ubuf,
							 rxpos, rxsize)) {
					pr_err("sbuf_read: failed to copy to user!\n");
					rval = -EFAULT;
					break;
				}
			}
		}

		/* update rx rdptr */
		ringhd->rxbuf_rdptr = ringhd->rxbuf_rdptr + rxsize;
		/* rx ringbuf is full ,so need to notify peer side */
		if (ringhd->rxbuf_wrptr - ringhd->rxbuf_rdptr ==
		    ringhd->rxbuf_size - rxsize) {
			smsg_set(&mevt, channel,
				 SMSG_TYPE_EVENT,
				 SMSG_EVENT_SBUF_RDPTR,
				 bufid);
			smsg_send(dst, &mevt, -1);
		}

		left -= rxsize;
		u_buf.buf += rxsize;
	}

	/* if read empty by user , reset lock time to 20ms */
	if (ring->rx_wakelock_state)
		__pm_wakeup_event(&ring->rx_wake_lock, jiffies_to_msecs(HZ / 50));

	ring->rx_wakelock_state = 0;

	mutex_unlock(&ring->rxlock);

	pr_debug("sbuf_read done: channel=%d, len=%d", channel, len - left);

	if (len == left)
		return rval;
	else
		return (len - left);
}
EXPORT_SYMBOL_GPL(sbuf_read);

int sbuf_poll_wait(u8 dst, u8 channel, u32 bufid,
		   struct file *filp, poll_table *wait)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	VOLA_SBUF_RING *ringhd = NULL;
	unsigned int mask = 0;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &sbuf->rings[bufid];
	ringhd = ring->header;
	if (sbuf->state != SBUF_STATE_READY) {
		pr_err("sbuf-%d-%d not ready to poll !\n", dst, channel);
		return -ENODEV;
	}

	poll_wait(filp, &ring->txwait, wait);
	poll_wait(filp, &ring->rxwait, wait);

	if (ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr)
		mask |= POLLIN | POLLRDNORM;

	if (ringhd->txbuf_wrptr - ringhd->txbuf_rdptr < ringhd->txbuf_size)
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}
EXPORT_SYMBOL_GPL(sbuf_poll_wait);

int sbuf_status(u8 dst, u8 channel)
{
	struct sbuf_mgr *sbuf;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];

	if (!sbuf)
		return -ENODEV;
	if (sbuf->state != SBUF_STATE_READY)
		return -ENODEV;

	return 0;
}
EXPORT_SYMBOL_GPL(sbuf_status);

int sbuf_register_notifier(u8 dst, u8 channel, u32 bufid,
			   void (*handler)(int event, void *data), void *data)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &sbuf->rings[bufid];
	ring->handler = handler;
	ring->data = data;

	return 0;
}
EXPORT_SYMBOL_GPL(sbuf_register_notifier);

void sbuf_get_status(u8 dst, char *status_info, int size)
{
	struct sbuf_mgr *sbuf = NULL;
	struct sbuf_ring *ring = NULL;
	VOLA_SBUF_RING *ringhd = NULL;
	wait_queue_entry_t *pos;
	struct task_struct *task;
	unsigned long flags;
	int i, n, len, cnt;
	u32 b_select;
	char *phead;

#if defined(SIPC_DEBUG_SBUF_RDWT_OWNER)
	struct name_node *node = NULL;
#endif

	if (!status_info || size < 0 || dst >= SIPC_ID_NR)
		return;
	len = strlen(status_info);

	for (i = 0;  i < SMSG_VALID_CH_NR; i++) {
		sbuf = sbufs[dst][i];
		if (!sbuf)
			continue;

		for (n = 0;  n < sbuf->ringnr && len < size; n++) {
			ring = &sbuf->rings[n];
			ringhd = ring->header;

			if ((ringhd->rxbuf_wrptr - ringhd->rxbuf_rdptr)
					< ringhd->rxbuf_size)
				continue;

			snprintf(status_info + len,
				 size - len,
				 "ch-%d-ring-%d is full,\n",
				 sbuf->channel,
				 n);
			len = strlen(status_info);

			/*  show all rxwait task */
			spin_lock_irqsave(&ring->rxwait.lock, flags);
			cnt = 0;
			list_for_each_entry(pos,
					    &ring->rxwait.head,
					    entry) {
				task = sbuf_wait_get_task(pos, &b_select);
				if (!task)
					continue;

				if (b_select)
					phead = "rxwait task";
				else
					phead = "select task";

				snprintf(
					 status_info + len,
					 size - len,
					 "%s %d: %s, state=0x%lx, pid=%d\n",
					 phead,
					 cnt, task->comm,
					 task->state, task->pid);
				cnt++;
				len = strlen(status_info);
			}
			spin_unlock_irqrestore(&ring->rxwait.lock, flags);

			/* only show the latest ever read task */
#if defined(SIPC_DEBUG_SBUF_RDWT_OWNER)
			spin_lock_irqsave(&ring->rxwait.lock, flags);
			list_for_each_entry(node, &ring->rx_list, list) {
				if (node->latest) {
					snprintf(
						 status_info + len,
						 size - len,
						 "read task: %s, pid = %d\n",
						 node->comm,
						 node->pid);
					break;
				}
			}
			spin_unlock_irqrestore(&ring->rxwait.lock, flags);
#endif
		}
	}
}
EXPORT_SYMBOL_GPL(sbuf_get_status);

#if defined(CONFIG_DEBUG_FS)
static void sbuf_debug_task_show(struct seq_file *m,
				 struct sbuf_mgr *sbuf, int task_type)
{
	int n, cnt;
	u32 b_select;
	unsigned long flags;
	struct sbuf_ring *ring = NULL;
	wait_queue_head_t *phead;
	char *buf;
	wait_queue_entry_t *pos;
	struct task_struct *task;

	for (n = 0;  n < sbuf->ringnr;	n++) {
		ring = &sbuf->rings[n];
		cnt = 0;

		if (task_type == TASK_RXWAIT) {
			phead = &ring->rxwait;
			buf = "rxwait task";
		} else if (task_type == TASK_TXWAIT) {
			phead = &ring->txwait;
			buf = "txwait task";
		} else {
			phead = &ring->rxwait;
			buf = "select task";
		}

		spin_lock_irqsave(&phead->lock, flags);
		list_for_each_entry(pos, &phead->head, entry) {
			task = sbuf_wait_get_task(pos, &b_select);
			if (!task)
				continue;

			if (b_select && (task_type != TASK_SELECT))
				continue;

			seq_printf(m, "  ring[%2d]: %s %d ",
				   n,
				   buf,
				   cnt);
			seq_printf(m, ": %s, state = 0x%lx, pid = %d\n",
				   task->comm,
				   task->state,
				   task->pid);
			cnt++;
		}
		spin_unlock_irqrestore(
			&phead->lock,
			flags);
	}
}

#if defined(SIPC_DEBUG_SBUF_RDWT_OWNER)
static void sbuf_debug_list_show(struct seq_file *m,
				 struct sbuf_mgr *sbuf, int b_rx)
{
	int n, cnt;
	struct sbuf_ring *ring = NULL;
	struct list_head *plist;
	char *buf;
	struct name_node *node = NULL;
	unsigned long flags;

	/* list all  sbuf task list */
	for (n = 0;  n < sbuf->ringnr;	n++) {
		ring = &sbuf->rings[n];
		cnt = 0;

		if (b_rx) {
			plist = &ring->rx_list;
			buf = "read task";
		} else {
			plist = &ring->tx_list;
			buf = "write task";
		}

		spin_lock_irqsave(&ring->rxwait.lock, flags);
		list_for_each_entry(node, plist, list) {
			seq_printf(m, "  ring[%2d]: %s %d : %s, pid = %d, latest = %d\n",
				   n,
				   buf,
				   cnt,
				   node->comm,
				   node->pid,
				   node->latest);
			cnt++;
		}
		spin_unlock_irqrestore(&ring->rxwait.lock, flags);
	}
}
#endif

static int sbuf_debug_show(struct seq_file *m, void *private)
{
	struct sbuf_mgr *sbuf = NULL;
	struct sbuf_ring *ring = NULL;
	VOLA_SBUF_RING *ringhd = NULL;
	int i, j, n, cnt;
	struct smsg_ipc *smsg_sipc = NULL;

	for (i = 0; i < SIPC_ID_NR; i++) {
		smsg_sipc = smsg_ipcs[i];
		if (!smsg_sipc)
			continue;
		sipc_debug_putline(m, '*', 120);
		seq_printf(m, "dst: 0x%0x, sipc: %s:\n", i, smsg_sipc->name);
		sipc_debug_putline(m, '*', 120);

		for (j = 0;  j < SMSG_VALID_CH_NR; j++) {
			sbuf = sbufs[i][j];
			if (!sbuf)
				continue;
			/* list a sbuf channel */
			sipc_debug_putline(m, '-', 100);
			seq_printf(m, "sbuf_%d_%03d, state: %d, ",
				   sbuf->dst,
				   sbuf->channel,
				   sbuf->state);
			seq_printf(m, "virt: 0x%lx, phy: 0x%0x, map: 0x%x",
				   (unsigned long)sbuf->smem_virt,
				   sbuf->smem_addr,
				   sbuf->mapped_smem_addr);
			seq_printf(m, " size: 0x%0x, ringnr: %d\n",
				   sbuf->smem_size,
				   sbuf->ringnr);
			sipc_debug_putline(m, '-', 100);

			/* list all  sbuf ring info list in a chanel */
			sipc_debug_putline(m, '-', 80);
			seq_puts(m, "  1. all sbuf ring info list:\n");
			for (n = 0;  n < sbuf->ringnr;  n++) {
				ring = &sbuf->rings[n];
				ringhd = ring->header;
				if (ringhd->txbuf_wrptr == 0 &&
				    ringhd->rxbuf_wrptr == 0)
					continue;

				seq_printf(m, "  rx ring[%2d]: addr: 0x%0x, ",
					   n, ringhd->rxbuf_addr);
				seq_printf(m, "rp: 0x%0x, wp: 0x%0x, size: 0x%0x\n",
					   ringhd->rxbuf_rdptr,
					   ringhd->rxbuf_wrptr,
					   ringhd->rxbuf_size);

				seq_printf(m, "  tx ring[%2d]: addr: 0x%0x, ",
					   n, ringhd->txbuf_addr);
				seq_printf(m, "rp: 0x%0x, wp: 0x%0x, size: 0x%0x\n",
					   ringhd->txbuf_rdptr,
					   ringhd->txbuf_wrptr,
					   ringhd->txbuf_size);
			}

			/* list all sbuf rxwait/txwait in a chanel */;
			sipc_debug_putline(m, '-', 80);
			seq_puts(m, "  2. all waittask list:\n");
			sbuf_debug_task_show(m, sbuf, TASK_RXWAIT);
			sbuf_debug_task_show(m, sbuf, TASK_TXWAIT);
			sbuf_debug_task_show(m, sbuf, TASK_SELECT);

#ifdef SIPC_DEBUG_SBUF_RDWT_OWNER
			/* list all sbuf ever read task list in a chanel */;
			sipc_debug_putline(m, '-', 80);
			seq_puts(m, "  3. all ever rdwt list:\n");
			sbuf_debug_list_show(m, sbuf, 1);
			sbuf_debug_list_show(m, sbuf, 0);
#endif

			/* list all  rx full ring list in a chanel */
			cnt = 0;
			for (n = 0;  n < sbuf->ringnr;	n++) {
				ring = &sbuf->rings[n];
				ringhd = ring->header;
				if ((ringhd->rxbuf_wrptr - ringhd->rxbuf_rdptr)
						== ringhd->rxbuf_size) {
					if (cnt == 0) {
						sipc_debug_putline(m, '-', 80);
						seq_puts(m, "  x. all rx full ring list:\n");
					}
					cnt++;
					seq_printf(m, "  ring[%2d]\n", n);
				}
			}
		}
	}

	return 0;
}

static int sbuf_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sbuf_debug_show, inode->i_private);
}

static const struct file_operations sbuf_debug_fops = {
	.open = sbuf_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int sbuf_init_debugfs(void *root)
{
	if (!root)
		return -ENXIO;
	debugfs_create_file("sbuf", S_IRUGO,
			    (struct dentry *)root,
			    NULL, &sbuf_debug_fops);
	return 0;
}
EXPORT_SYMBOL_GPL(sbuf_init_debugfs);

#endif /* CONFIG_DEBUG_FS */

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SBUF driver");
MODULE_LICENSE("GPL");
