/*copyright (C) 2019 Spreadtrum Communications Inc.
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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/math64.h>

#define PUB_MONITOR_CLK		128	/* 128MHz */
#define PUB_DFS_MONITOR_CLK	65	/* 6.5MHz */
#define PUB_STATUS_MON_CTRL_OFFSET	0x6200
#define DMC_DDR_CLK_CTRL_OFFSET	0x4000
#define PUB_CLK_DMC_REF_EB	22
#define PUB_CLK_DFS_EB	15
#define DDR_MAX_SUPPORT_CS_NUM		2
#define DMC_PROC_NAME "sprd_dmc"
#define DDR_PROPERTY_NAME "property"
#define DDR_INFO_NAME "ddr_info"
#define INVALID_RES_IDX  0xffffffff
/*
 * new requirement: all register value is clear to 0,
 * when enable signle change from 0 to 1;
 */
struct pub_monitor_dbg {
	u32 pub_status_mon_ctrl;
	u32 idle_time;		/* clk 128M */
	u32 write_time;		/* clk 128M */
	u32 read_time;		/* clk 128M */
	u32 sref_time;		/* clk 128M */
	u32 light_time;		/* clk 6.5M */
	u32 st_ls_cnt;
	u32 fx_time[8];
	u32 dfs_cnt;
};

struct dmc_data {
	u32 proc_res;
	u32 mon_res;
	u32 size_l_offset;
	u32 size_h_offset;
	u32 type_offset;
	u32 mr_offset[DDR_MAX_SUPPORT_CS_NUM];
};

struct dmc_drv_data {
	struct pub_monitor_dbg reg_val;
	struct proc_dir_entry *proc_dir;
	struct dentry *monitor_dir;
	struct proc_dir_entry *property;
	struct proc_dir_entry *info;
	void __iomem *mon_base;
	int pub_mon_enabled;
	u32 type;
	u32 mr_val[DDR_MAX_SUPPORT_CS_NUM];
	u32 reg_clk_ctrl;
	u64 size;
};
#define PIKE2_SIZE_L_OFFSET	0x1b4
#define PIKE2_SIZE_H_OFFSET	0x1b8
#define PIKE2_TYPE_OFFSET	0x1bc
#define PIKE2_CS0_MR_OFFSET	0x1c0
#define PIKE2_CS1_MR_OFFSET	0x1c4

#define SHARKLE_SIZE_L_OFFSET	0x1b4
#define SHARKLE_SIZE_H_OFFSET	0x1b8
#define SHARKLE_TYPE_OFFSET	0x1bc
#define SHARKLE_CS0_MR_OFFSET	0x1c0
#define SHARKLE_CS1_MR_OFFSET	0x1c4

#define SHARKL3_SIZE_L_OFFSET	0x1c4
#define SHARKL3_SIZE_H_OFFSET	0x1c8
#define SHARKL3_TYPE_OFFSET	0x1b8
#define SHARKL3_CS0_MR_OFFSET	0x1b0
#define SHARKL3_CS1_MR_OFFSET	0x1b4

#define SHARKL5_SIZE_L_OFFSET	0x0
#define SHARKL5_SIZE_H_OFFSET	0x4
#define SHARKL5_TYPE_OFFSET	0x8
#define SHARKL5_CS0_MR_OFFSET	0xc
#define SHARKL5_CS1_MR_OFFSET	0x10

#define SHARKL5PRO_SIZE_L_OFFSET 0x0
#define SHARKL5PRO_SIZE_H_OFFSET 0x4
#define SHARKL5PRO_TYPE_OFFSET	 0x8
#define SHARKL5PRO_CS0_MR_OFFSET 0xc
#define SHARKL5PRO_CS1_MR_OFFSET 0x10

#define ROC1_SIZE_L_OFFSET	0x0
#define ROC1_SIZE_H_OFFSET	0x4
#define ROC1_TYPE_OFFSET	0x8
#define ROC1_CS0_MR_OFFSET	0xc
#define ROC1_CS1_MR_OFFSET	0x10

