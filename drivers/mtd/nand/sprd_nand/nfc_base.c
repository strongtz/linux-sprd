/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * host software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * host program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/slab.h>

#include "nfc_base.h"

const char *part_probes[] = {"cmdlinepart", NULL};

static int sprd_nfc_init_device(struct sprd_nfc_base *ctrl)
{
	spin_lock_init(&ctrl->lock);
	ctrl->state = FL_READY;
	init_waitqueue_head(&ctrl->wq);

	return 0;
}

static int sprd_nfc_get_device(struct sprd_nfc_base *ctrl, int new_state)
{
	spinlock_t *lock = &ctrl->lock;
	wait_queue_head_t *wq = &ctrl->wq;
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(wq, &wait);
	while (1) {
		set_current_state(TASK_UNINTERRUPTIBLE);

		spin_lock(lock);
		if (ctrl->state == FL_READY) {
			ctrl->state = new_state;
			ctrl->holder = current;
			spin_unlock(lock);
			break;
		}
		spin_unlock(lock);

		io_schedule();
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(wq, &wait);
	if (new_state != FL_PM_SUSPENDED)
		ctrl->ctrl_en(ctrl, 1);

	return 0;
}

static void sprd_nfc_put_device(struct sprd_nfc_base *ctrl,
				char *call_name, int ret)
{
	if (ctrl->state != FL_PM_SUSPENDED)
		ctrl->ctrl_en(ctrl, 0);
	/*  Release the ctrl and the chip  */
	spin_lock(&ctrl->lock);
	ctrl->state = FL_READY;
	ctrl->holder = NULL;
	spin_unlock(&ctrl->lock);
	wake_up(&ctrl->wq);
	if (ret)
		pr_err("nfc base: %s ret code %d", call_name, ret);
}

static void init_param(struct sprd_nfc_base *ctrl)
{
	ctrl->page_mask = (1 << ctrl->pageshift) - 1;
	ctrl->blk_mask = (1 << ctrl->blkshift) - 1;
	ctrl->page_per_buf = (1 << (ctrl->bufshift - ctrl->pageshift));
	ctrl->page_per_mtd = (1 << (ctrl->chip_shift - ctrl->pageshift)) *
			      ctrl->chip_num;
}

/* because the *bbt[] type is u32,so shift >> 5 */
static inline void set_bbt(struct sprd_nfc_base *ctrl, u32 shift)
{
	ctrl->bbt[(shift) >> 5] |= (1 << ((shift) - (((shift) >> 5) << 5)));
}

static inline bool is_bbt(struct sprd_nfc_base *ctrl, u32 shift)
{
	return !!(ctrl->bbt[(shift) >> 5] &
		  (1 << ((shift) - (((shift) >> 5) << 5))));
}

static int init_bbt(struct sprd_nfc_base *ctrl)
{
	u32 blk, cnt, bbt_size;
	int ret;

	cnt = (1 << (ctrl->chip_shift - ctrl->blkshift)) * ctrl->chip_num;
	bbt_size = (cnt >> 5) << 2;
	ctrl->bbt = kmalloc(bbt_size, GFP_KERNEL);
	memset(ctrl->bbt, 0, bbt_size);
	for (blk = 0; blk < cnt; blk++) {
		ret = ctrl->nand_is_bad_blk(ctrl, blk << (ctrl->blkshift -
							  ctrl->pageshift));
		if (ret < 0) {
			dev_err(ctrl->dev, "nand:init bbt err..\n");
			kfree(ctrl->bbt);
			ctrl->bbt = 0;
			return ret;
		} else if (ret) {
			set_bbt(ctrl, blk);
			dev_info(ctrl->dev,
				 "nand:blk %d, blkpage %d is bad block.\n", blk,
				 blk << (ctrl->blkshift - ctrl->pageshift));
		}
	}

	return ret;
}

static int nand_read_oob(struct mtd_info *mtd, struct sprd_nfc_base *ctrl,
			 loff_t from, struct mtd_oob_ops *ops,
			 struct mtd_ecc_stats *ecc_stats)
{
	int ret = 0, tmpret = 0;
	u32 page_s, page_e, page_num, page_per_buf;
	u32 remain_page_inbuf, ret_num, i;
	struct mtd_ecc_stats stats;
	u32 read_len;
	u32 obb_read_len;
	u32 max_obb_read_len;
	u8 *mbuf, *obb_buf, *sbuf_v;
	u32 toread_m, toread_s, col;

	stats = mtd->ecc_stats;
	ops->retlen = 0;
	ops->oobretlen = 0;

	read_len = ops->len;
	obb_read_len = ops->ooblen;
	if (ops->mode == MTD_OPS_AUTO_OOB)
		max_obb_read_len = ctrl->oobavail;
	else
		max_obb_read_len = ctrl->obb_size;

	mbuf = ops->datbuf;
	obb_buf = ops->oobbuf;
	col = from & ctrl->page_mask;
	page_s = from >> ctrl->pageshift;

	if (from < NFC_SPL_MAX_SIZE)
		page_per_buf = 1;
	else
		page_per_buf = ctrl->page_per_buf;

	if (mbuf) {
		if (!read_len)
			return 0;

		if ((obb_buf) && max_obb_read_len <= ops->ooboffs)
			return -EINVAL;

		page_e = ((from + read_len - 1) >> ctrl->pageshift);
		page_num = page_e - page_s + 1;
	} else if (obb_buf) {
		if (((max_obb_read_len) &&
		     max_obb_read_len <= ops->ooboffs) ||
		    (!max_obb_read_len && ops->ooboffs > 0)) {
			return -EINVAL;
		}
		if (!obb_read_len)
			return 0;

		page_num = obb_read_len / (max_obb_read_len - ops->ooboffs);
		if (obb_read_len % (max_obb_read_len - ops->ooboffs))
			page_num++;

	} else {
		return -EINVAL;
	}

	/* check param */
	if ((page_s + page_num) > ctrl->page_per_mtd)
		return -EINVAL;

	remain_page_inbuf = (page_per_buf - (page_s & (page_per_buf - 1)));
	if (remain_page_inbuf >= page_num) {
		tmpret = ctrl->read_page_in_blk(ctrl, page_s,
						page_num, &ret_num,
						(u32)(!!ops->datbuf),
						(u32)(!!ops->oobbuf),
						ops->mode, ecc_stats);
		if (tmpret == -EUCLEAN)
			ret = -EUCLEAN;
		else if (tmpret < 0)
			return tmpret;
		if (mbuf) {
			memcpy(mbuf, ctrl->mbuf_v + col, read_len);
			ops->retlen = read_len;
		}
		if (obb_buf) {
			sbuf_v = ctrl->sbuf_v;
			for (i = 0; i < page_num; i++) {
				toread_s = min(max_obb_read_len - ops->ooboffs,
					       obb_read_len);
				memcpy(obb_buf, sbuf_v + ops->ooboffs,
				       toread_s);
				obb_buf += toread_s;
				obb_read_len -= toread_s;
				sbuf_v += max_obb_read_len;
			}
			ops->oobretlen = ops->ooblen - obb_read_len;
		}
		return ret;
	}
	tmpret = ctrl->read_page_in_blk(ctrl, page_s, remain_page_inbuf,
					&ret_num, (u32)(!!ops->datbuf),
					(u32)(!!ops->oobbuf),
					ops->mode, ecc_stats);
	if (tmpret == -EUCLEAN)
		ret = -EUCLEAN;
	else if (tmpret < 0)
		return tmpret;
	if (mbuf) {
		toread_m = ((remain_page_inbuf << ctrl->pageshift) - col);
		memcpy(mbuf, ctrl->mbuf_v + col, toread_m);
		mbuf += toread_m;
		read_len -= toread_m;
		col = 0;
		ops->retlen += toread_m;
	}
	if (obb_buf) {
		sbuf_v = ctrl->sbuf_v;
		for (i = 0; i < remain_page_inbuf; i++) {
			toread_s = min(max_obb_read_len - ops->ooboffs,
				       obb_read_len);
			memcpy(obb_buf, sbuf_v + ops->ooboffs, toread_s);
			obb_buf += toread_s;
			sbuf_v += max_obb_read_len;
			obb_read_len -= toread_s;
		}
		ops->oobretlen = ops->ooblen - obb_read_len;
	}
	page_s += remain_page_inbuf;
	page_num -= remain_page_inbuf;

	while (page_num >= page_per_buf) {
		tmpret = ctrl->read_page_in_blk(ctrl, page_s, page_per_buf,
						&ret_num,
						(u32)(!!ops->datbuf),
						(u32)(!!ops->oobbuf),
						ops->mode, ecc_stats);
		if (tmpret == -EUCLEAN)
			ret = -EUCLEAN;
		else if (tmpret < 0)
			return tmpret;
		if (mbuf) {
			toread_m = min(read_len,
				       (page_per_buf << ctrl->pageshift));
			memcpy(mbuf, ctrl->mbuf_v, toread_m);
			mbuf += toread_m;
			read_len -= toread_m;
			ops->retlen += toread_m;
		}
		if (obb_buf) {
			sbuf_v = ctrl->sbuf_v;
			for (i = 0; i < page_per_buf; i++) {
				toread_s = min(max_obb_read_len - ops->ooboffs,
					       obb_read_len);
				memcpy(obb_buf, sbuf_v + ops->ooboffs,
				       toread_s);
				obb_buf += toread_s;
				sbuf_v += max_obb_read_len;
				obb_read_len -= toread_s;
			}
			ops->oobretlen = ops->ooblen - obb_read_len;
		}
		page_s += page_per_buf;
		page_num -= page_per_buf;
	}
	if (page_num) {
		tmpret = ctrl->read_page_in_blk(ctrl, page_s, page_num,
						&ret_num,
						(u32)(!!ops->datbuf),
						(u32)(!!ops->oobbuf),
						ops->mode, ecc_stats);
		if (tmpret == -EUCLEAN)
			ret = -EUCLEAN;
		else if (tmpret < 0)
			return tmpret;
		if (mbuf) {
			toread_m = read_len;
			memcpy(mbuf, ctrl->mbuf_v, toread_m);
			mbuf += toread_m;
			read_len -= toread_m;
			ops->retlen += toread_m;
		}
		if (obb_buf) {
			sbuf_v = ctrl->sbuf_v;
			for (i = 0; i < page_num; i++) {
				toread_s = min(max_obb_read_len - ops->ooboffs,
					       obb_read_len);
				memcpy(obb_buf, sbuf_v + ops->ooboffs,
				       toread_s);
				obb_buf += toread_s;
				sbuf_v += max_obb_read_len;
				obb_read_len -= toread_s;
			}
			ops->oobretlen = ops->ooblen - obb_read_len;
		}
	}
	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return ret;
}

static int nand_write_oob(struct sprd_nfc_base *ctrl, loff_t to,
			  struct mtd_oob_ops *ops)
{
	int ret = 0;
	u32 page_s, page_e, page_num = 0;
	u32 page_per_buf, remain_page_inbuf, ret_num, i;

	u32 read_len;
	u32 obb_read_len;
	u32 max_obb_read_len;
	u8 *mbuf, *obb_buf, *sbuf_v;
	u32 toread_m = 0, toread_s, col;

	ops->retlen = 0;
	ops->oobretlen = 0;
	read_len = ops->len;
	obb_read_len = ops->ooblen;
	if (ops->mode == MTD_OPS_AUTO_OOB)
		max_obb_read_len = ctrl->oobavail;
	else
		max_obb_read_len = ctrl->obb_size;

	mbuf = ops->datbuf;
	obb_buf = ops->oobbuf;
	sbuf_v = ctrl->sbuf_v;
	col = to & ctrl->page_mask;
	page_s = to >> ctrl->pageshift;

	/* check whether write spl image */
	if (to < NFC_SPL_MAX_SIZE)
		page_per_buf = 1;
	else
		page_per_buf = ctrl->page_per_buf;

	if (mbuf) {
		if (!read_len)
			return 0;

		if ((obb_buf) && max_obb_read_len <= ops->ooboffs)
			return -EINVAL;

		page_e = ((to + read_len - 1) >> ctrl->pageshift);
		page_num = page_e - page_s + 1;
	} else if (obb_buf) {
		if (((max_obb_read_len) &&
		     max_obb_read_len <= ops->ooboffs) ||
		    (!max_obb_read_len && ops->ooboffs > 0) ||
		    (max_obb_read_len < (ops->ooboffs + obb_read_len)))
			return -EINVAL;

		if (!obb_read_len)
			return 0;

		page_num = obb_read_len / (max_obb_read_len - ops->ooboffs);
		if (obb_read_len % (max_obb_read_len - ops->ooboffs))
			page_num++;
		else
			return -EINVAL;
	}
	if ((page_s + page_num) > ctrl->page_per_mtd)
		return -EINVAL;

	if (!mbuf && ops->mode != MTD_OPS_RAW)
		memset(ctrl->mbuf_v, 0xFF, (min(page_per_buf, page_num))
		       << ctrl->pageshift);

	if (!obb_buf && ops->mode != MTD_OPS_RAW)
		memset(ctrl->sbuf_v, 0xFF,
		       (min(page_per_buf, page_num)) * ctrl->obb_size);

	remain_page_inbuf = (page_per_buf - (page_s & (page_per_buf - 1)));
	if (remain_page_inbuf >= page_num) {
		if (mbuf) {
			memset(ctrl->mbuf_v, 0xFF, col);
			memcpy(ctrl->mbuf_v + col, mbuf, read_len);
			if ((col + read_len) & ctrl->page_mask) {
				memset(ctrl->mbuf_v + col + read_len, 0xFF,
				       ((1 << ctrl->pageshift) -
					((col + read_len) & ctrl->page_mask)));
			}
		}
		if (obb_buf) {
			sbuf_v = ctrl->sbuf_v;
			for (i = 0; i < page_num; i++) {
				toread_s = min(max_obb_read_len - ops->ooboffs,
					       obb_read_len);
				memset(sbuf_v, 0xFF, ops->ooboffs);
				memcpy(sbuf_v + ops->ooboffs,
				       obb_buf, toread_s);
				memset(sbuf_v + ops->ooboffs + toread_s,
				       0xFF, max_obb_read_len -
				       ops->ooboffs - toread_s);
				obb_buf += toread_s;
				obb_read_len -= toread_s;
				sbuf_v += max_obb_read_len;
			}
		}
		ret = ctrl->write_page_in_blk(ctrl, page_s, page_num, &ret_num,
					      (u32)(!!ops->datbuf),
					      (u32)(!!ops->oobbuf),
					      ops->mode);
		if (ret == 0) {
			if (ops->datbuf)
				ops->retlen = read_len;

			if (ops->oobbuf)
				ops->oobretlen = ops->ooblen - obb_read_len;
		}
		return ret;
	}
	if (mbuf) {
		toread_m = ((remain_page_inbuf << ctrl->pageshift) - col);
		memset(ctrl->mbuf_v, 0xFF, col);
		memcpy(ctrl->mbuf_v + col, mbuf, toread_m);
		mbuf += toread_m;
		read_len -= toread_m;
		col = 0;
	}
	if (obb_buf) {
		sbuf_v = ctrl->sbuf_v;
		for (i = 0; i < remain_page_inbuf; i++) {
			toread_s = min(max_obb_read_len - ops->ooboffs,
				       obb_read_len);
			memset(sbuf_v, 0xFF, ops->ooboffs);
			memcpy(sbuf_v + ops->ooboffs, obb_buf, toread_s);
			memset(sbuf_v + ops->ooboffs + toread_s, 0xFF,
			       max_obb_read_len - ops->ooboffs - toread_s);
			obb_buf += toread_s;
			sbuf_v += max_obb_read_len;
			obb_read_len -= toread_s;
		}
	}
	ret = ctrl->write_page_in_blk(ctrl, page_s, remain_page_inbuf, &ret_num,
				      (u32)(!!ops->datbuf),
				      (u32)(!!ops->oobbuf), ops->mode);
	if (ret < 0)
		return ret;

	if (ret == 0) {
		if (ops->datbuf)
			ops->retlen += toread_m;

		if (ops->oobbuf)
			ops->oobretlen = ops->ooblen - obb_read_len;
	}

	page_s += remain_page_inbuf;
	page_num -= remain_page_inbuf;

	while (page_num >= page_per_buf) {
		if (mbuf) {
			toread_m = min(read_len,
				       (page_per_buf << ctrl->pageshift));
			memcpy(ctrl->mbuf_v, mbuf, toread_m);
			mbuf += toread_m;
			read_len -= toread_m;
		}
		if (obb_buf) {
			sbuf_v = ctrl->sbuf_v;
			for (i = 0; i < page_per_buf; i++) {
				toread_s = min(max_obb_read_len - ops->ooboffs,
					       obb_read_len);
				memset(sbuf_v, 0xFF, ops->ooboffs);
				memcpy(sbuf_v + ops->ooboffs, obb_buf,
				       toread_s);
				memset(sbuf_v + ops->ooboffs + toread_s,
				       0xFF, max_obb_read_len -
				       ops->ooboffs - toread_s);
				obb_buf += toread_s;
				sbuf_v += max_obb_read_len;
				obb_read_len -= toread_s;
			}
		}
		ret = ctrl->write_page_in_blk(ctrl, page_s,
					      page_per_buf, &ret_num,
					      (u32)(!!ops->datbuf),
					      (u32)(!!ops->oobbuf),
					      ops->mode);
		if (ret < 0)
			return ret;

		if (ret == 0) {
			if (ops->datbuf)
				ops->retlen += toread_m;

			if (ops->oobbuf)
				ops->oobretlen = ops->ooblen - obb_read_len;
		}
		page_s += page_per_buf;
		page_num -= page_per_buf;
	}
	if (page_num) {
		if (mbuf) {
			toread_m = read_len;
			memcpy(ctrl->mbuf_v, mbuf, toread_m);
			if (toread_m & ctrl->page_mask) {
				memset(ctrl->mbuf_v + toread_m, 0xFF,
				       ((1 << ctrl->pageshift) -
					(toread_m & ctrl->page_mask)));
			}
			mbuf += toread_m;
			read_len -= toread_m;
		}
		if (obb_buf) {
			sbuf_v = ctrl->sbuf_v;
			for (i = 0; i < page_num; i++) {
				toread_s = min(max_obb_read_len - ops->ooboffs,
					       obb_read_len);
				memset(sbuf_v, 0xFF, ops->ooboffs);
				memcpy(sbuf_v + ops->ooboffs, obb_buf,
				       toread_s);
				memset(sbuf_v + ops->ooboffs + toread_s,
				       0xFF, max_obb_read_len -
				       ops->ooboffs - toread_s);
				obb_buf += toread_s;
				sbuf_v += max_obb_read_len;
				obb_read_len -= toread_s;
			}
		}
		ret = ctrl->write_page_in_blk(ctrl, page_s, page_num, &ret_num,
					      (u32)(!!ops->datbuf),
					      (u32)(!!ops->oobbuf),
					      ops->mode);
		if (ret < 0)
			return ret;

		if (ret == 0) {
			if (ops->datbuf)
				ops->retlen += toread_m;

			if (ops->oobbuf)
				ops->oobretlen = ops->ooblen - obb_read_len;
		}
	}

	return ret;
}

static int callback_block_isbad(struct mtd_info *mtd, loff_t ofs)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;
	int ret;

	spin_lock(&ctrl->lock);
	ret = is_bbt(ctrl, ofs >> ctrl->blkshift);
	spin_unlock(&ctrl->lock);

	return ret;
}

