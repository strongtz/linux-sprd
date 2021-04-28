/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/sprd_dfs_drv.h>

#define MASTER_NULL   0x0
#define MASTER_DPU    0x1
#define MASTER_DCAM   0x2
#define MASTER_PUBCP  0x3
#define MASTER_WTLCP  0x4
#define MASTER_WTLCP1 0x5
#define MASTER_AGCP   0x6
#define MASTER_SW     0x7
#define MASTER_VDSP   0x8


#define TYPE_FREQ   0x0
#define TYPE_BW     0x1


struct vote_data {
	unsigned int master;
	unsigned int vote_reg;
	unsigned int freq_bit;
	unsigned int freq_mask;
	unsigned int bw_bit;
	unsigned int bw_mask;
	unsigned int freq_en_bit;
	unsigned int bw_en_bit;
	unsigned int reinit_val;
};

static	phys_addr_t base_addr = 0x31056400;
static struct vote_data vote_data[] =  {
	{MASTER_DPU, 0x0, 16, 0x7, 0, 0xffff, 25, 25, 0},
	{MASTER_DCAM, 0x4, 16, 0x7, 0, 0xffff, 25, 25, 0},
	{MASTER_PUBCP, 0x8, 16, 0x7, 0, 0xffff, 24, 25, 0},
	{MASTER_WTLCP, 0xc, 16, 0x7, 0, 0xffff, 24, 25, 0},
	{MASTER_WTLCP1, 0x10, 16, 0x7, 0, 0xffff, 24, 25, 0},
	{MASTER_AGCP, 0x14, 16, 0x7, 0, 0xffff, 24, 25, 0},
	{MASTER_SW, 0x18, 16, 0x7, 0, 0xffff, 24, 25, 0},
	{MASTER_SW, 0x1c, 16, 0x7, 0, 0xffff, 24, 25, 0},
};

static unsigned int freq_table[8] = {
	256, 384, 512, 768, 1024, 1333, 1536, 1866
};

static unsigned int freq_to_sel(unsigned int freq)
{
	unsigned int i;

	for (i = 0; i < 8; i++) {
		if (freq < freq_table[i])
			break;
	}
	return i;
}

int dfs_ext_vote(unsigned int freq, unsigned int magic)
{
	static DEFINE_SPINLOCK(lock);
	unsigned int master;
	unsigned int type;
	unsigned int temp;
	void __iomem *base;
	struct vote_data *master_data;

	master = magic & 0xff;
	type = (magic >> 8) & 0xf;


	if ((magic < MASTER_DPU) || (magic > MASTER_SW))
		return -EINVAL;

	master_data = &vote_data[master-1];

	base = ioremap(base_addr, 256);
	if (!base)
		return -ENOMEM;

	spin_lock(&lock);

	temp = readl_relaxed(base + master_data->vote_reg);
	temp &= ~(1<<master_data->freq_en_bit);
	temp &= ~(1<<master_data->bw_en_bit);
	temp |= 0xc0000000;
	writel_relaxed(temp, base + master_data->vote_reg);

	if (type == TYPE_FREQ) {
		temp = readl_relaxed(base + master_data->vote_reg);
		temp &= ~(master_data->freq_mask<<master_data->freq_bit);
		temp |= freq_to_sel(freq)<<master_data->freq_bit;
		writel_relaxed(temp, base + master_data->vote_reg);
	} else if (type == TYPE_BW) {
		temp = readl_relaxed(base + master_data->vote_reg);
		temp &= ~(master_data->freq_mask<<master_data->freq_bit);
		temp |= freq<<master_data->freq_bit;
		temp |= 1<<master_data->bw_en_bit;
		writel_relaxed(temp, base + master_data->vote_reg);
	}


	temp = readl_relaxed(base + master_data->vote_reg);
	temp |= 1<<master_data->freq_en_bit;
	writel_relaxed(temp, base + master_data->vote_reg);

	master_data->reinit_val = temp;

	spin_unlock(&lock);
	iounmap(base);
	return 0;
}

void dfs_ext_vote_resume(void)
{
	unsigned int i;
	struct vote_data *master_data;
	void __iomem *base;

	base = ioremap(base_addr, 256);
	if (!base)
		return;
	for (i = MASTER_DPU; i <= MASTER_VDSP; i++) {
		master_data = &vote_data[i-1];
		if (master_data->reinit_val == 0)
			continue;
		writel_relaxed(master_data->reinit_val,
				base + master_data->vote_reg);
	}
	iounmap(base);
}