#define ORCA_SIZE_L_OFFSET	0x0
#define ORCA_SIZE_H_OFFSET	0x4
#define ORCA_TYPE_OFFSET	0x8
#define ORCA_CS0_MR_OFFSET	0xc
#define ORCA_CS1_MR_OFFSET	0x10

static const struct dmc_data pike2_data = {
	.proc_res = 0,
	.mon_res = INVALID_RES_IDX,
	.size_l_offset = PIKE2_SIZE_L_OFFSET,
	.size_h_offset = PIKE2_SIZE_H_OFFSET,
	.type_offset = PIKE2_TYPE_OFFSET,
	.mr_offset = {
		PIKE2_CS0_MR_OFFSET,
		PIKE2_CS1_MR_OFFSET,
	},
};

static const struct dmc_data sharkle_data = {
	.proc_res = 0,
	.mon_res = INVALID_RES_IDX,
	.size_l_offset = SHARKLE_SIZE_L_OFFSET,
	.size_h_offset = SHARKLE_SIZE_H_OFFSET,
	.type_offset = SHARKLE_TYPE_OFFSET,
	.mr_offset = {
		SHARKLE_CS0_MR_OFFSET,
		SHARKLE_CS1_MR_OFFSET,
	},
};

static const struct dmc_data sharkl3_data = {
	.proc_res = 0,
	.mon_res = INVALID_RES_IDX,
	.size_l_offset = SHARKL3_SIZE_L_OFFSET,
	.size_h_offset = SHARKL3_SIZE_H_OFFSET,
	.type_offset = SHARKL3_TYPE_OFFSET,
	.mr_offset = {
		SHARKL3_CS0_MR_OFFSET,
		SHARKL3_CS1_MR_OFFSET,
	},
};

static const struct dmc_data sharkl5_data = {
	.proc_res = 1,
	.mon_res = 0,
	.size_l_offset = SHARKL5_SIZE_L_OFFSET,
	.size_h_offset = SHARKL5_SIZE_H_OFFSET,
	.type_offset = SHARKL5_TYPE_OFFSET,
	.mr_offset = {
		SHARKL5_CS0_MR_OFFSET,
		SHARKL5_CS1_MR_OFFSET,
	},
};

static const struct dmc_data sharkl5pro_data = {
	.proc_res = 1,
	.mon_res = 0,
	.size_l_offset = SHARKL5PRO_SIZE_L_OFFSET,
	.size_h_offset = SHARKL5PRO_SIZE_H_OFFSET,
	.type_offset = SHARKL5PRO_TYPE_OFFSET,
	.mr_offset = {
		SHARKL5PRO_CS0_MR_OFFSET,
		SHARKL5PRO_CS1_MR_OFFSET,
	},
};
static const struct dmc_data roc1_data = {
	.proc_res = 1,
	.mon_res = 0,
	.size_l_offset = ROC1_SIZE_L_OFFSET,
	.size_h_offset = ROC1_SIZE_H_OFFSET,
	.type_offset = ROC1_TYPE_OFFSET,
	.mr_offset = {
		ROC1_CS0_MR_OFFSET,
		ROC1_CS1_MR_OFFSET,
	},
};
static const struct dmc_data orca_data = {
	.proc_res = 1,
	.mon_res = 0,
	.size_l_offset = ORCA_SIZE_L_OFFSET,
	.size_h_offset = ORCA_SIZE_H_OFFSET,
	.type_offset = ORCA_TYPE_OFFSET,
	.mr_offset = {
		ORCA_CS0_MR_OFFSET,
		ORCA_CS1_MR_OFFSET,
	},
};

static struct dmc_drv_data drv_data;

static const char *const ddr_type_to_str[] = {
	"Invalid",
	"LPDDR1",
	"LPDDR2",
	"LPDDR3",
	"LPDDR4",
	"LPDDR4X",
	"LPDDR4Y",
	"LPDDR5",
};

#ifdef CONFIG_PROC_FS
static int sprd_ddr_size_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%llu\n", (long long int)(drv_data.size / 1024 / 1024));
	return 0;
}

static int sprd_ddr_size_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_ddr_size_show, PDE_DATA(inode));
}

