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
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/flashchip.h>
#include <linux/of_device.h>

#include "nfc_base.h"
#include "sprd_nand.h"
#include "sprd_nand_param.h"

struct sprd_nand_param {
	u8 id[NFC_MAX_ID_LEN];
	u32 nblkcnt;
	u32 npage_per_blk;
	u32 nsect_per_page;
	u32 nsect_size;
	u32 nspare_size;

	u32 page_size; /* no contain spare */
	u32 main_size; /* page_size */
	u32 spare_size;

	u32 badflag_pos;
	u32 badflag_len;
	u32 ecc_mode;
	u32 ecc_pos;
	u32 ecc_size;
	u32 info_pos;
	u32 info_size;

	u8 nbus_width;
	u8 ncycle;

	/* ACS */
	u32 t_als;
	u32 t_cls;
	/* ACE */
	u32 t_clh;
	u32 t_alh;
	/* RWS */
	u32 t_rr;
	u32 t_adl;
	/* RWE */

	/* RWH & RWL */
	u32 t_wh;  /* we high when write */
	u32 t_wp;  /* we low  when write */
	u32 t_reh; /* re high when read */
	u32 t_rp;  /* re low when read */
};

struct nand_inst {
	u8 *program_name;
	u32 cnt;
	u32 step_tag;
	u32 int_bits;
	u32 inst[NFC_MAX_MC_INST_NUM];
};

struct sprd_nand_host {
	/* resource0: interrupt id & clock name &  register bass address */
	int irq;
	u32 frequence;
	struct device *dev;
	void __iomem *ioaddr;
	struct clk *clk_ahb_enable_gate;
	struct clk *clk_nand;
	struct clk *clk_ecc;
	struct clk *clk_parent_sdr;
	struct clk *clk_parent_ddr;
	struct clk *clk_ahb_ecc;
	struct clk *clk_ahb_26m;
	struct clk *clk_nand_1x;
	struct clk *clk_nand_2x;

	/* resource1: nand param & mtd ecclayout & risk threshold */
	struct sprd_nand_param param;
	u32 eccmode_reg;
	u32 risk_threshold;
	u32 cs[CFG0_CS_MAX];
	u32 csnum;

	/* u32 csDis; */
	u32 sect_perpage_shift;
	u32 sectshift;
	u32 sectmsk;
	u32 pageshift;
	u32 blkshift;
	u32 bufshift;
	u32 badblkcnt;
	/* resource3: local DMA buffer */
	u8 *mbuf_p;
	u8 *mbuf_v;
	u8 *sbuf_p;
	u8 *sbuf_v;
	u8 *stsbuf_p;
	u8 *stsbuf_v;
	u32 *seedbuf_p;
	u32 *seedbuf_v;
	u32 mbuf_size;
	u32 sbuf_size;
	u32 stsbuf_size;
	u32 seedbuf_size;
	/*
	 * resource4: register base value. some value is const
	 * while operate on nand flash, we store it to local valuable.
	 * Time & cfg & status mach. It is different with operation R W E.
	 */
	u32 nfc_time0_r;
	u32 nfc_time0_w;
	u32 nfc_time0_e;

	u32 nfc_start;
	u32 nfc_cfg0;
	u32 nfc_cfg1;
	u32 nfc_cfg2;
	u32 nfc_cfg3;
	u32 nfc_cfg4;

	u32 nfc_time0_val;
	u32 nfc_cfg0_val;
	u32 nfc_sts_mach_val;

	struct nand_inst inst_read_main_spare;
	struct nand_inst inst_read_main_raw;
	struct nand_inst inst_read_spare_raw;
	struct nand_inst inst_write_main_spare;
	struct nand_inst inst_write_main_raw;
	struct nand_inst inst_write_spare_raw;
	struct nand_inst inst_erase;

	bool randomizer;
};

#define GETCS(page)                                                            \
	host->cs[((page) >> ((nfc_base->chip_shift) - (nfc_base->pageshift)))]

static inline void sprd_nand_cmd_init(struct nand_inst *cmd_inst, u32 position)
{
	cmd_inst->int_bits = position;
}

static inline void sprd_nand_cmd_add(struct nand_inst *cmd_inst, u16 inst_p)
{
	cmd_inst->inst[cmd_inst->cnt] = inst_p;
	cmd_inst->cnt++;
}

static inline void sprd_nand_cmd_tag(struct nand_inst *cmd_inst)
{
	cmd_inst->step_tag = cmd_inst->cnt;
}

static inline void sprd_nand_cmd_change(struct sprd_nand_host *host,
					struct nand_inst *cmd_inst, u32 pg)
{
	cmd_inst->inst[cmd_inst->step_tag] =
		INST_ADDR(SPRD_NAND_PAGE_SHIFT_0(pg), 1);
	cmd_inst->inst[cmd_inst->step_tag + 1] =
		INST_ADDR(SPRD_NAND_PAGE_SHIFT_8(pg), 0);
	if (host->param.ncycle == 5)
		cmd_inst->inst[cmd_inst->step_tag + 2] =
			INST_ADDR(SPRD_NAND_PAGE_SHIFT_16(pg), 0);
}

struct nand_ecc_stats {
	u16 ecc_stats[16];
	u32 layout4_ecc_stats;
	u32 freecount[5];
};

static struct nand_inst inst_reset = {
	"_inst_reset",
	3, 0, INT_TO | INT_DONE, {
		INST_CMD(0xFF),
		INST_WRB0(),
		INST_DONE()
	}
};

static struct nand_inst inst_readid = {
	"_inst_readid",
	5, 0, INT_TO | INT_DONE, {
		INST_CMD(0x90),
		INST_ADDR(0, 0),
		INST_INOP(10),
		INST_IDST(0x08),
		INST_DONE()
	}
};

static struct sprd_nand_timing default_timing = {10, 25, 15};

static const u32 seedtbl[64] = {
	0x056c, 0x1bc77, 0x5d356, 0x1f645d, 0x0fbc, 0x0090c, 0x7f880, 0x3d9e86,
	0x1717, 0x1e1ad, 0x6db67, 0x7d7ea0, 0x0a52, 0x0d564, 0x6fbac, 0x6823dd,
	0x07cf, 0x1cb3b, 0x37cd1, 0x5c91f0, 0x064e, 0x167a7, 0x0f1d2, 0x506be8,
	0x098c, 0x1bd54, 0x2c2af, 0x4b5fb7, 0x1399, 0x11690, 0x1d310, 0x27e53b,
	0x1246, 0x14794, 0x0f34f, 0x347bc4, 0x0150, 0x00787, 0x73450, 0x3d8927,
	0x11f1, 0x17bad, 0x46eaa, 0x5403f5, 0x1026, 0x173ab, 0x79634, 0x01b987,
	0x1c45, 0x08b63, 0x42924, 0x4bf708, 0x012a, 0x03a3a, 0x435d5, 0x1a7baa,
	0x0849, 0x1cb9b, 0x28350, 0x1e8309, 0x1d4c, 0x0af6e, 0x0949e, 0x00193a,
};

static s8 bit_num8[256] = {
	8, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 4, 7, 6, 6, 5, 6, 5, 5, 4,
	6, 5, 5, 4, 5, 4, 4, 3, 7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2, 7, 6, 6, 5, 6, 5, 5, 4,
	6, 5, 5, 4, 5, 4, 4, 3, 6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2, 5, 4, 4, 3, 4, 3, 3, 2,
	4, 3, 3, 2, 3, 2, 2, 1, 7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2, 6, 5, 5, 4, 5, 4, 4, 3,
	5, 4, 4, 3, 4, 3, 3, 2, 5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
	6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2, 5, 4, 4, 3, 4, 3, 3, 2,
	4, 3, 3, 2, 3, 2, 2, 1, 5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
	4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0};

static inline void sprd_nand_writel(struct sprd_nand_host *host,
				    unsigned long val, unsigned long reg)
{
	writel_relaxed(val, host->ioaddr + reg);
}

static inline unsigned long sprd_nand_readl(struct sprd_nand_host *host,
					    unsigned long reg)
{
	return readl_relaxed(host->ioaddr + reg);
}

static void sprd_nand_pageseed(struct sprd_nand_host *host, u32 page)
{
	u32 i, j, remain, shift, mask, numbit;
	u32 offset = page >> 4;

	if (page & ~SPRD_NAND_PAGE_MASK)
		return;

	memset(host->seedbuf_v, 0, SEED_BUF_SIZE * 4);
	for (i = 0; i < SEED_TBL_SIZE; i++) {
		switch (i & 0x3) {
		case 0:
			numbit = 13;
			break;
		case 1:
			numbit = 17;
			break;
		case 2:
			numbit = 19;
			break;
		case 3:
			numbit = 23;
			break;
		}
		for (j = 0; j <= numbit - 1; j++) {
			if (seedtbl[i] & BIT(j))
				host->seedbuf_v[i] |= BIT(numbit - 1 - j);
		}
		if (offset) {
			if (offset > numbit - 1)
				shift = offset - numbit;
			else
				shift = offset;
			mask = (BIT(numbit) - 1) >> shift;
			remain = host->seedbuf_v[i] & ~mask;
			remain >>= numbit - shift;
			host->seedbuf_v[i] &= mask;
			host->seedbuf_v[i] <<= shift;
			host->seedbuf_v[i] |= remain;
		}
	}
	host->seedbuf_v[SEED_TBL_SIZE] = host->seedbuf_v[0];
	host->seedbuf_v[SEED_TBL_SIZE + 1] = host->seedbuf_v[1];
	host->seedbuf_v[SEED_TBL_SIZE + 2] = host->seedbuf_v[2];
	host->seedbuf_v[SEED_TBL_SIZE + 3] = host->seedbuf_v[3];
}