static int callback_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;
	int ret = 0;
	u32 page;

	spin_lock(&ctrl->lock);
	if (is_bbt(ctrl, ofs >> ctrl->blkshift))
		goto exit;

	page = ((ofs >> ctrl->blkshift) << (ctrl->blkshift - ctrl->pageshift));
	spin_unlock(&ctrl->lock);

	sprd_nfc_get_device(ctrl, FL_ERASING);
	ret = ctrl->nand_erase_blk(ctrl, page);
	sprd_nfc_put_device(ctrl, __FILE__, ret);

	sprd_nfc_get_device(ctrl, FL_WRITING);
	ret = ctrl->nand_mark_bad_blk(ctrl, page);
	sprd_nfc_put_device(ctrl, __FILE__, ret);

	spin_lock(&ctrl->lock);
	set_bbt(ctrl, ofs >> ctrl->blkshift);
exit:

	spin_unlock(&ctrl->lock);
	return ret;
}

static int callback_read(struct mtd_info *mtd, loff_t from, size_t len,
			 size_t *retlen, u_char *buf)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;
	int ret;
	struct mtd_oob_ops ops = {};

	ops.mode = MTD_OPS_PLACE_OOB;
	ops.len = len;
	ops.datbuf = (u8 *)buf;
	ops.oobbuf = NULL;
	sprd_nfc_get_device(ctrl, FL_READING);
	ret = nand_read_oob(mtd, ctrl, from, &ops, &mtd->ecc_stats);
	sprd_nfc_put_device(ctrl, __FILE__, ret);
	*retlen = ops.retlen;

	return ret;
}