static const struct file_operations sprd_ddr_size_fops = {
	.open = sprd_ddr_size_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sprd_ddr_info_show(struct seq_file *m, void *v)
{
	int cs_num, i, type;

	type = (drv_data.type & 0x0f);
	if (type < ARRAY_SIZE(ddr_type_to_str))
		seq_printf(m, "%s\n", ddr_type_to_str[type]);
	else
		return -EINVAL;

	cs_num = (drv_data.type >> 4) & 0x0f;
	if (cs_num > DDR_MAX_SUPPORT_CS_NUM)
		cs_num = DDR_MAX_SUPPORT_CS_NUM;
	seq_printf(m, "CS_NUM:%d\n", cs_num);

	for (i = 0; i < cs_num; i++) {
		seq_printf(m,
			   "CS%d MR Value: MR5=0x%x,MR6=0x%x, MR7=0x%x,MR8=0x%x\n",
			   i, (drv_data.mr_val[i] & 0xff),
			   ((drv_data.mr_val[i] >> 8) & 0xff),
			   ((drv_data.mr_val[i] >> 16) & 0xff),
			   ((drv_data.mr_val[i] >> 24) & 0xff));
	}
	return 0;
}

static int sprd_ddr_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_ddr_info_show, PDE_DATA(inode));
}