static void sprd_nand_enable_randomizer(struct sprd_nand_host *host,
					u32 page, u32 mode)
{
	u32 spl_page = SPL_MAX_SIZE / host->param.page_size;
	u32 *seedaddr = host->seedbuf_p + PAGE2SEED_ADDR_OFFSET(page);

	if (host->randomizer)
		return;
	if (page < spl_page && mode != MTD_OPS_RAW) {
		sprd_nand_pageseed(host, page);

		sprd_nand_writel(host, NFC_POLYNOMIALS0, NFC_POLY0_REG);
		sprd_nand_writel(host, NFC_POLYNOMIALS1, NFC_POLY1_REG);
		sprd_nand_writel(host, NFC_POLYNOMIALS2, NFC_POLY2_REG);
		sprd_nand_writel(host, NFC_POLYNOMIALS3, NFC_POLY3_REG);
		sprd_nand_writel(host, NFC_POLYNOMIALS0, NFC_POLY4_REG);
		sprd_nand_writel(host, NFC_POLYNOMIALS1, NFC_POLY5_REG);
		sprd_nand_writel(host, NFC_POLYNOMIALS2, NFC_POLY6_REG);
		sprd_nand_writel(host, NFC_POLYNOMIALS3, NFC_POLY7_REG);
		sprd_nand_writel(host, 0x0, NFC_SEED_ADDRH_REG);
		sprd_nand_writel(host, (unsigned long)seedaddr,
				 NFC_SEED_ADDRL_REG);
		host->nfc_cfg3 |= CFG3_RANDOM_EN | CFG3_POLY_4R1_EN;
		sprd_nand_writel(host, host->nfc_cfg3, NFC_CFG3_REG);
	}
}

static void sprd_nand_disable_randomizer(struct sprd_nand_host *host,
					 u32 page, u32 mode)
{
	u32 spl_page = SPL_MAX_SIZE / host->param.page_size;

	if (host->randomizer)
		return;
	if (page < spl_page && mode != MTD_OPS_RAW) {
		host->nfc_cfg3 &= ~(CFG3_POLY_4R1_EN | CFG3_RANDOM_EN);
		sprd_nand_writel(host, host->nfc_cfg3, NFC_CFG3_REG);
	}
}

static void sprd_nand_cmd_exec(struct sprd_nand_host *host,
			       struct nand_inst *program, u32 repeat,
			       u32 if_use_int)
{
	u32 i;

	host->nfc_cfg0_val |= CFG0_SET_REPEAT_NUM(repeat);
	sprd_nand_writel(host, host->nfc_cfg0_val, NFC_CFG0_REG);
	sprd_nand_writel(host, host->nfc_time0_val, NFC_TIMING0_REG);
	sprd_nand_writel(host, host->nfc_sts_mach_val, NFC_STAT_STSMCH_REG);
	for (i = 0; i < program->cnt; i += 2) {
		sprd_nand_writel(host, (program->inst[i] |
				(program->inst[i + 1] << 16)),
			       NFC_INST00_REG + (i << 1));
	}

	sprd_nand_writel(host, 0, NFC_INT_REG);
	sprd_nand_writel(host, GENMASK(11, 8), NFC_INT_REG);
	if (if_use_int)
		sprd_nand_writel(host, (program->int_bits & 0xF), NFC_INT_REG);

	sprd_nand_writel(host, host->nfc_start | CTRL_NFC_CMD_START,
			 NFC_START_REG);
}

static int sprd_nand_cmd_wait(struct sprd_nand_host *host,
			      struct nand_inst *program)
{
	int ret = -EIO;
	u32 nfc_timeout_val, regval;

	if (strcmp(program->program_name, "_inst_reset") == 0)
		nfc_timeout_val = NFC_RESET_TIMEOUT_VAL;
	else
		nfc_timeout_val = NFC_TIMEOUT_VAL;

	ret = readl_relaxed_poll_timeout(host->ioaddr + NFC_INT_REG, regval,
					 (SPRD_NAND_GET_INT_VAL(regval) &
					  ((INT_TO | INT_STSMCH | INT_WP) &
					   program->int_bits)) ||
					 (SPRD_NAND_GET_INT_VAL(regval) &
					  (INT_DONE & program->int_bits)),
					 0, nfc_timeout_val);
	if (ret) {
		dev_err(host->dev, "command %s wait timeout.\n",
			program->program_name);
		return ret;
	}

	if (SPRD_NAND_GET_INT_VAL(regval) &
	    ((INT_TO | INT_STSMCH | INT_WP) & program->int_bits))
		ret = -EIO;

	if (ret)
		pr_err("sprd nand cmd %s fail", program->program_name);

	return ret;
}

/* host state0 is used for nand id and reset cmd */
static void sprd_nand_init_reg_state0(struct sprd_nand_host *host)
{
	host->nfc_cfg0 = CFG0_DEF0_MAST_ENDIAN | CFG0_DEF0_SECT_NUM_IN_INST |
		CFG0_DEF0_DETECT_ALL_FF;
	host->nfc_cfg3 = CFG3_DETECT_ALL_FF;
	host->nfc_cfg4 = CFG4_SLICE_CLK_EN | CFG4_PHY_DLL_CLK_2X_EN;
	sprd_nand_writel(host, host->nfc_cfg0, NFC_CFG0_REG);
	sprd_nand_writel(host, host->nfc_cfg3, NFC_CFG3_REG);
	sprd_nand_writel(host, host->nfc_cfg4, NFC_CFG4_REG);

	sprd_nand_writel(host, RAM_MAIN_ADDR(-1), NFC_MAIN_ADDRH_REG);
	sprd_nand_writel(host, RAM_MAIN_ADDR(-1), NFC_MAIN_ADDRL_REG);
	sprd_nand_writel(host, RAM_SPAR_ADDR(-1), NFC_SPAR_ADDRH_REG);
	sprd_nand_writel(host, RAM_SPAR_ADDR(-1), NFC_SPAR_ADDRL_REG);
	sprd_nand_writel(host, RAM_STAT_ADDR(-1), NFC_STAT_ADDRH_REG);
	sprd_nand_writel(host, RAM_STAT_ADDR(-1), NFC_STAT_ADDRL_REG);
	/* need delay time after set NFC_CFG0_REG */
	usleep_range(1000, 1500);
}

static void sprd_nand_select_cs(struct sprd_nand_host *host, int cs)
{
	host->nfc_cfg0 = (host->nfc_cfg0 & CFG0_CS_MSKCLR) |
		CFG0_SET_CS_SEL(cs);
}

static int sprd_nand_reset(struct sprd_nand_host *host)
{
	struct nand_inst *inst = &inst_reset;

	host->nfc_sts_mach_val = DEF0_MATCH;
	host->nfc_time0_val = host->nfc_time0_r;
	host->nfc_cfg0_val = host->nfc_cfg0 | CFG0_SET_NFC_MODE(0x2);
	sprd_nand_cmd_exec(host, inst, 1, 0);

	return sprd_nand_cmd_wait(host, inst);
}

static int sprd_nand_readid(struct sprd_nand_host *host)
{
	struct nand_inst *inst = &inst_readid;
	int ret;
	u32 id0, id1;
	static u32 if_has_read;

	host->nfc_sts_mach_val = DEF0_MATCH;
	host->nfc_cfg0_val = host->nfc_cfg0 | CFG0_SET_NFC_MODE(0x2);
	host->nfc_time0_val = NFC_READID_TIMING;
	sprd_nand_cmd_exec(host, inst, 1, 0);
	ret = sprd_nand_cmd_wait(host, inst);
	if (ret != 0)
		return ret;

	id0 = sprd_nand_readl(host, NFC_STATUS0_REG);
	id1 = sprd_nand_readl(host, NFC_STATUS1_REG);
	if (if_has_read == 0) {
		if_has_read = 1;
		host->param.id[0] = (u8)(id0 & 0xFF);
		host->param.id[1] = (u8)((id0 >> 8) & 0xFF);
		host->param.id[2] = (u8)((id0 >> 16) & 0xFF);
		host->param.id[3] = (u8)((id0 >> 24) & 0xFF);
		host->param.id[4] = (u8)(id1 & 0xFF);
		host->param.id[5] = (u8)((id1 >> 8) & 0xFF);
		host->param.id[6] = (u8)((id1 >> 16) & 0xFF);
		host->param.id[7] = (u8)((id1 >> 24) & 0xFF);
	} else if ((host->param.id[0] != (u8)(id0 & 0xFF)) ||
		   (host->param.id[1] != (u8)((id0 >> 8) & 0xFF)) ||
		   (host->param.id[2] != (u8)((id0 >> 16) & 0xFF)) ||
		   (host->param.id[3] != (u8)((id0 >> 24) & 0xFF)) ||
		   (host->param.id[4] != (u8)(id1 & 0xFF)) ||
		   (host->param.id[5] != (u8)((id1 >> 8) & 0xFF)) ||
		   (host->param.id[6] != (u8)((id1 >> 16) & 0xFF)) ||
		   (host->param.id[7] != (u8)((id1 >> 24) & 0xFF))) {
		return -EINVAL;
	}

	return ret;
}

static void sprd_nand_delect_cs(struct sprd_nand_host *host, int ret)
{
	if (ret == -EIO) {
		sprd_nand_writel(host, CTRL_NFC_CMD_CLR, NFC_START_REG);
		sprd_nand_reset(host);
	}
}

static void sprd_nand_set_timing_config(struct sprd_nand_host *host,
					struct sprd_nand_timing *timing,
					u32 clk_hz)
{
	u32 temp_val, clk_mhz, reg_val = 0;

	/* the clock source is 2x clock */
	clk_mhz = clk_hz / 2000000;
	/* get acs value : 0ns */
	reg_val |= ((2 & 0x1F) << NFC_ACS_OFFSET);

	/* temp_val: 0: 1clock, 1: 2clocks... */
	temp_val = timing->ace_ns * clk_mhz / 1000 - 1;
	if (((timing->ace_ns * clk_mhz) % 1000) != 0)
		temp_val++;

	reg_val |= ((temp_val & 0x1F) << NFC_ACE_OFFSET);

	/* get rws value : 20 ns */
	temp_val = 20 * clk_mhz / 1000 - 1;
	reg_val |= ((temp_val & 0x3F) << NFC_RWS_OFFSET);

	/* get rws value : 0 ns */
	reg_val |= ((2 & 0x1F) << NFC_RWE_OFFSET);

	/*
	 * get rwh value,if the remainder bigger than 500, then plus 1 for more
	 * accurate
	 */
	temp_val = timing->rwh_ns * clk_mhz / 1000 - 1;
	if (((timing->rwh_ns * clk_mhz) % 1000) >= 500)
		temp_val++;

	reg_val |= ((temp_val & 0x1F) << NFC_RWH_OFFSET);

	/*
	 * get rwl value,if the remainder bigger than 500, then plus 1 for more
	 * accurate
	 */
	temp_val = timing->rwl_ns * clk_mhz / 1000 - 1;
	if (((timing->rwl_ns * clk_mhz) % 1000) >= 500)
		temp_val++;

	reg_val |= (temp_val & 0x3F);

	dev_dbg(host->dev, "%s nand timing val: 0x%x\n\r", __func__, reg_val);

	host->nfc_time0_r = reg_val;
	host->nfc_time0_w = reg_val;
	host->nfc_time0_e = reg_val;
}