static int callback_write(struct mtd_info *mtd, loff_t to, size_t len,
			  size_t *retlen, const u_char *buf)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;
	int ret;
	struct mtd_oob_ops ops;

	memset(&ops, 0, sizeof(struct mtd_oob_ops));
	ops.mode = MTD_OPS_PLACE_OOB;
	ops.len = len;
	ops.datbuf = (u8 *)buf;
	ops.oobbuf = NULL;
	sprd_nfc_get_device(ctrl, FL_WRITING);
	ret = nand_write_oob(ctrl, to, &ops);
	sprd_nfc_put_device(ctrl, __FILE__, ret);
	*retlen = ops.retlen;

	return ret;
}

static int callback_read_oob(struct mtd_info *mtd, loff_t from,
			     struct mtd_oob_ops *ops)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;
	int ret;

	sprd_nfc_get_device(ctrl, FL_READING);
	ret = nand_read_oob(mtd, ctrl, from, ops, &mtd->ecc_stats);
	sprd_nfc_put_device(ctrl, __FILE__, ret);

	return ret;
}

static int callback_write_oob(struct mtd_info *mtd, loff_t to,
			      struct mtd_oob_ops *ops)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;
	int ret;

	sprd_nfc_get_device(ctrl, FL_WRITING);
	ret = nand_write_oob(ctrl, to, ops);
	sprd_nfc_put_device(ctrl, __FILE__, ret);

	return ret;
}

