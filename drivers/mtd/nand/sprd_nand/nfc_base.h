
#include <linux/mtd/flashchip.h>

#define NFC_MAX_ID_LEN 8
#define NFC_SPL_MAX_SIZE (64 << 10)

struct sprd_nfc_base {
	/* resource0:These information is initialize by nfc base self; */
	spinlock_t lock;
	flstate_t state;
	wait_queue_head_t wq;
	struct device *dev;
	struct task_struct *holder;

	u32 page_mask;
	u32 blk_mask;
	u32 page_per_buf;
	u32 page_per_mtd;
	u32 *bbt;

	struct dentry *debugfs_root;
	u32 allow_erase_badblock;

	/* resource1:This information must be give by user */
	u8 id[NFC_MAX_ID_LEN];
	u32 ecc_mode;
	u32 risk_threshold;
	u32 eccbytes;
	u32 oobavail;
	u32 obb_size;
	u32 pageshift;
	u32 blkshift;
	u32 chip_shift;
	u32 chip_num;
	u32 bufshift;

	u8 *mbuf_v;
	u8 *sbuf_v;

	int (*ctrl_en)(struct sprd_nfc_base *ctrl, u32 en);
	void (*ctrl_suspend)(struct sprd_nfc_base *ctrl);
	int (*ctrl_resume)(struct sprd_nfc_base *ctrl);

	int (*read_page_in_blk)(struct sprd_nfc_base *ctrl, u32 page,
				u32 num, u32 *ret_num,
				u32 if_has_mbuf, u32 if_has_sbuf,
				u32 mode, struct mtd_ecc_stats *ecc_sts);

	int (*write_page_in_blk)(struct sprd_nfc_base *ctrl, u32 page,
				 u32 num, u32 *ret_num,
				 u32 if_has_mbuf, u32 if_has_sbuf,
				 u32 mode);

	int (*nand_erase_blk)(struct sprd_nfc_base *ctrl, u32 page);
	int (*nand_is_bad_blk)(struct sprd_nfc_base *ctrl, u32 page);
	int (*nand_mark_bad_blk)(struct sprd_nfc_base *ctrl, u32 page);

	int (*debugfs_drv_show)(struct sprd_nfc_base *ctrl, struct seq_file *s,
				void *data);

	void *priv;
};

int sprd_nfc_base_register(struct sprd_nfc_base *ctrl);