static struct sprd_nand_maker *sprd_nand_find_maker(u8 idmaker)
{
	struct sprd_nand_maker *pmaker = maker_table;

	while (pmaker->idmaker != 0) {
		if (pmaker->idmaker == idmaker)
			return pmaker;
		pmaker++;
	}

	return NULL;
}

static struct sprd_nand_device *
sprd_nand_find_device(struct sprd_nand_maker *pmaker, u8 id_device)
{
	struct sprd_nand_device *pdevice = pmaker->p_devtab;

	while (pdevice->id_device != 0) {
		if (pdevice->id_device == id_device)
			return pdevice;
		pdevice++;
	}

	return NULL;
}

static void sprd_nand_print_info(struct sprd_nand_vendor_param *p)
{
	struct sprd_nand_maker *pmaker = sprd_nand_find_maker(p->idmaker);
	struct sprd_nand_device *pdevice = sprd_nand_find_device(pmaker,
								 p->id_device);

	pr_info("nand device is %s:%s\n", pmaker->p_name, pdevice->p_name);
	pr_info("nand device block size is %d\n", p->blk_size);
	pr_info("nand device page size is %d\n", p->page_size);
	pr_info("nand device spare size is %d\n", p->nspare_size);
	pr_info("nand device eccbits is %d\n", p->s_oob.ecc_bits);
}

struct sprd_nand_vendor_param *sprd_get_nand_param(u8 *id)
{
	struct sprd_nand_vendor_param *param = sprd_nand_vendor_param_table;
	u32 i;

	for (i = 0; i < 5; i++)
		pr_info("nand device id[%d] is %x\n", i, id[i]);

	while (param->idmaker != 0) {
		if (!memcmp(param->id, id, i)) {
			sprd_nand_print_info(param);
			return param;
		}
		param++;
	}
	pr_info("Nand params unconfig, please check it. Halt on booting!!!\n");

	return NULL;
}

static int sprd_nand_param_init_nandhw(struct sprd_nand_host *host)
{
	struct sprd_nand_vendor_param *param;

	/* get base param info */
	param = sprd_get_nand_param(host->param.id);
	if (!param)
		return -EINVAL;

	sprd_nand_set_timing_config(host, &param->s_timing, host->frequence);
	host->param.nblkcnt = param->blk_num;
	host->param.npage_per_blk = param->blk_size / param->page_size;
	host->param.nsect_per_page = param->page_size / param->nsect_size;
	host->param.nsect_size = param->nsect_size;
	host->param.nspare_size = param->s_oob.oob_size;
	host->param.page_size = param->page_size; /* no contain spare */
	host->param.main_size = param->page_size;
	host->param.spare_size = param->nspare_size;
	host->param.badflag_pos = 0; /* --- default:need fixed */
	host->param.badflag_len = 2; /* --- default:need fixed */
	host->param.ecc_mode = param->s_oob.ecc_bits;
	host->param.ecc_pos = param->s_oob.ecc_pos;
	host->param.ecc_size = param->s_oob.ecc_size;
	host->param.info_pos = param->s_oob.info_pos;
	host->param.info_size = param->s_oob.info_size;

	host->param.nbus_width = param->nbus_width;
	host->param.ncycle = param->ncycles;

	dev_dbg(host->dev, "nand:nBlkcnt = [%d], npageperblk = [%d]\n",
		host->param.nblkcnt, host->param.npage_per_blk);
	dev_dbg(host->dev, "nand:nsectperpage = [%d], nsecsize = [%d]\n",
		host->param.nsect_per_page, host->param.nsect_size);
	dev_dbg(host->dev, "nand:nspare_size = [%d], page_size = [%d]\n",
		host->param.nspare_size, host->param.page_size);
	dev_dbg(host->dev, "nand:page_size =  [%d], spare_size = [%d]\n",
		host->param.page_size, host->param.spare_size);
	dev_dbg(host->dev, "nand:ecc_mode = [%d], ecc_pos = [%d]\n",
		host->param.ecc_mode, host->param.ecc_pos);
	dev_dbg(host->dev, "nand:ecc_size = [%d], info_pos = [%d], info_size = [%d]\n",
		host->param.ecc_size, host->param.info_pos,
		host->param.info_size);

	/* setting globe param seting */
	switch (host->param.ecc_mode) {
	case 1:
		host->risk_threshold = 1;
		host->eccmode_reg = 0;
		break;
	case 2:
		host->risk_threshold = 1;
		host->eccmode_reg = 1;
		break;
	case 4:
		host->risk_threshold = 2;
		host->eccmode_reg = 2;
		break;
	case 8:
		host->risk_threshold = 4;
		host->eccmode_reg = 3;
		break;
	case 12:
		host->risk_threshold = 6;
		host->eccmode_reg = 4;
		break;
	case 16:
		host->risk_threshold = 8;
		host->eccmode_reg = 5;
		break;
	case 24:
		host->risk_threshold = 12;
		host->eccmode_reg = 6;
		break;
	case 40:
		host->risk_threshold = 20;
		host->eccmode_reg = 7;
		break;
	case 60:
		host->risk_threshold = 30;
		host->eccmode_reg = 8;
		break;
	default:
		dev_err(host->dev, "nand:sprd nand ecc mode not support!\n");
		return -EINVAL;
	}

	host->sect_perpage_shift = ffs(host->param.nsect_per_page) - 1;
	host->sectshift = ffs(host->param.nsect_size) - 1;
	host->sectmsk = BIT(host->sectshift) - 1;
	host->pageshift = ffs(host->param.nsect_per_page <<
			      host->sectshift) - 1;
	host->blkshift = ffs(host->param.npage_per_blk << host->pageshift) - 1;
	host->bufshift = ffs(NFC_MBUF_SIZE) - 1;
	host->bufshift = min(host->bufshift, host->blkshift);
	dev_dbg(host->dev, "nand: sectperpgshift %d, sectshift %d\n",
		host->sect_perpage_shift, host->sectshift);
	dev_dbg(host->dev, "nand:secmsk %d, pageshift %d,blkshift %d, bufshift %d\n",
		host->sectmsk, host->pageshift, host->blkshift, host->bufshift);

	return 0;
}