static const struct file_operations sprd_ddr_info_fops = {
	.open = sprd_ddr_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sprd_ddr_proc_creat(void)
{
	drv_data.proc_dir = proc_mkdir(DMC_PROC_NAME, NULL);
	if (!drv_data.proc_dir)
		return -ENOMEM;

	drv_data.property = proc_create_data(DDR_PROPERTY_NAME, 0444,
					     drv_data.proc_dir,
					     &sprd_ddr_size_fops, NULL);
	if (!drv_data.property) {
		remove_proc_entry(DMC_PROC_NAME, NULL);
		return -ENOMEM;
	}
	drv_data.info = proc_create_data(DDR_INFO_NAME, 0444,
					 drv_data.proc_dir, &sprd_ddr_info_fops,
					 NULL);
	if (!drv_data.info) {
		remove_proc_entry(DDR_PROPERTY_NAME, drv_data.proc_dir);
		remove_proc_entry(DMC_PROC_NAME, NULL);
		return -ENOMEM;
	}
	return 0;
}
#endif

static int sprd_pub_monitor_reg_get(void)
{
	u32 *ptr = (u32 *) &drv_data.reg_val;
	int i;

	for (i = 0; i < sizeof(drv_data.reg_val) / sizeof(u32); i++) {
		*ptr = readl_relaxed(drv_data.mon_base
			+ PUB_STATUS_MON_CTRL_OFFSET + i * 4);
		ptr++;
	}
	return 0;
}

static int sprd_pub_monitor_enable(int enable)
{
	if (!drv_data.mon_base)
		return -ENOMEM;
	if (enable) {
		writel_relaxed(0x307,
			       drv_data.mon_base + PUB_STATUS_MON_CTRL_OFFSET);
	} else {
		writel_relaxed(0x0,
			       drv_data.mon_base + PUB_STATUS_MON_CTRL_OFFSET);
	}
	return 0;
}

static int sprd_pub_monitor_status_show(struct seq_file *m, void *v)
{
	int i;
	u64 idle_time, write_time, read_time, sref_time, light_time;
	u64 fx_time[8], total_tm, sts_tm;
	u32 light_cnt, sref_cnt;

	if (!drv_data.mon_base)
		return -ENOMEM;

	if (!drv_data.pub_mon_enabled)
		return -ENODATA;

	sprd_pub_monitor_enable(0);
	if (sprd_pub_monitor_reg_get())
		return -ENODATA;
	idle_time = div64_u64((u64)drv_data.reg_val.idle_time * 1000ULL,
			      PUB_MONITOR_CLK);
	write_time = div64_u64((u64)drv_data.reg_val.write_time * 1000ULL,
			      PUB_MONITOR_CLK);
	read_time = div64_u64((u64)drv_data.reg_val.read_time * 1000ULL,
			      PUB_MONITOR_CLK);
	sref_time = div64_u64((u64)drv_data.reg_val.sref_time * 1000ULL,
			      PUB_MONITOR_CLK);
	light_time = div64_u64((u64)drv_data.reg_val.light_time * 10000ULL,
			      PUB_DFS_MONITOR_CLK);
	light_cnt = drv_data.reg_val.st_ls_cnt & 0xffff;
	sref_cnt = (drv_data.reg_val.st_ls_cnt >> 16) & 0xffff;
	sts_tm = idle_time + write_time + read_time + sref_time + light_time;

	for (i = 0, total_tm = 0; i < 8; i++) {
		fx_time[i] = div64_u64((u64)drv_data.reg_val.fx_time[i] * 10000ULL,
					PUB_DFS_MONITOR_CLK);
		total_tm += fx_time[i];
	}

	seq_printf(m, "idle_time:  %llu ns\n", idle_time);
	seq_printf(m, "write_time: %llu ns\n", write_time);
	seq_printf(m, "read_time: %llu ns\n", read_time);
	seq_printf(m, "sref_time: %llu ns\n", sref_time);
	seq_printf(m, "light_time: %llu ns\n", light_time);
	seq_printf(m, "light_cnt: %d\n", light_cnt);
	seq_printf(m, "sref_cnt: %d\n", sref_cnt);

	for (i = 0; i < 8; i++)
		seq_printf(m, "F%d time: %llu ns\n", i, fx_time[i]);
	seq_printf(m, "dfs_cnt: %d\n", (drv_data.reg_val.dfs_cnt & 0x3ff));
	seq_printf(m, "total_time:%lldns, sts_time:%lldns\n", total_tm, sts_tm);
	sprd_pub_monitor_enable(1);
	return 0;
}

static int sprd_pub_monitor_status_open(struct inode *inodep,
					struct file *filep)
{
	return single_open(filep, sprd_pub_monitor_status_show, NULL);
}

static const struct file_operations pub_monitor_status_fops = {
	.open = sprd_pub_monitor_status_open,
	.read = seq_read,
	.write = NULL,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sprd_pub_monitor_enable_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", drv_data.pub_mon_enabled);
	return 0;
}

static int sprd_pub_monitor_enable_open(struct inode *inodep,
					struct file *filep)
{
	return single_open(filep, sprd_pub_monitor_enable_show, NULL);
}

static ssize_t sprd_pub_monitor_enable_write(struct file *filep,
					     const char __user *buf,
					     size_t len, loff_t *ppos)
{
	char buf_tmp[8];
	u32 reg = 0;
	int ret;

	if (len < 1)
		return -EINVAL;
	if (copy_from_user(buf_tmp, buf, 1))
		return -EFAULT;

	if (buf_tmp[0] == '1' || buf_tmp[0] == 1) {
		reg = readl_relaxed(drv_data.mon_base
			+ DMC_DDR_CLK_CTRL_OFFSET);
		if (!(reg & 1<<PUB_CLK_DMC_REF_EB)
		    || !(reg & 1<<PUB_CLK_DFS_EB)) {
			if (!(reg & 1<<PUB_CLK_DMC_REF_EB))
				drv_data.reg_clk_ctrl |= 1<<PUB_CLK_DMC_REF_EB;
			if (!(reg & 1<<PUB_CLK_DFS_EB))
				drv_data.reg_clk_ctrl |= 1<<PUB_CLK_DFS_EB;
			reg = reg | drv_data.reg_clk_ctrl;
			writel_relaxed(reg, drv_data.mon_base
				+ DMC_DDR_CLK_CTRL_OFFSET);
		}
		drv_data.pub_mon_enabled = 1;
		ret = sprd_pub_monitor_enable(0);
		if (ret)
			return ret;
		ret = sprd_pub_monitor_enable(1);
		if (ret)
			return ret;
	} else if (buf_tmp[0] == '0' || buf_tmp[0] == 0) {
		if (drv_data.reg_clk_ctrl) {
			reg = readl_relaxed(drv_data.mon_base
					+ DMC_DDR_CLK_CTRL_OFFSET);
			reg = reg & ~(drv_data.reg_clk_ctrl);
			writel_relaxed(reg, drv_data.mon_base
					+ DMC_DDR_CLK_CTRL_OFFSET);
			drv_data.reg_clk_ctrl = 0;
		}
		drv_data.pub_mon_enabled = 0;
		ret = sprd_pub_monitor_enable(0);
		if (ret)
			return ret;
	} else {
		return -EINVAL;
	}
	return len;
}

static const struct file_operations pub_monitor_enable_fops = {
	.open = sprd_pub_monitor_enable_open,
	.read = seq_read,
	.write = sprd_pub_monitor_enable_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sprd_pub_monitor_init(void)
{
	drv_data.monitor_dir = debugfs_create_dir("sprd_pub_monitor", NULL);
	if (!drv_data.monitor_dir) {
		pr_err("pub_monitor creat dir error\n");
		return -ENOMEM;
	}
	debugfs_create_file("enable", 0664, drv_data.monitor_dir, NULL,
			    &pub_monitor_enable_fops);
	debugfs_create_file("status", 0444, drv_data.monitor_dir, NULL,
			    &pub_monitor_status_fops);

	return 0;
}

static int sprd_dmc_probe(struct platform_device *pdev)
{
	const struct dmc_data *pdata = NULL;
	struct resource *res = NULL;
	void __iomem *io_addr;
	int i;

	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}
	if (pdata->proc_res != INVALID_RES_IDX) {
		res = platform_get_resource(pdev, IORESOURCE_MEM,
			pdata->proc_res);
		if (!res)
			return -ENODEV;
		io_addr = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(io_addr))
			return PTR_ERR(io_addr);
		drv_data.size = readl_relaxed(io_addr + pdata->size_l_offset)
		  + ((u64) readl_relaxed(io_addr + pdata->size_h_offset) << 32);
		drv_data.type = readl_relaxed(io_addr + pdata->type_offset);
		for (i = 0; i < DDR_MAX_SUPPORT_CS_NUM; i++)
			drv_data.mr_val[i] =
		    readl_relaxed(io_addr + pdata->mr_offset[i]);
		iounmap(io_addr);
#ifdef CONFIG_PROC_FS
		sprd_ddr_proc_creat();
#endif
	}

	if (pdata->mon_res != INVALID_RES_IDX) {
		res = platform_get_resource(pdev, IORESOURCE_MEM,
			pdata->mon_res);
		if (!res)
			return -ENODEV;
		drv_data.mon_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(drv_data.mon_base))
			return PTR_ERR(drv_data.mon_base);
		sprd_pub_monitor_init();
	}
	return 0;
}