static int callback_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;
	int ret;
	u32 blk_s, blk_e, i;

	if ((instr->addr & ctrl->blk_mask) || (instr->len & ctrl->blk_mask)) {
		/* check whether addr and len is block align */
		return -EINVAL;
	}

	sprd_nfc_get_device(ctrl, FL_ERASING);
	do {
		blk_s = instr->addr >> ctrl->blkshift;
		blk_e = (instr->addr + instr->len - 1) >> ctrl->blkshift;

		instr->state = MTD_ERASING;
		for (i = blk_s; i <= blk_e; i++) {
			spin_lock(&ctrl->lock);
			if (is_bbt(ctrl, i) && !ctrl->allow_erase_badblock) {
				spin_unlock(&ctrl->lock);
				instr->state = MTD_ERASE_FAILED;
				break;
			}
			spin_unlock(&ctrl->lock);
			if (ctrl->nand_erase_blk(ctrl, i <<
						 (ctrl->blkshift -
						  ctrl->pageshift))) {
				instr->fail_addr = (i << ctrl->blkshift);
				instr->state = MTD_ERASE_FAILED;
				break;
			}
		}
		if (instr->state == MTD_ERASING)
			instr->state = MTD_ERASE_DONE;

	} while (0);
	ret = (instr->state == MTD_ERASE_DONE ? 0 : -EIO);
	sprd_nfc_put_device(ctrl, __FILE__, ret);

	if (!ret)
		mtd_erase_callback(instr);

	return ret;
}