static void sprd_nand_param_init_inst(struct sprd_nand_host *host)
{
	u32 column = BIT(host->pageshift);

	dev_dbg(host->dev,
		"nand:param init inst column %d,host->pageshift %d\n",
		column, host->pageshift);
	if (host->param.nbus_width == BW_16) {
		pr_debug("nand:buswidth = 16\n");
		column >>= 1;
	}
	/* erase */
	host->inst_erase.program_name = "_inst_erase";
	sprd_nand_cmd_init(&host->inst_erase,
			   INT_TO | INT_DONE | INT_WP | INT_STSMCH);
	sprd_nand_cmd_add(&host->inst_erase, INST_CMD(0x60));
	sprd_nand_cmd_tag(&host->inst_erase);
	sprd_nand_cmd_add(&host->inst_erase, INST_ADDR(0, 1));
	sprd_nand_cmd_add(&host->inst_erase, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nand_cmd_add(&host->inst_erase, INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_erase, INST_CMD(0xD0));
	sprd_nand_cmd_add(&host->inst_erase, INST_WRB0());
	sprd_nand_cmd_add(&host->inst_erase, INST_CMD(0x70));
	sprd_nand_cmd_add(&host->inst_erase, INST_IDST(1));
	sprd_nand_cmd_add(&host->inst_erase, INST_DONE());
	/* read main+spare(info)+Ecc or Raw */
	host->inst_read_main_spare.program_name = "_inst_read_main_spare";
	sprd_nand_cmd_init(&host->inst_read_main_spare, INT_TO | INT_DONE);
	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_CMD(0x00));
	sprd_nand_cmd_add(&host->inst_read_main_spare,
			  INST_ADDR((0xFF & (u8)column), 0));
	sprd_nand_cmd_add(&host->inst_read_main_spare,
			  INST_ADDR((0xFF & (u8)(column >> 8)), 0));
	sprd_nand_cmd_tag(&host->inst_read_main_spare);
	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_ADDR(0, 1));
	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nand_cmd_add(&host->inst_read_main_spare, INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_CMD(0x30));
	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_WRB0());
	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_SRDT());

	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_CMD(0x05));
	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_CMD(0xE0));
	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_MRDT());

	sprd_nand_cmd_add(&host->inst_read_main_spare, INST_DONE());
	/* read main raw */
	host->inst_read_main_raw.program_name = "_inst_read_main_raw";
	sprd_nand_cmd_init(&host->inst_read_main_raw, INT_TO | INT_DONE);
	sprd_nand_cmd_add(&host->inst_read_main_raw, INST_CMD(0x00));
	sprd_nand_cmd_add(&host->inst_read_main_raw, INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_read_main_raw, INST_ADDR(0, 0));
	sprd_nand_cmd_tag(&host->inst_read_main_raw);
	sprd_nand_cmd_add(&host->inst_read_main_raw, INST_ADDR(0, 1));
	sprd_nand_cmd_add(&host->inst_read_main_raw, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nand_cmd_add(&host->inst_read_main_raw, INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_read_main_raw, INST_CMD(0x30));
	sprd_nand_cmd_add(&host->inst_read_main_raw, INST_WRB0());
	sprd_nand_cmd_add(&host->inst_read_main_raw, INST_MRDT());
	sprd_nand_cmd_add(&host->inst_read_main_raw, INST_DONE());
	/*
	 * read spare raw: read only main or only spare data,
	 * it is read to main addr.
	 */
	host->inst_read_spare_raw.program_name = "_inst_read_spare_raw";
	sprd_nand_cmd_init(&host->inst_read_spare_raw, INT_TO | INT_DONE);
	sprd_nand_cmd_add(&host->inst_read_spare_raw, INST_CMD(0x00));
	sprd_nand_cmd_add(&host->inst_read_spare_raw,
			  INST_ADDR((0xFF & (u8)column), 0));
	sprd_nand_cmd_add(&host->inst_read_spare_raw,
			  INST_ADDR((0xFF & (u8)(column >> 8)), 0));
	sprd_nand_cmd_tag(&host->inst_read_spare_raw);
	sprd_nand_cmd_add(&host->inst_read_spare_raw, INST_ADDR(0, 1));
	sprd_nand_cmd_add(&host->inst_read_spare_raw, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nand_cmd_add(&host->inst_read_spare_raw, INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_read_spare_raw, INST_CMD(0x30));
	sprd_nand_cmd_add(&host->inst_read_spare_raw, INST_WRB0());
	sprd_nand_cmd_add(&host->inst_read_spare_raw, INST_MRDT());
	sprd_nand_cmd_add(&host->inst_read_spare_raw, INST_DONE());
	/* write main+spare(info)+ecc */
	host->inst_write_main_spare.program_name = "_inst_write_main_spare";
	sprd_nand_cmd_init(&host->inst_write_main_spare,
			   INT_TO | INT_DONE | INT_WP | INT_STSMCH);
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_CMD(0x80));
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_ADDR(0, 0));
	sprd_nand_cmd_tag(&host->inst_write_main_spare);
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_ADDR(0, 1));
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nand_cmd_add(&host->inst_write_main_spare,
				  INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_MWDT());

	/* just need input column addr */
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_CMD(0x85));
	sprd_nand_cmd_add(&host->inst_write_main_spare,
			  INST_ADDR((0xFF & (u8)column), 0));
	sprd_nand_cmd_add(&host->inst_write_main_spare,
			  INST_ADDR((0xFF & (u8)(column >> 8)), 0));
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_SWDT());

	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_CMD(0x10));
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_WRB0());
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_CMD(0x70));
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_IDST(1));
	sprd_nand_cmd_add(&host->inst_write_main_spare, INST_DONE());
	/* write main raw */
	host->inst_write_main_raw.program_name = "_inst_write_main_raw";
	sprd_nand_cmd_init(&host->inst_write_main_raw,
			   INT_TO | INT_DONE | INT_WP | INT_STSMCH);
	sprd_nand_cmd_add(&host->inst_write_main_raw, INST_CMD(0x80));
	sprd_nand_cmd_add(&host->inst_write_main_raw, INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_write_main_raw, INST_ADDR(0, 0));
	sprd_nand_cmd_tag(&host->inst_write_main_raw);
	sprd_nand_cmd_add(&host->inst_write_main_raw, INST_ADDR(0, 1));
	sprd_nand_cmd_add(&host->inst_write_main_raw, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nand_cmd_add(&host->inst_write_main_raw, INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_write_main_raw, INST_MWDT());
	sprd_nand_cmd_add(&host->inst_write_main_raw, INST_CMD(0x10));
	sprd_nand_cmd_add(&host->inst_write_main_raw, INST_WRB0());
	sprd_nand_cmd_add(&host->inst_write_main_raw, INST_CMD(0x70));
	sprd_nand_cmd_add(&host->inst_write_main_raw, INST_IDST(1));
	sprd_nand_cmd_add(&host->inst_write_main_raw, INST_DONE());
	/* write spare raw */
	host->inst_write_spare_raw.program_name = "_inst_write_spare_raw";
	sprd_nand_cmd_init(&host->inst_write_spare_raw,
			   INT_TO | INT_DONE | INT_WP | INT_STSMCH);
	sprd_nand_cmd_add(&host->inst_write_spare_raw, INST_CMD(0x80));
	sprd_nand_cmd_add(&host->inst_write_spare_raw,
			  INST_ADDR((0xFF & column), 0));
	sprd_nand_cmd_add(&host->inst_write_spare_raw,
			  INST_ADDR((0xFF & (column >> 8)), 0));
	sprd_nand_cmd_tag(&host->inst_write_spare_raw);
	sprd_nand_cmd_add(&host->inst_write_spare_raw, INST_ADDR(0, 1));
	sprd_nand_cmd_add(&host->inst_write_spare_raw, INST_ADDR(0, 0));
	if (host->param.ncycle == 5)
		sprd_nand_cmd_add(&host->inst_write_spare_raw, INST_ADDR(0, 0));
	sprd_nand_cmd_add(&host->inst_write_spare_raw, INST_SWDT());
	sprd_nand_cmd_add(&host->inst_write_spare_raw, INST_CMD(0x10));
	sprd_nand_cmd_add(&host->inst_write_spare_raw, INST_WRB0());
	sprd_nand_cmd_add(&host->inst_write_spare_raw, INST_CMD(0x70));
	sprd_nand_cmd_add(&host->inst_write_spare_raw, INST_IDST(1));
	sprd_nand_cmd_add(&host->inst_write_spare_raw, INST_DONE());
}

static int sprd_nand_param_init_buf(struct sprd_nand_host *host)
{
	dma_addr_t phys_addr;
	void *virt_ptr;
	u32 mbuf_size = BIT(host->bufshift);
	u32 sbuf_size = BIT(host->bufshift - host->sectshift) *
		host->param.nspare_size;
	u32 stsbuf_size = BIT(host->bufshift - host->sectshift) *
		(sizeof(struct nand_ecc_stats));
	u32 seedbuf_size = SEED_BUF_SIZE * 4;

	dev_dbg(host->dev,
		"nand:mbuf_size %d, sbuf_size %d, stsbuf_size %d\n",
		 mbuf_size, sbuf_size, stsbuf_size);
	dev_dbg(host->dev,
		"nand:host->bufshift %d, param.nspare_size %d,sectshift %d\n",
		host->bufshift, host->param.nspare_size, host->sectshift);
	virt_ptr = dma_alloc_coherent(host->dev, mbuf_size, &phys_addr,
				      GFP_KERNEL);
	if (!virt_ptr) {
		dev_err(host->dev,
			"nand:Failed to allocate memory for DMA main buf\n");
		return -ENOMEM;
	}
	host->mbuf_p = (u8 *)phys_addr;
	host->mbuf_v = (u8 *)virt_ptr;
	host->mbuf_size = mbuf_size;

	virt_ptr = dma_alloc_coherent(host->dev, sbuf_size, &phys_addr,
				      GFP_KERNEL);
	if (!virt_ptr) {
		dev_err(host->dev,
			"nand:Failed to allocate memory for DMA spare buf\n");
		goto err_sbuf_alloc;
	}
	host->sbuf_p = (u8 *)phys_addr;
	host->sbuf_v = (u8 *)virt_ptr;
	host->sbuf_size = sbuf_size;

	virt_ptr = dma_alloc_coherent(host->dev, stsbuf_size, &phys_addr,
				      GFP_KERNEL);
	if (!virt_ptr) {
		dev_err(host->dev,
			"nand:Failed to allocate memory for DMA sts buffer\n");
		goto err_stsbuf_alloc;
	}
	host->stsbuf_p = (u8 *)phys_addr;
	host->stsbuf_v = (u8 *)virt_ptr;
	host->stsbuf_size = stsbuf_size;

	virt_ptr = dma_alloc_coherent(host->dev, seedbuf_size, &phys_addr,
				      GFP_KERNEL);
	if (!virt_ptr) {
		dev_err(host->dev, "nand:Failed to allocate memory for DMA seed buffer\n");
		goto err_seedbuf_alloc;
	}
	host->seedbuf_p = (u32 *)phys_addr;
	host->seedbuf_v = (u32 *)virt_ptr;
	host->seedbuf_size = seedbuf_size;

	return 0;

err_seedbuf_alloc:
	dma_free_coherent(host->dev, stsbuf_size, (void *)host->stsbuf_v,
			  (dma_addr_t)host->stsbuf_p);
err_stsbuf_alloc:
	dma_free_coherent(host->dev, sbuf_size, (void *)host->sbuf_v,
			  (dma_addr_t)host->sbuf_p);
err_sbuf_alloc:
	dma_free_coherent(host->dev, mbuf_size, (void *)host->mbuf_v,
			  (dma_addr_t)host->mbuf_p);

	return -ENOMEM;
}

static void sprd_nand_free_dma_buf(struct sprd_nand_host *host)
{
	dma_free_coherent(host->dev, host->seedbuf_size,
			  (void *)host->seedbuf_v, (dma_addr_t)host->seedbuf_p);
	dma_free_coherent(host->dev, host->stsbuf_size, (void *)host->stsbuf_v,
			  (dma_addr_t)host->stsbuf_p);
	dma_free_coherent(host->dev, host->sbuf_size, (void *)host->sbuf_v,
			  (dma_addr_t)host->sbuf_p);
	dma_free_coherent(host->dev, host->mbuf_size, (void *)host->mbuf_v,
			  (dma_addr_t)host->mbuf_p);
}

static int sprd_nand_param_init(struct sprd_nand_host *host)
{
	int ret;

	ret = sprd_nand_param_init_nandhw(host);
	if (ret)
		return ret;

	sprd_nand_param_init_inst(host);

	return sprd_nand_param_init_buf(host);
}