static int sprd_dmc_remove(struct platform_device *pdev)
{
	debugfs_remove_recursive(drv_data.monitor_dir);
	if (drv_data.property != NULL)
		remove_proc_entry(DDR_PROPERTY_NAME, drv_data.proc_dir);
	if (drv_data.info != NULL)
		remove_proc_entry(DDR_INFO_NAME, drv_data.proc_dir);
	if (drv_data.proc_dir != NULL)
		remove_proc_entry(DMC_PROC_NAME, NULL);
	return 0;
}

static const struct of_device_id sprd_dmc_of_match[] = {
	{.compatible = "sprd,pike2-dmc", .data = &pike2_data},
	{.compatible = "sprd,sharkle-dmc", .data = &sharkle_data},
	{.compatible = "sprd,sharkl3-dmc", .data = &sharkl3_data},
	{.compatible = "sprd,sharkl5-dmc", .data = &sharkl5_data},
	{.compatible = "sprd,sharkl5pro-dmc", .data = &sharkl5pro_data},
	{.compatible = "sprd,roc1-dmc", .data = &roc1_data},
	{.compatible = "sprd,orca-dmc", .data = &orca_data},
	{},
};

MODULE_DEVICE_TABLE(of, sprd_dmc_of_match);

static struct platform_driver sprd_dmc_driver = {
	.probe = sprd_dmc_probe,
	.remove = sprd_dmc_remove,
	.driver = {
		   .name = "sprd-dmc",
		   .of_match_table = sprd_dmc_of_match,
	},
};

module_platform_driver(sprd_dmc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("junqiang wang<junqiang.wang@spreadtrum.com>");
MODULE_DESCRIPTION("dmc drv for Spreadtrum");
