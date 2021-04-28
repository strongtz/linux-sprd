/* sprd_mailbox_drv.h */

#ifndef SPRD_MAILBOX_DRV_H
#define SPRD_MAILBOX_DRV_H

#include <linux/sprd_mailbox.h>

#define SEND_FIFO_LEN 64

#define MBOX_INVALID_CORE	0xff

struct mbox_dts_cfg_tag {
	struct regmap *gpr;
	u32 enable_reg;
	u32 mask_bit;
	struct resource inboxres;
	struct resource outboxres;
	u32 inbox_irq;
	u32 outbox_irq;
	u32 outbox_sensor_irq;
	u32 sensor_core;
	u32 core_cnt;
	u32 version;
};

struct mbox_cfg_tag {
	u32 inbox_irq;
	u32 inbox_base;
	u32 inbox_range;
	u32 inbox_fifo_size;
	u32 inbox_irq_mask;

	u32 outbox_irq;
	u32 outbox_sensor_irq;
	u32 sensor_core;

	u32 outbox_base;
	u32 outbox_range;
	u32 outbox_fifo_size;
	u32 outbox_irq_mask;

	u32 rd_bit;
	u32 rd_mask;
	u32 wr_bit;
	u32 wr_mask;

	u32 enable_reg;
	u32 mask_bit;

	u32 core_cnt;
	u32 version;

	u32 prior_low;
	u32 prior_high;
};

struct mbox_chn_tag {
	MBOX_FUNCALL mbox_smsg_handler;
	unsigned long max_irq_proc_time;
	unsigned long max_recv_flag_cnt;
	void *mbox_priv_data;
};

struct  mbox_fifo_data_tag {
	u8 core_id;
	u64 msg;
};

struct mbox_operations_tag {
	int  (*cfg_init)(struct mbox_dts_cfg_tag *, u8 *);
	int (*phy_register_irq_handle)(u8, MBOX_FUNCALL, void *);
	int (*phy_unregister_irq_handle)(u8);
	irqreturn_t  (*src_irqhandle)(int, void *);
	irqreturn_t (*recv_irqhandle)(int, void *);
	irqreturn_t (*sensor_recv_irqhandle)(int, void *);
	int (*phy_send)(u8, u64);
	void (*process_bak_msg)(void);
	u32 (*phy_core_fifo_full)(int);
	void (*phy_just_sent)(u8, u64);
	bool (*outbox_has_irq)(void);
};

struct mbox_device_tag {
	u32 version;
	u32 max_cnt;
	const struct mbox_operations_tag *fops;
	const struct file_operations *debug_fops;
};

void mbox_start_fifo_send(u8 send_mask);
u8 mbox_get_send_fifo_mask(u8 send_bit);
#if defined(CONFIG_DEBUG_FS)
void mbox_check_all_send_fifo(struct seq_file *m);
#endif
void mbox_get_phy_device(struct mbox_device_tag **mbox_dev);

#endif /* SPRD_MAILBOX_DRV_H */