static void sprd_nand_init_reg_state1(struct sprd_nand_host *host)
{
	host->nfc_start |= CTRL_DEF1_ECC_MODE(host->eccmode_reg);
	host->nfc_cfg0 |= CFG0_DEF1_SECT_NUM(host->param.nsect_per_page) |
		CFG0_DEF1_BUS_WIDTH(host->param.nbus_width) |
		CFG0_DEF1_MAIN_SPAR_APT(host->param.nsect_per_page);
	host->nfc_cfg1 = CFG1_DEF1_SPAR_INFO_SIZE(host->param.info_size) |
		CFG1_DEF1_SPAR_SIZE(host->param.nspare_size) |
		CFG1_DEF_MAIN_SIZE(host->param.nsect_size);
	host->nfc_cfg2 = CFG2_DEF1_SPAR_SECTOR_NUM(host->param.nsect_per_page) |
		CFG2_DEF1_SPAR_INFO_POS(host->param.info_pos) |
		CFG2_DEF1_ECC_POSITION(host->param.ecc_pos);

	sprd_nand_writel(host, host->nfc_cfg0, NFC_CFG0_REG);
	sprd_nand_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
	sprd_nand_writel(host, host->nfc_cfg2, NFC_CFG2_REG);
	sprd_nand_writel(host, 0xFFFFFFFF, NFC_TIMEOUT_REG);

	sprd_nand_writel(host, 0x0, NFC_STAT_ADDRH_REG);
	sprd_nand_writel(host, RAM_STAT_ADDR(host->stsbuf_p),
			 NFC_STAT_ADDRL_REG);
	sprd_nand_writel(host, 0x0, NFC_MAIN_ADDRH_REG);
	sprd_nand_writel(host, RAM_MAIN_ADDR(host->mbuf_p), NFC_MAIN_ADDRL_REG);
	sprd_nand_writel(host, 0x0, NFC_SPAR_ADDRH_REG);
	sprd_nand_writel(host, RAM_SPAR_ADDR(host->sbuf_p), NFC_SPAR_ADDRL_REG);
	/* need delay time after set NFC_CFG0_REG */
	usleep_range(1000, 1500);
}

static int sprd_nand_debugfs_show(struct sprd_nfc_base *nfcbase,
				  struct seq_file *s, void *data)
{
	u32 i;
	struct sprd_nand_host *host = (struct sprd_nand_host *)nfcbase->priv;

	seq_printf(s, "nand drv irq = %d\n", host->irq);
	seq_printf(s, "nand drv freq = %d\n", host->frequence);
	seq_printf(s, "nand drv ioaddr = %p\n", host->ioaddr);
	/*
	 * resource1: nand param & mtd ecclayout & risk
	 * threshold & nessisary const value
	 */
	seq_printf(s, "drv eccmode_reg = 0x%x\n", host->eccmode_reg);
	seq_printf(s, "drv risk_threshold = %d\n", host->risk_threshold);
	seq_printf(s, "drv csnum = %d:", host->csnum);
	for (i = 0; i < host->csnum; i++)
		seq_printf(s, "\t%d", host->cs[i]);

	seq_puts(s, "\n");
	/* seq_printf(s, "drv csDis = %d\n", host->csDis); */
	seq_printf(s, "drv sect_perpage_shift = %d\n",
		   host->sect_perpage_shift);
	seq_printf(s, "drv sectshift = %d\n", host->sectshift);
	seq_printf(s, "drv sectmsk = 0x%x\n", host->sectmsk);
	seq_printf(s, "drv pageshift = %d\n", host->pageshift);
	seq_printf(s, "drv blkshift = %d\n", host->blkshift);
	seq_printf(s, "drv bufshift = %d\n", host->bufshift);
	/* resource3: local DMA buffer */
	seq_printf(s, "drv mbuf_p = %p\n", host->mbuf_p);
	seq_printf(s, "drv mbuf_v = %p\n", host->mbuf_v);
	seq_printf(s, "drv sbuf_p = %p\n", host->sbuf_p);
	seq_printf(s, "drv sbuf_v = %p\n", host->sbuf_v);
	seq_printf(s, "drv stsbuf_p = %p\n", host->stsbuf_p);
	seq_printf(s, "drv stsbuf_v = %p\n", host->stsbuf_v);
	/*
	 * resource4: register base value. some value is const while operate
	 * on nand flash, we store it to local valuable.
	 * Time & cfg & status mach. It is different with operation R W E.
	 */
	seq_printf(s, "drv nfc_time0_r = 0x%x\n", host->nfc_time0_r);
	seq_printf(s, "drv nfc_time0_w = 0x%x\n", host->nfc_time0_w);
	seq_printf(s, "drv nfc_time0_e = 0x%x\n", host->nfc_time0_e);

	seq_printf(s, "drv nfc_cfg0 = 0x%x\n", host->nfc_cfg0);
	seq_printf(s, "drv nfc_cfg1 = 0x%x\n", host->nfc_cfg1);
	seq_printf(s, "drv nfc_cfg2 = 0x%x\n", host->nfc_cfg2);

	/* seq_printf(s, "---Nand HW Param---\n"); */
	seq_puts(s, "drv param id:");
	for (i = 0; i < NFC_MAX_ID_LEN; i++)
		seq_printf(s, "\t0x%x", host->param.id[i]);

	seq_puts(s, "\n");
	seq_printf(s, "drv param nblkcnt = %d\n", host->param.nblkcnt);
	seq_printf(s, "drv param npage_per_blk = %d\n",
		   host->param.npage_per_blk);
	seq_printf(s, "drv param nsect_per_page = %d\n",
		   host->param.nsect_per_page);
	seq_printf(s, "drv param nsect_size = %d\n", host->param.nsect_size);
	seq_printf(s, "drv param nspare_size = %d\n", host->param.nspare_size);
	seq_printf(s, "drv param badflag_pos = %d\n", host->param.badflag_pos);
	seq_printf(s, "drv param badflag_len = %d\n", host->param.badflag_len);
	seq_printf(s, "drv param ecc_mode = %d\n", host->param.ecc_mode);
	seq_printf(s, "drv param ecc_pos = %d\n", host->param.ecc_pos);
	seq_printf(s, "drv param ecc_size = %d\n", host->param.ecc_size);
	seq_printf(s, "drv param info_pos = %d\n", host->param.info_pos);
	seq_printf(s, "drv param info_size = %d\n", host->param.info_size);
	seq_printf(s, "drv param nbus_width = %d\n", host->param.nbus_width);
	seq_printf(s, "drv param ncycle = %d\n", host->param.ncycle);
	/* ACS */
	seq_printf(s, "drv param t_als = %d\n", host->param.t_als);
	seq_printf(s, "drv param t_cls = %d\n", host->param.t_cls);
	/* ACE */
	seq_printf(s, "drv param t_clh = %d\n", host->param.t_clh);
	seq_printf(s, "drv param t_alh = %d\n", host->param.t_alh);
	/* RWS */
	seq_printf(s, "drv param t_rr = %d\n", host->param.t_rr);
	seq_printf(s, "drv param t_adl = %d\n", host->param.t_adl);
	/* RWH & RWL */
	seq_printf(s, "drv param t_wh = %d\n", host->param.t_wh);
	seq_printf(s, "drv param t_wp = %d\n", host->param.t_wp);
	seq_printf(s, "drv param t_reh = %d\n", host->param.t_reh);
	seq_printf(s, "drv param t_rp = %d\n", host->param.t_rp);

	return 0;
}

/*
 * 0: ecc pass
 * -EBADMSG: ecc fail
 * -EUCLEAN: ecc risk
 */
static int sprd_nand_check_ff(struct sprd_nand_host *host, u32 sects,
			      u32 mode)
{
	u32 i, obb_size, sectsize = BIT(host->sectshift);
	u8 *main_buf, *spare_buf;
	s32 bit0_num, bit0_total = 0;
	u32 mbit_0pos[60];
	u32 mbit_0arridx = 0;
	u32 sbit_0pos[60];
	u32 sbit_0arridx = 0;
	s32 risk_num = min_t(int32_t, 4, host->risk_threshold);

	if (mode == MTD_OPS_AUTO_OOB)
		obb_size = host->param.info_size;
	else
		obb_size = host->param.nspare_size;

	main_buf = host->mbuf_v + (sects << host->sectshift);
	spare_buf = host->sbuf_v + (sects * obb_size);

	for (i = 0; i < sectsize; i++) {
		bit0_num = (int32_t)bit_num8[main_buf[i]];
		if (bit0_num) {
			pr_err("main_buf[i] = 0x%x\n", main_buf[i]);
			bit0_total += bit0_num;
			if (bit0_total > risk_num)
				return -EBADMSG;

			mbit_0pos[mbit_0arridx] = i;
			mbit_0arridx++;
		}
	}
	sbit_0arridx = 0;
	for (i = 0; i < obb_size; i++) {
		bit0_num = (int32_t)bit_num8[spare_buf[i]];
		if (bit0_num) {
			pr_err("spare_buf[i] = 0x%x\n", spare_buf[i]);
			bit0_total += bit0_num;
			if (bit0_total > risk_num)
				return -EBADMSG;

			sbit_0pos[sbit_0arridx] = i;
			sbit_0arridx++;
		}
	}
	for (i = 0; i < mbit_0arridx; i++)
		main_buf[mbit_0pos[i]] = 0xFF;
	for (i = 0; i < sbit_0arridx; i++)
		spare_buf[sbit_0pos[i]] = 0xFF;

	return bit0_total;
}

static int sprd_nand_ecc_analyze(struct sprd_nand_host *host, u32 num,
				 struct mtd_ecc_stats *ecc_sts, u32 mode)
{
	u32 i;
	u32 n;
	struct nand_ecc_stats *nand_ecc_sts =
		(struct nand_ecc_stats *)(host->stsbuf_v);
	u32 sector_num = host->param.nsect_per_page;
	u32 ecc_banknum = num / sector_num;
	u32 sector = 0;
	int ret = 0;

	for (i = 0; i <= ecc_banknum; i++) {
		for (n = 0; n < min((num - sector_num * i), sector_num); n++) {
			sector = n + sector_num * i;
			switch (ECC_STAT(nand_ecc_sts->ecc_stats[n])) {
			case 0x00:
				/* pass */
				break;
			case 0x02:
			case 0x03:
				/* fail */
				ret = sprd_nand_check_ff(host, sector, mode);
				if (ret == -EBADMSG)
					ecc_sts->failed++;
				else
					ecc_sts->corrected += ret;
				break;
			case 0x01:
				if (ECC_NUM(nand_ecc_sts->ecc_stats[n]) ==
				    0x1FF) {
					ret = sprd_nand_check_ff(host, sector,
								 mode);
					if (ret == -EBADMSG)
						ecc_sts->failed++;
					else
						ecc_sts->corrected += ret;
				};
				if (host->param.ecc_mode <=
				    ECC_NUM(nand_ecc_sts->ecc_stats[n])) {
					ecc_sts->failed++;
					ret = -EBADMSG;
				} else {
					ecc_sts->corrected +=
					ECC_NUM(nand_ecc_sts->ecc_stats[n]);
				}
				break;
			default:
				pr_err("sprd nand error ecc sts\n");
				break;
			}
			if (ret == -EBADMSG)
				goto err;
		}
		nand_ecc_sts++;
	}
	if (ret)
		pr_err("sprd nand read ecc sts %d\n", ret);