static void callback_sync(struct mtd_info *mtd)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;

	sprd_nfc_get_device(ctrl, FL_SYNCING);
	sprd_nfc_put_device(ctrl, __FILE__, 0);
}

static int callback_suspend(struct mtd_info *mtd)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;

	sprd_nfc_get_device(ctrl, FL_PM_SUSPENDED);
	ctrl->ctrl_suspend(ctrl);
	sprd_nfc_put_device(ctrl, __FILE__, 0);

	return 0;
}

static void callback_resume(struct mtd_info *mtd)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;

	sprd_nfc_get_device(ctrl, FL_PM_SUSPENDED);
	ctrl->ctrl_resume(ctrl);
	sprd_nfc_put_device(ctrl, __FILE__, 0);
}

static int sprd_nfc_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *oob_region)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;

	if (section)
		return -EINVAL;

	oob_region->length = ctrl->oobavail;
	oob_region->offset = 2;

	return 0;
}

static int sprd_nfc_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *oob_region)
{
	struct sprd_nfc_base *ctrl = (struct sprd_nfc_base *)mtd->priv;

	if (section)
		return -EINVAL;

	oob_region->length = ctrl->eccbytes;
	oob_region->offset = mtd->oobsize - oob_region->length;

	return 0;
}

static const struct mtd_ooblayout_ops sprd_nfc_ooblayout_ops = {
	.free = sprd_nfc_ooblayout_free,
	.ecc = sprd_nfc_ooblayout_ecc,
};