	return ret;
err:
	pr_err("sprd nand read ecc sts %d\n", ret);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_OFFSET, 32, 4,
		       (void *)(host->mbuf_v + (sector << host->sectshift)),
		       0x200, 1);

	return ret;
}

/*
 * if(0!=mBuf) then read main area
 * if(0!=sBuf) then read spare area or read spare info
 * if(MTD_OPS_PLACE_OOB == mode) then just read main area with Ecc
 * if(MTD_OPS_AUTO_OOB == mode) then read main area(if(0!=mBuf)),
 * and spare info(if(0!=sbuf)), with Ecc
 * if(MTD_OPS_RAW == mode) then read main area(if(0!=mBuf)),
 * and spare info(if(0!=sbuf)), without Ecc
 * return
 * 0: ecc pass
 * -EBADMSG	: ecc fail
 * -EUCLEAN	: ecc risk
 * -EIO		: read fail
 * mtd->ecc_stats
 */
static int sprd_nand_read_page_in_block(struct sprd_nfc_base *nfc_base,
					u32 page, u32 num,
					u32 *ret_num, u32 if_has_mbuf,
					u32 if_has_sbuf, u32 mode,
					struct mtd_ecc_stats *ecc_sts)
{
	struct nand_inst *inst = NULL;
	int ret;
	u32 if_change_buf = 0;

	struct sprd_nand_host *host = (struct sprd_nand_host *)nfc_base->priv;
	struct nand_inst inst_read_main_spare = host->inst_read_main_spare;
	struct nand_inst inst_read_main_raw = host->inst_read_main_raw;
	struct nand_inst inst_read_spare_raw = host->inst_read_spare_raw;

	sprd_nand_select_cs(host, GETCS(page));
	host->nfc_sts_mach_val = MACH_READ;
	host->nfc_time0_val = host->nfc_time0_r;
	host->nfc_cfg0_val = (host->nfc_cfg0 | CFG0_SET_NFC_MODE(0));
	sprd_nand_enable_randomizer(host, page, mode);

	switch (mode) {
	case MTD_OPS_AUTO_OOB:
		host->nfc_cfg0_val |= CFG0_SET_SPARE_ONLY_INFO_PROD_EN;
	case MTD_OPS_PLACE_OOB:
		host->nfc_cfg0_val |= (CFG0_SET_ECC_EN | CFG0_SET_MAIN_USE |
				       CFG0_SET_SPAR_USE);
		inst = &inst_read_main_spare;
		break;
	case MTD_OPS_RAW:
		if (if_has_mbuf & if_has_sbuf) {
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE |
					       CFG0_SET_SPAR_USE);
			inst = &inst_read_main_spare;
		} else if (if_has_mbuf) {
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE);
			inst = &inst_read_main_raw;
		} else if (if_has_sbuf) {
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE);
			inst = &inst_read_spare_raw;
			/*
			 * 0 nand controller use mainAddr to send spare data,
			 * So i have to change some globe config here
			 * if_change_buf = 1;
			 * 1 change to main buf
			 */
			if_change_buf = 1;
			sprd_nand_writel(host, 0x0, NFC_MAIN_ADDRH_REG);
			sprd_nand_writel(host, RAM_MAIN_ADDR(host->sbuf_p),
					 NFC_MAIN_ADDRL_REG);
			/* 2 change page_size */
			host->nfc_cfg1 &= (~CFG1_temp_MIAN_SIZE_MSK);
			host->nfc_cfg1 |=
				CFG1_DEF_MAIN_SIZE(host->param.nspare_size <<
						   host->sect_perpage_shift);
			sprd_nand_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
			/* 3 change sect number */
			host->nfc_cfg0_val &= (~CFG0_SET_SECT_NUM_MSK);
			host->nfc_cfg0_val |= CFG0_DEF1_SECT_NUM(1);
		} else {
			sprd_nand_delect_cs(host, -EINVAL);
			return -EINVAL;
		}
		break;
	default:
		pr_err("nand:sprd nand ops mode error!\n");
		break;
	}

	sprd_nand_cmd_change(host, inst, page);
	sprd_nand_cmd_exec(host, inst, num, 0);
	ret = sprd_nand_cmd_wait(host, inst);
	if (if_change_buf) {
		/* 1 change to main buf */
		sprd_nand_writel(host, 0x0, NFC_MAIN_ADDRH_REG);
		sprd_nand_writel(host, RAM_MAIN_ADDR(host->mbuf_p),
				 NFC_MAIN_ADDRL_REG);
		/* 2 change page_size */
		host->nfc_cfg1 &= (~CFG1_temp_MIAN_SIZE_MSK);
		host->nfc_cfg1 |= CFG1_DEF_MAIN_SIZE(host->param.nsect_size);
		sprd_nand_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
		/* 3 change sect number */
		sprd_nand_writel(host, host->nfc_cfg0, NFC_CFG0_REG);
	}
	sprd_nand_disable_randomizer(host, page, mode);

	if (ret) {
		*ret_num = 0;
		sprd_nand_delect_cs(host, -EIO);
		return -EIO;
	}
	*ret_num = num;

	switch (mode) {
	case MTD_OPS_AUTO_OOB:
	case MTD_OPS_PLACE_OOB:
		ret = sprd_nand_ecc_analyze(host,
					    num * host->param.nsect_per_page,
					    ecc_sts, mode);
		if (ret)
			pr_err("nand:page %d, ecc error ret %d\n", page, ret);
		break;
	case MTD_OPS_RAW:
		break;
	default:
		dev_err(host->dev, "nand:sprd nand ops mode error!\n");
		break;
	}
	sprd_nand_delect_cs(host, ret);
	return ret;
}

static int sprd_nand_read_page_retry(struct sprd_nfc_base *nfc_base,
				     u32 page, u32 num,
				     u32 *ret_num, u32 if_has_mbuf,
				     u32 if_has_sbuf, u32 mode,
				     struct mtd_ecc_stats *ecc_sts)
{
	int ret, i = 0;

	do {
		ret = sprd_nand_read_page_in_block(nfc_base, page, num, ret_num,
						   if_has_mbuf, if_has_sbuf,
						   mode, ecc_sts);
		if (!ret || ret == -EINVAL || ret == -EUCLEAN)
			break;
		i++;
		pr_err("sprd_nand:retry %d\n", i);
	} while (i < 3);

	return 0;
}

static void sprd_nand_disable_wp(struct sprd_nand_host *host)
{
	u32 reg_nfc_cfg0;

	reg_nfc_cfg0 = sprd_nand_readl(host, NFC_CFG0_REG);
	reg_nfc_cfg0 |= CFG0_SET_WPN;
	sprd_nand_writel(host, reg_nfc_cfg0, NFC_CFG0_REG);
	/* need delay time after set NFC_CFG0_REG */
	udelay(1);
}

static void sprd_nand_enable_wp(struct sprd_nand_host *host)
{
	u32 reg_nfc_cfg0;

	reg_nfc_cfg0 = sprd_nand_readl(host, NFC_CFG0_REG);
	reg_nfc_cfg0 &= ~CFG0_SET_WPN;
	sprd_nand_writel(host, reg_nfc_cfg0, NFC_CFG0_REG);
	/* need delay time after set NFC_CFG0_REG */
	udelay(1);
}

static int sprd_nand_write_page_in_blk(struct sprd_nfc_base *nfc_base,
				       u32 page, u32 num,
				       u32 *ret_num, u32 if_has_mbuf,
				       u32 if_has_sbuf, u32 mode)
{
	struct nand_inst *inst = NULL;
	int ret;
	u32 if_change_buf = 0;
	struct sprd_nand_host *host = (struct sprd_nand_host *)nfc_base->priv;
	struct nand_inst inst_write_main_spare = host->inst_write_main_spare;
	struct nand_inst inst_write_spare_raw = host->inst_write_spare_raw;
	struct nand_inst inst_write_main_raw = host->inst_write_main_raw;

	sprd_nand_select_cs(host, GETCS(page));
	sprd_nand_disable_wp(host);
	host->nfc_cfg0 |= CFG0_SET_WPN;

	host->nfc_sts_mach_val = MACH_WRITE;
	host->nfc_time0_val = host->nfc_time0_w;
	host->nfc_cfg0_val =
		(host->nfc_cfg0 | CFG0_SET_NFC_MODE(0) | CFG0_SET_NFC_RW);
	sprd_nand_enable_randomizer(host, page, mode);

	switch (mode) {
	case MTD_OPS_AUTO_OOB:
		host->nfc_cfg0_val |= CFG0_SET_SPARE_ONLY_INFO_PROD_EN;
	case MTD_OPS_PLACE_OOB:
		host->nfc_cfg0_val |= (CFG0_SET_ECC_EN | CFG0_SET_MAIN_USE |
				       CFG0_SET_SPAR_USE);
		inst = &inst_write_main_spare;
		break;
	case MTD_OPS_RAW:
		if (if_has_mbuf & if_has_sbuf) {
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE |
					       CFG0_SET_SPAR_USE);
			inst = &inst_write_main_spare;
		} else if (if_has_mbuf) {
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE);
			inst = &inst_write_main_raw;
		} else if (if_has_sbuf) {
			/* use main to write spare area */
			host->nfc_cfg0_val |= (CFG0_SET_MAIN_USE);
			inst = &inst_write_spare_raw;
			/*
			 * 0 nand controller use mainAddr to send spare data,
			 * So i have to change some globe config here
			 * if_change_buf = 1;
			 * 1 change to main buf
			 */
			if_change_buf = 1;
			sprd_nand_writel(host, 0x0, NFC_MAIN_ADDRH_REG);
			sprd_nand_writel(host, RAM_MAIN_ADDR(host->sbuf_p),
					 NFC_MAIN_ADDRL_REG);
			/* 2 change page_size */
			host->nfc_cfg1 &= (~CFG1_temp_MIAN_SIZE_MSK);
			host->nfc_cfg1 |=
				CFG1_DEF_MAIN_SIZE(host->param.nspare_size <<
						   host->sect_perpage_shift);
			sprd_nand_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
			/* 3 change sect number */
			host->nfc_cfg0_val &= (~CFG0_SET_SECT_NUM_MSK);
			host->nfc_cfg0_val |= CFG0_DEF1_SECT_NUM(1);
		} else {
			sprd_nand_enable_wp(host);
			host->nfc_cfg0 &= ~(CFG0_SET_WPN);
			sprd_nand_delect_cs(host, -EINVAL);
			return -EINVAL;
		}
		break;
	default:
		pr_err("nand:sprd nand ops mode error!\n");
		break;
	}

	sprd_nand_cmd_change(host, inst, page);
	sprd_nand_cmd_exec(host, inst, num, 0);
	ret = sprd_nand_cmd_wait(host, inst);
	if (if_change_buf) {
		/* 1 change to main buf */
		sprd_nand_writel(host, 0x0, NFC_MAIN_ADDRH_REG);
		sprd_nand_writel(host, RAM_MAIN_ADDR(host->mbuf_p),
				 NFC_MAIN_ADDRL_REG);
		/* 2 change page_size */
		host->nfc_cfg1 &= (~CFG1_temp_MIAN_SIZE_MSK);
		host->nfc_cfg1 |= CFG1_DEF_MAIN_SIZE(host->param.nsect_size);
		sprd_nand_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
		/* 3 change sect number */
		sprd_nand_writel(host, host->nfc_cfg0, NFC_CFG0_REG);
	}
	sprd_nand_disable_randomizer(host, page, mode);

	if (ret) {
		*ret_num = 0;
		sprd_nand_enable_wp(host);
		host->nfc_cfg0 &= ~(CFG0_SET_WPN);
		sprd_nand_delect_cs(host, -EIO);
		return ret;
	}
	*ret_num = num;
	sprd_nand_enable_wp(host);
	host->nfc_cfg0 &= ~(CFG0_SET_WPN);
	sprd_nand_delect_cs(host, 0);

	return 0;
}

static int sprd_nand_eraseblk(struct sprd_nfc_base *nfc_base, u32 page)
{
	int ret;
	struct sprd_nand_host *host = (struct sprd_nand_host *)nfc_base->priv;
	struct nand_inst inst_erase_blk = host->inst_erase;
	struct nand_inst *inst = &inst_erase_blk;

	sprd_nand_select_cs(host, GETCS(page));
	sprd_nand_disable_wp(host);
	host->nfc_cfg0 |= CFG0_SET_WPN;

	host->nfc_sts_mach_val = MACH_ERASE;
	host->nfc_time0_val = host->nfc_time0_e;
	host->nfc_cfg0_val = (host->nfc_cfg0 | CFG0_SET_NFC_MODE(2));

	sprd_nand_cmd_change(host, inst, page);
	sprd_nand_cmd_exec(host, inst, 1, 0);
	ret = sprd_nand_cmd_wait(host, inst);

	sprd_nand_enable_wp(host);
	host->nfc_cfg0 &= ~(CFG0_SET_WPN);
	sprd_nand_delect_cs(host, ret);

	return ret;
}

static int sprd_nand_check_badflag(u8 *flag, u32 len)
{
	/* caculate zero bit number; */
	u32 i, k, num = 0;

	for (i = 0; i < len; i++) {
		for (k = 0; k < 8; k++) {
			if (flag[i] & BIT(k))
				num++;
		}
	}

	return (num < (len << 2));
}

int sprd_nand_check_badblk(struct sprd_nfc_base *nfc_base, u32 page)
{
	int ret;
	u32 ret_num;
	struct sprd_nand_host *host = (struct sprd_nand_host *)nfc_base->priv;

	ret = nfc_base->read_page_in_blk(nfc_base, page, 1, &ret_num, 0, 1,
					 MTD_OPS_RAW, 0);
	if (ret)
		return ret;

	return sprd_nand_check_badflag(host->sbuf_v + host->param.badflag_pos,
				       host->param.badflag_len);
}

int sprd_nand_mark_badblk(struct sprd_nfc_base *nfc_base, u32 page)
{
	u32 ret_num;
	struct sprd_nand_host *host = (struct sprd_nand_host *)nfc_base->priv;

	memset(host->sbuf_v, 0xFF,
	       host->param.nspare_size * host->param.nsect_per_page);
	memset(host->sbuf_v + host->param.badflag_pos, 0,
	       host->param.badflag_len);
	return nfc_base->write_page_in_blk(nfc_base, page, 1, &ret_num, 0, 1,
					  MTD_OPS_RAW);
}

static void sprd_nand_clk_disable(struct sprd_nand_host *host)
{
	clk_disable_unprepare(host->clk_ecc);
	clk_disable_unprepare(host->clk_nand);
	clk_disable_unprepare(host->clk_nand_2x);
	clk_disable_unprepare(host->clk_nand_1x);
	clk_disable_unprepare(host->clk_ahb_26m);
	clk_disable_unprepare(host->clk_ahb_ecc);
	clk_disable_unprepare(host->clk_ahb_enable_gate);
}

static int sprd_nand_ctrl_en(struct sprd_nfc_base *nfc_base, u32 en)
{
	struct sprd_nand_host *host = (struct sprd_nand_host *)nfc_base->priv;
	int ret = 0;

	if (!en) {
		sprd_nand_clk_disable(host);
		return ret;
	}

	ret = clk_prepare_enable(host->clk_ahb_enable_gate);
	if (ret)
		return ret;
	ret = clk_prepare_enable(host->clk_ahb_ecc);
	if (ret)
		goto err_ahb_ecc_clk;
	ret = clk_prepare_enable(host->clk_ahb_26m);
	if (ret)
		goto err_ahb_26m_clk;
	ret = clk_prepare_enable(host->clk_nand_1x);
	if (ret)
		goto err_nand_1x_clk;
	ret = clk_prepare_enable(host->clk_nand_2x);
	if (ret)
		goto err_nand_2x_clk;
	clk_set_parent(host->clk_nand, host->clk_parent_sdr);
	clk_set_parent(host->clk_ecc, host->clk_parent_ddr);
	ret = clk_prepare_enable(host->clk_nand);
	if (ret)
		goto err_nand_clk;
	ret = clk_prepare_enable(host->clk_ecc);
	if (ret)
		goto err_ecc_clk;

	return 0;

err_ecc_clk:
	clk_disable_unprepare(host->clk_nand);
err_nand_clk:
	clk_disable_unprepare(host->clk_nand_2x);
err_nand_2x_clk:
	clk_disable_unprepare(host->clk_nand_1x);
err_nand_1x_clk:
	clk_disable_unprepare(host->clk_ahb_26m);
err_ahb_26m_clk:
	clk_disable_unprepare(host->clk_ahb_ecc);
err_ahb_ecc_clk:
	clk_disable_unprepare(host->clk_ahb_enable_gate);

	return ret;
}

static void sprd_nand_set_mode(struct sprd_nand_host *host, u8 infi_type,
			       u32 delay)
{
	host->nfc_cfg1 &= ~(CFG1_INTF_TYPE(7));
	host->nfc_cfg1 |= CFG1_INTF_TYPE(infi_type);
	sprd_nand_writel(host, host->nfc_cfg1, NFC_CFG1_REG);
	sprd_nand_writel(host, delay, NFC_DLL0_CFG);
	sprd_nand_writel(host, delay, NFC_DLL1_CFG);
	sprd_nand_writel(host, delay, NFC_DLL2_CFG);
}

/* clock issues is handled by sprd_nfc_get_device & sprd_nfc_put_device
 * in nfc_base.c file.
 * when do nandc operateor such as erase write & read,
 * so we make suspend dummy here.
 */
static void sprd_nand_suspend(struct sprd_nfc_base *host) {}

static int sprd_nand_resume(struct sprd_nfc_base *nfc_base)
{
	struct sprd_nand_host *host = (struct sprd_nand_host *)nfc_base->priv;
	int i, ret;

	ret = sprd_nand_ctrl_en(nfc_base, 1);
	if (ret) {
		pr_err("nand resume enable controller clock fail\n");
		return ret;
	}
	sprd_nand_set_mode(host, 0, 0);
	sprd_nand_init_reg_state0(host);

	for (i = 0; i < host->csnum; i++) {
		pr_info("try to probe flash%d\n", i);
		sprd_nand_select_cs(host, i);
		ret = sprd_nand_reset(host);
		if (ret) {
			sprd_nand_writel(host, CTRL_NFC_CMD_CLR, NFC_START_REG);
			pr_err("flash%d reset fail\n", i);
			break;
		}
		sprd_nand_delect_cs(host, ret);
	}
	sprd_nand_init_reg_state1(host);
	ret = sprd_nand_ctrl_en(nfc_base, 0);
	if (ret) {
		pr_err("nand resume disable controller clock fail\n");
		return ret;
	}

	return 0;
}

static void sprd_nfc_base_init(struct sprd_nfc_base *base,
			       struct sprd_nand_host *host)
{
	memcpy(base->id, host->param.id, NFC_MAX_ID_LEN);