static void init_mtd(struct mtd_info *mtd, struct sprd_nfc_base *ctrl)
{
	mtd->dev.parent = ctrl->dev;
	mtd->priv = ctrl;
	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;
	mtd->size = (1 << ctrl->chip_shift) * ctrl->chip_num;
	mtd->erasesize = (1 << ctrl->blkshift);
	mtd->writesize = (1 << ctrl->pageshift);
	mtd->writebufsize = (1 << ctrl->bufshift);
	mtd->oobsize = ctrl->obb_size;
	mtd->oobavail = ctrl->oobavail;

	mtd->bitflip_threshold = ctrl->risk_threshold;
	mtd->name = "sprd-nand";

	mtd_set_ooblayout(mtd, &sprd_nfc_ooblayout_ops);
	mtd->ecc_strength = ctrl->ecc_mode;

	mtd->_erase = callback_erase;
	mtd->_read = callback_read;
	mtd->_write = callback_write;
	mtd->_read_oob = callback_read_oob;
	mtd->_write_oob = callback_write_oob;
	mtd->_sync = callback_sync;
	mtd->_block_isbad = callback_block_isbad;
	mtd->_block_markbad = callback_block_markbad;
	mtd->_suspend = callback_suspend;
	mtd->_resume = callback_resume;

	mtd->owner = THIS_MODULE;
}

int sprd_nfc_base_register(struct sprd_nfc_base *ctrl)
{
	struct mtd_info *sprd_mtd;

	sprd_mtd = devm_kzalloc(ctrl->dev, sizeof(*sprd_mtd), GFP_KERNEL);
	if (!sprd_mtd)
		return -ENOMEM;

	sprd_nfc_init_device(ctrl);
	init_param(ctrl);
	init_bbt(ctrl);
	init_mtd(sprd_mtd, ctrl);

	return mtd_device_parse_register(sprd_mtd, part_probes, 0, 0, 0);
}