	base->dev = host->dev;
	base->ecc_mode = host->param.ecc_mode;
	base->risk_threshold = host->risk_threshold;
	base->obb_size = host->param.nspare_size
		<< (host->pageshift - host->sectshift);
	base->eccbytes = host->param.ecc_size <<
		(host->pageshift - host->sectshift);
	base->oobavail = host->param.info_size <<
		(host->pageshift - host->sectshift);
	base->pageshift = host->pageshift;
	base->blkshift = host->blkshift;
	base->chip_shift = ffs(host->param.nblkcnt << host->blkshift) - 1;
	base->chip_num = host->csnum;
	base->bufshift = host->bufshift;
	base->ctrl_en = sprd_nand_ctrl_en;
	base->ctrl_suspend = sprd_nand_suspend;
	base->ctrl_resume = sprd_nand_resume;
	base->read_page_in_blk = sprd_nand_read_page_retry;
	base->write_page_in_blk = sprd_nand_write_page_in_blk;
	base->nand_erase_blk = sprd_nand_eraseblk;
	base->nand_is_bad_blk = sprd_nand_check_badblk;
	base->nand_mark_bad_blk = sprd_nand_mark_badblk;
	base->mbuf_v = host->mbuf_v;
	base->sbuf_v = host->sbuf_v;
	base->debugfs_drv_show = sprd_nand_debugfs_show;
	base->priv = host;
	dev_dbg(host->dev,
		"nand: base->risk_threshold [%d]\n", base->risk_threshold);
	dev_dbg(host->dev, "nand: base->obb_size[%d]\n", base->obb_size);
	dev_dbg(host->dev, "nand: base->eccbytes [%d]\n", base->eccbytes);
	dev_dbg(host->dev, "nand: base->oobavail [%d]\n", base->oobavail);
	dev_dbg(host->dev, "nand: base->pageshift [%d]\n", base->pageshift);
	dev_dbg(host->dev, "nand: base->blkshift [%d]\n", base->blkshift);
	dev_dbg(host->dev, "nand: base->chip_shift[%d]\n", base->chip_shift);
	dev_dbg(host->dev, "nand: base->chip_num [%d]\n", base->chip_num);
	dev_dbg(host->dev, "nand: base->bufshift [%d]\n", base->bufshift);
}

static int sprd_nand_parse_dt(struct platform_device *pdev,
			      struct sprd_nand_host *host)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->ioaddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(host->ioaddr))
		return PTR_ERR(host->ioaddr);

	host->clk_nand = devm_clk_get(dev, "nandc_clk");
	if (IS_ERR(host->clk_nand))
		return PTR_ERR(host->clk_nand);
	host->clk_ecc = devm_clk_get(dev, "nandc_ecc_clk");
	if (IS_ERR(host->clk_ecc))
		return PTR_ERR(host->clk_ecc);
	host->clk_parent_sdr = devm_clk_get(dev, "nandc_parent_sdr");
	if (IS_ERR(host->clk_parent_sdr))
		return PTR_ERR(host->clk_parent_sdr);
	host->clk_parent_ddr = devm_clk_get(dev, "nandc_parent_ddr");
	if (IS_ERR(host->clk_parent_ddr))
		return PTR_ERR(host->clk_parent_ddr);
	host->clk_ahb_enable_gate = devm_clk_get(dev, "nandc_ahb_enable");
	if (IS_ERR(host->clk_ahb_enable_gate))
		return PTR_ERR(host->clk_ahb_enable_gate);
	host->clk_ahb_ecc = devm_clk_get(dev, "nandc_ecc_enable");
	if (IS_ERR(host->clk_ahb_ecc))
		return PTR_ERR(host->clk_ahb_ecc);
	host->clk_ahb_26m = devm_clk_get(dev, "nandc_26m_enable");
	if (IS_ERR(host->clk_ahb_26m))
		return PTR_ERR(host->clk_ahb_26m);
	host->clk_nand_1x = devm_clk_get(dev, "nandc_1x_enable");
	if (IS_ERR(host->clk_nand_1x))
		return PTR_ERR(host->clk_nand_1x);
	host->clk_nand_2x = devm_clk_get(dev, "nandc_2x_enable");
	if (IS_ERR(host->clk_nand_2x))
		return PTR_ERR(host->clk_nand_2x);

	ret = clk_prepare_enable(host->clk_ahb_enable_gate);
	if (ret)
		return ret;
	ret = clk_prepare_enable(host->clk_ahb_ecc);
	if (ret)
		goto err_ahb_ecc_clk;
	ret = clk_prepare_enable(host->clk_ahb_26m);
	if (ret)
		goto err_ahb_26m_clk;
	ret = clk_prepare_enable(host->clk_nand_1x);
	if (ret)
		goto err_nand_1x_clk;
	ret = clk_prepare_enable(host->clk_nand_2x);
	if (ret)
		goto err_nand_2x_clk;
	clk_set_parent(host->clk_nand, host->clk_parent_sdr);
	host->frequence = clk_get_rate(host->clk_parent_sdr);
	dev_dbg(&pdev->dev, "nand:sprd nand freq = %d\n", host->frequence);
	ret = clk_prepare_enable(host->clk_nand);
	if (ret)
		goto err_nand_clk;
	clk_set_parent(host->clk_ecc, host->clk_parent_ddr);
	ret = clk_prepare_enable(host->clk_ecc);
	if (ret)
		goto err_ecc_clk;

	return 0;

err_ecc_clk:
	clk_disable_unprepare(host->clk_nand);
err_nand_clk:
	clk_disable_unprepare(host->clk_nand_2x);
err_nand_2x_clk:
	clk_disable_unprepare(host->clk_nand_1x);
err_nand_1x_clk:
	clk_disable_unprepare(host->clk_ahb_26m);
err_ahb_26m_clk:
	clk_disable_unprepare(host->clk_ahb_ecc);
err_ahb_ecc_clk:
	clk_disable_unprepare(host->clk_ahb_enable_gate);

	return ret;
}

static int sprd_nand_test_is_badblock(struct sprd_nfc_base *nfc_base,
				      struct sprd_nand_host *host,
				      u16 block, u16 page)
{
	u32 testpage;
	int ret;

	testpage = (block << 6) + page;
	ret = sprd_nand_check_badblk(nfc_base, testpage);
	if (ret) {
		host->badblkcnt++;
		dev_dbg(host->dev, "nand:block [%d] is bad.\n", block);
	}

	return ret;
}

static void sprd_nand_test_scan_badblk(struct sprd_nfc_base *nfcbase,
				       struct sprd_nand_host *host)
{
	int i, cnt = host->param.nblkcnt;

	dev_dbg(host->dev,
		"nand:scan the %d blocks to find bad block.\n", cnt);
	for (i = 0; i < cnt; i++)
		sprd_nand_test_is_badblock(nfcbase, host, i, 0);
}

static int sprd_nand_drv_probe(struct platform_device *pdev)
{
	struct sprd_nand_host *host;
	struct sprd_nfc_base *nfc_base;
	u32 i;
	int ret;

	host = devm_kzalloc(&pdev->dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	nfc_base = devm_kzalloc(&pdev->dev, sizeof(*nfc_base), GFP_KERNEL);
	if (!nfc_base)
		return -ENOMEM;

	ret = sprd_nand_parse_dt(pdev, host);
	if (ret)
		return ret;

	host->dev = &pdev->dev;
	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0) {
		ret = host->irq;
		goto err_clk_disable;
	}

	host->randomizer = of_property_read_bool(pdev->dev.of_node,
						 "sprd,random-mode");

	sprd_nand_set_mode(host, 0, 0);
	sprd_nand_init_reg_state0(host);

	sprd_nand_set_timing_config(host, &default_timing, host->frequence);
	host->csnum = 1;
	host->cs[0] = 0;
	sprd_nand_select_cs(host, 0);
	ret = sprd_nand_reset(host);
	if (ret) {
		dev_err(&pdev->dev, "nand_drv_probe  flash0 reset fail\n");
		sprd_nand_delect_cs(host, ret);
		goto err_clk_disable;
	}
	ret = sprd_nand_readid(host);
	if (ret) {
		dev_err(&pdev->dev,
			"nand_drv_probe  flash0 sprd_nand_readid fail\n");
		sprd_nand_delect_cs(host, ret);
		goto err_clk_disable;
	}
	sprd_nand_delect_cs(host, ret);

	/*probably only flash0 used*/
	for (i = 1; i < CFG0_CS_MAX; i++) {
		dev_dbg(&pdev->dev, "try to probe flash%d\n", i);
		sprd_nand_select_cs(host, i);
		ret = sprd_nand_reset(host);
		if (ret) {
			sprd_nand_writel(host, CTRL_NFC_CMD_CLR, NFC_START_REG);
			dev_warn(&pdev->dev, "flash%d reset fail\n", i);
			break;
		}
		ret = sprd_nand_readid(host);
		if (ret) {
			dev_warn(&pdev->dev,
				 "flash%d sprd_nand_readid fail\n", i);
			sprd_nand_delect_cs(host, ret);
			break;
		}
		dev_dbg(&pdev->dev, "find flash%d,id[] is %x %x %x %x %x\n",
			i, host->param.id[0], host->param.id[1],
			host->param.id[2], host->param.id[3],
			host->param.id[4]);
		host->csnum = host->csnum + 1;
		host->cs[i] = i;
		sprd_nand_delect_cs(host, ret);
	}

	ret = sprd_nand_param_init(host);
	if (ret)
		goto err_clk_disable;

	sprd_nand_init_reg_state1(host);
	sprd_nfc_base_init(nfc_base, host);
	sprd_nand_test_scan_badblk(nfc_base, host);
	dev_dbg(&pdev->dev,
		"nand: device has [%d] bad blocks.\n", host->badblkcnt);

	platform_set_drvdata(pdev, host);
	ret = sprd_nfc_base_register(nfc_base);
	if (ret)
		goto err_release_dma_buf;
	sprd_nand_clk_disable(host);

	return 0;

err_release_dma_buf:
	sprd_nand_free_dma_buf(host);
err_clk_disable:
	sprd_nand_clk_disable(host);
	dev_err(&pdev->dev, "sprd_nand get basic resource fail\n");

	return ret;
}

static int sprd_nand_drv_remove(struct platform_device *pdev)
{
	struct sprd_nand_host *host = platform_get_drvdata(pdev);

	sprd_nand_free_dma_buf(host);
	sprd_nand_clk_disable(host);

	return 0;
}

static const struct of_device_id sprd_nand_of_match[] = {
	{.compatible = "sprd,orca-nandc"},
	{ }
};

MODULE_DEVICE_TABLE(of, sprd_nand_of_match);
static struct platform_driver sprd_nand_driver = {
	.probe = sprd_nand_drv_probe,
	.remove = sprd_nand_drv_remove,
	.driver = {
		.name = "sprd-nand",
		.of_match_table = sprd_nand_of_match,
	},
};

module_platform_driver(sprd_nand_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SPRD MTD NAND driver");
