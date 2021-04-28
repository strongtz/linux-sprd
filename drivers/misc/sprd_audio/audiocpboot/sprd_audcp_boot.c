/*
 * Copyright (C) 2019 UNISOC Inc.
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

#define pr_fmt(fmt) "audcp boot "fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include "audio_mem.h"
#include "audio-sipc.h"
#include "sprd-string.h"

struct audcp_status {
	u32 core_status;
	u32 sys_status;
	u32 sleep_status;
};

enum {
	AUDCPBOOT_CTRL_SYS_SHUTDOWN,
	AUDCPBOOT_CTRL_CORE_SHUTDOWN,
	AUDCPBOOT_CTRL_DEEP_SLEEP,
	AUDCPBOOT_CTRL_CORE_RESET,
	AUDCPBOOT_CTRL_SYS_RESET,
	AUDCPBOOT_CTRL_RESET_SEL,
	AUDCPBOOT_CTRL_SYS_STATUS,
	AUDCPBOOT_CTRL_CORE_STATUS,
	AUDCPBOOT_CTRL_SLEEP_STATUS,
	AUDCPBOOT_CTRL_BOOTPROTECT,
	AUDCPBOOT_CTRL_BOOTVECTOR,
	AUDCPBOOT_CTRL_BOOTADDRESSSEL,
	AUDCPBOOT_CTRL_EXT4,
	AUDCPBOOT_CTRL_EXT5,
	AUDCPBOOT_CTRL_EXT6,
	AUDCPBOOT_CTRL_MAX,
};

static char * const bootctrl_name[AUDCPBOOT_CTRL_MAX] = {
	[AUDCPBOOT_CTRL_SYS_SHUTDOWN] = "sysshutdown",
	[AUDCPBOOT_CTRL_CORE_SHUTDOWN] = "coreshutdown",
	[AUDCPBOOT_CTRL_DEEP_SLEEP] = "deepsleep",
	[AUDCPBOOT_CTRL_CORE_RESET] = "corereset",
	[AUDCPBOOT_CTRL_SYS_RESET] = "sysreset",
	[AUDCPBOOT_CTRL_RESET_SEL] = "reset_sel",
	[AUDCPBOOT_CTRL_SYS_STATUS] = "sysstatus",
	[AUDCPBOOT_CTRL_CORE_STATUS] = "corestatus",
	[AUDCPBOOT_CTRL_SLEEP_STATUS] = "sleepstatus",
	[AUDCPBOOT_CTRL_BOOTPROTECT] = "bootprotect",
	[AUDCPBOOT_CTRL_BOOTVECTOR] = "bootvector",
	[AUDCPBOOT_CTRL_BOOTADDRESSSEL] = "bootaddress_sel",
};

static const char *sprd_audcp_bootctrl2name(int ctrl)
{
	if (ctrl >= AUDCPBOOT_CTRL_MAX) {
		pr_err("invalid ctrl %s %d\n", __func__, ctrl);
		return "";
	}
	if (!bootctrl_name[ctrl]) {
		pr_err("null string =%d\n", ctrl);
		return "";
	}

	return bootctrl_name[ctrl];
}

/* match with hal */
#define MAX_NAME_LEN 0x20
struct load_node_info {
	char name[MAX_NAME_LEN];
	u32 load_phy_addr;
	u32 size;
};

enum {
	MEM_AREA_CP_AON_IRAM,
	MEM_AREA_COMMU_SMSG,
	MEM_AREA_COMMU_SMSG_PARA,
	MEM_AREA_RESET_MAX,
};

struct mem_area_to_clean {
	void *vir;
	unsigned long phy;
	u32 size;
};

struct audcp_boot_data {
	struct regmap *ctrl_rmaps[AUDCPBOOT_CTRL_MAX];
	char *ctrl_reg_name[AUDCPBOOT_CTRL_MAX];
	u32 ctrl_reg[AUDCPBOOT_CTRL_MAX];
	u32 ctrl_mask[AUDCPBOOT_CTRL_MAX];
	u32 ctrl_num;
	struct load_node_info lnode;
	u32 base_addr_phy;
	void *base_addr_virt;
	int download_index;
	loff_t ppos;
	struct mem_area_to_clean reset_mem[MEM_AREA_RESET_MAX];
	u32 boot_vector;
};

static void sprd_audcp_memset_communication_area(struct audcp_boot_data *pdata)
{
	int i;

	for (i = 0; i < MEM_AREA_RESET_MAX; i++)
		memset_io((void __iomem *)pdata->reset_mem[i].vir, 0, pdata->reset_mem[i].size);
}

static ssize_t ldinfo_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct audcp_boot_data *data = dev_get_drvdata(dev);
	int count;

	if (!data)
		return -ENODEV;
	count = sizeof(data->lnode);
	memcpy(buf, &data->lnode, count);

	return count;
}

static ssize_t start_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct audcp_boot_data *pdata = dev_get_drvdata(dev);
	struct regmap **map;
	u32 *reg, *mask;
	u32 index, val;

	if (!pdata)
		return -ENODEV;

	map = pdata->ctrl_rmaps;
	reg = pdata->ctrl_reg;
	mask = pdata->ctrl_mask;

	sprd_audcp_memset_communication_area(pdata);
	/* reset sel to audio cp */
	index = AUDCPBOOT_CTRL_RESET_SEL;
	val = ~mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* set sys and core reset bit to 1 */
	index = AUDCPBOOT_CTRL_CORE_RESET;
	val = mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	index = AUDCPBOOT_CTRL_SYS_RESET;
	val = mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* clear audio cp sys force shutdown */
	index = AUDCPBOOT_CTRL_SYS_SHUTDOWN;
	val = ~mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* clear audio dsp auto shutdown */
	index = AUDCPBOOT_CTRL_CORE_SHUTDOWN;
	val = ~mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* clear audio cp force deep sleep */
	index = AUDCPBOOT_CTRL_DEEP_SLEEP;
	val = ~mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* disable audio cp boot protect */
	index = AUDCPBOOT_CTRL_BOOTPROTECT;
	val = 0x9620 << (ffs(mask[index]) - 1);
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* set boot vector */
	index = AUDCPBOOT_CTRL_BOOTVECTOR;
	val = pdata->boot_vector << (ffs(mask[index]) - 1);
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* set boot address select mode */
	index = AUDCPBOOT_CTRL_BOOTADDRESSSEL;
	val = mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* release sys and core reset to boot audio cp */
	index = AUDCPBOOT_CTRL_CORE_RESET;
	val = ~mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	index = AUDCPBOOT_CTRL_SYS_RESET;
	val = ~mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);

	return count;
}

static ssize_t stop_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct audcp_boot_data *pdata = dev_get_drvdata(dev);
	struct regmap **map;
	u32 *reg, *mask;
	u32 index, val;

	if (!pdata)
		return -ENODEV;
	map = pdata->ctrl_rmaps;
	reg = pdata->ctrl_reg;
	mask = pdata->ctrl_mask;

	/* reset download index */
	pdata->download_index = 0;
	pdata->ppos = 0;
	/* reset sel to audio cp */
	index = AUDCPBOOT_CTRL_RESET_SEL;
	val = ~mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* reset sys and core */
	index = AUDCPBOOT_CTRL_CORE_RESET;
	val = ~mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	index = AUDCPBOOT_CTRL_SYS_RESET;
	val = ~mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* audio cp force deep sleep */
	index = AUDCPBOOT_CTRL_DEEP_SLEEP;
	val = mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* audio cp sys force shutdown */
	index = AUDCPBOOT_CTRL_SYS_SHUTDOWN;
	val = mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* audio dsp auto shutdown */
	index = AUDCPBOOT_CTRL_CORE_SHUTDOWN;
	val = mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	/* set sys and core reset bit to 1 */
	index = AUDCPBOOT_CTRL_CORE_RESET;
	val = mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);
	index = AUDCPBOOT_CTRL_SYS_RESET;
	val = mask[index];
	regmap_update_bits(map[index], reg[index], mask[index], val);

	return count;
}

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct audcp_boot_data *pdata = dev_get_drvdata(dev);
	struct regmap **map;
	u32 *reg, *mask;
	u32 index, val;
	struct audcp_status status = {};

	if (!pdata)
		return -ENODEV;

	map = pdata->ctrl_rmaps;
	reg = pdata->ctrl_reg;
	mask = pdata->ctrl_mask;

	/* auddsp core wake up */
	index = AUDCPBOOT_CTRL_CORE_STATUS;
	regmap_read(map[index], reg[index], &val);
	status.core_status = (val & mask[index]) >> (ffs(mask[index]) - 1);
	/* 2 audcp sys wake up 0: power up finished 7:power off */
	index = AUDCPBOOT_CTRL_SYS_STATUS;
	regmap_read(map[index], reg[index], &val);
	status.sys_status = (val & mask[index]) >> (ffs(mask[index]) - 1);
	/* 3 sleep status */
	index =  AUDCPBOOT_CTRL_SLEEP_STATUS;
	regmap_read(map[index], reg[index], &val);
	status.sleep_status = (val & mask[index]) >> (ffs(mask[index]) - 1);
	memcpy(buf, &status, sizeof(status));

	return sizeof(status);
}

#define AUDCP_BOOT_EACH_COPY_SIZE 4096
static ssize_t agdsp_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct audcp_boot_data *data = dev_get_drvdata(dev);
	int i = 0, r;
	const char *pos;
	u32 copy_size = AUDCP_BOOT_EACH_COPY_SIZE, offset, size;
	unsigned long base_addr, cur_addr;

	if (!data) {
		dev_err(dev, "private data is null\n");
		return -EINVAL;
	}

	base_addr = (unsigned long)data->base_addr_virt;
	size = data->lnode.size;
	offset = (u32)data->ppos;
	count = min((size_t)(size - offset), count);
	r = count;
	do {
		if (r < AUDCP_BOOT_EACH_COPY_SIZE)
			copy_size = r;
		pos = buf + AUDCP_BOOT_EACH_COPY_SIZE * i;
		cur_addr = base_addr + AUDCP_BOOT_EACH_COPY_SIZE *
			data->download_index;
		memcpy((void *)cur_addr, pos, copy_size);
		r -= copy_size;
		data->download_index++;
		i++;
	} while (r > 0);
	data->ppos += count;

	return count;
}

static DEVICE_ATTR_RO(ldinfo);
static DEVICE_ATTR_WO(start);
static DEVICE_ATTR_WO(stop);
static DEVICE_ATTR_RO(status);
static DEVICE_ATTR_WO(agdsp);

static struct attribute *sprd_audcp_boot_attrs[] = {
	&dev_attr_ldinfo.attr,
	&dev_attr_start.attr,
	&dev_attr_stop.attr,
	&dev_attr_status.attr,
	&dev_attr_agdsp.attr,
	NULL,
};

ATTRIBUTE_GROUPS(sprd_audcp_boot);

static const struct of_device_id sprd_audcp_boot_match_table[] = {
	{.compatible = "sprd,sharkl5-audcp-boot",},
	{.compatible = "sprd,roc1-audcp-boot",},
	{.compatible = "sprd,orca-audcp-boot",},
};

#define AGDSP_BOOT_OFFSET 0x80
static u32 sprd_audcp_get_boot_vector(u32 bin_load_addr)
{
	return (bin_load_addr + AGDSP_BOOT_OFFSET) >> 1;
}

static int sprd_audcp_boot_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret, cr_num = 0;
	u32 syscon_args[2], size;
	struct audcp_boot_data *data;
	unsigned long addr_phy, addr_vir;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	do {
		data->ctrl_rmaps[cr_num] =
			syscon_regmap_lookup_by_name(np,
						     sprd_audcp_bootctrl2name(
						     cr_num));
		if (IS_ERR(data->ctrl_rmaps[cr_num])) {
			dev_err(&pdev->dev, "can't find %s\n",
				sprd_audcp_bootctrl2name(cr_num));
			continue;
		}
		ret = syscon_get_args_by_name(np,
					      sprd_audcp_bootctrl2name(cr_num),
					      2, syscon_args);
		if (ret == 2) {
			data->ctrl_reg[cr_num] = syscon_args[0];
			data->ctrl_mask[cr_num] = syscon_args[1];
		} else {
			dev_err(&pdev->dev, "failed to map ctrl reg\n");
			return -EINVAL;
		}
		cr_num++;
	} while (bootctrl_name[cr_num]);
	data->ctrl_num = cr_num;

	data->base_addr_phy = audio_mem_alloc(MEM_AUDCP_DSPBIN, &size);
	if (!data->base_addr_phy) {
		dev_err(&pdev->dev, "alloc MEM_AUDCP_DSPBIN failed\n");
		return -ENOMEM;
	}
	data->lnode.load_phy_addr = data->base_addr_phy;
	data->lnode.size = size;
	strcpy(data->lnode.name, "agdsp");
	data->boot_vector = sprd_audcp_get_boot_vector(data->base_addr_phy);
	data->base_addr_virt = audio_mem_vmap(data->base_addr_phy,
					      data->lnode.size, 1);
	if (!data->base_addr_virt) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "memory map for dsp bin failed\n");
		goto err1;
	}
	dev_info(&pdev->dev, "base_addr_phy = %#x, base_addr_virt=%p, bin size =%#x\n",
		 data->base_addr_phy, data->base_addr_virt, data->lnode.size);
	addr_phy = audio_mem_alloc(IRAM_AUDCP_AON, &size);
	if (!addr_phy) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "alloc IRAM AUDCP AON failed\n");
		goto err2;
	}
	data->reset_mem[MEM_AREA_CP_AON_IRAM].phy = addr_phy;
	data->reset_mem[MEM_AREA_CP_AON_IRAM].size = size;
	addr_vir = (unsigned long)audio_mem_vmap(addr_phy, size, 1);
	if (!addr_vir) {
		dev_err(&pdev->dev, "audio vmap iram cp aon failed\n");
		ret = -ENOMEM;
		goto err3;
	}
	data->reset_mem[MEM_AREA_CP_AON_IRAM].vir = (void *)addr_vir;
	ret = aud_get_aud_ipc_smsg_addr(&addr_phy, &addr_vir, &size);
	if (ret < 0) {
		dev_err(&pdev->dev, "get aud ipc smsg address failed\n");
		goto err4;
	}
	data->reset_mem[MEM_AREA_COMMU_SMSG].vir = (void *)addr_vir;
	data->reset_mem[MEM_AREA_COMMU_SMSG].phy = addr_phy;
	data->reset_mem[MEM_AREA_COMMU_SMSG].size = size;
	ret = aud_get_aud_ipc_smsg_para_addr(&addr_phy, &addr_vir, &size);
	if (ret < 0) {
		dev_err(&pdev->dev, "get aud ipc smsg para address failed\n");
		goto err4;
	}
	data->reset_mem[MEM_AREA_COMMU_SMSG_PARA].vir = (void *)addr_vir;
	data->reset_mem[MEM_AREA_COMMU_SMSG_PARA].phy = addr_phy;
	data->reset_mem[MEM_AREA_COMMU_SMSG_PARA].size = size;
	/* create sys fs */
	ret = device_add_groups(&pdev->dev, sprd_audcp_boot_groups);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add audcp boot attributes\n");
		goto err4;
	}
	platform_set_drvdata(pdev, data);

	return 0;
err4:
	audio_mem_unmap(data->reset_mem[MEM_AREA_CP_AON_IRAM].vir);
err3:
	audio_mem_free(IRAM_AUDCP_AON,
		       data->reset_mem[MEM_AREA_CP_AON_IRAM].phy,
		       data->reset_mem[MEM_AREA_CP_AON_IRAM].size);
err2:
	audio_mem_unmap(data->base_addr_virt);
err1:
	audio_mem_free(MEM_AUDCP_DSPBIN, data->base_addr_phy, data->lnode.size);

	return ret;
}

static int sprd_audcp_boot_remove(struct platform_device *pdev)
{
	struct audcp_boot_data *data = platform_get_drvdata(pdev);

	if (!data) {
		dev_err(&pdev->dev, "data is null\n");
		return -EINVAL;
	}
	device_remove_groups(&pdev->dev, sprd_audcp_boot_groups);
	audio_mem_unmap(data->reset_mem[MEM_AREA_CP_AON_IRAM].vir);
	audio_mem_free(IRAM_AUDCP_AON,
		       data->reset_mem[MEM_AREA_CP_AON_IRAM].phy,
		       data->reset_mem[MEM_AREA_CP_AON_IRAM].size);
	if (data->base_addr_virt)
		audio_mem_unmap(data->base_addr_virt);
	if (data->base_addr_phy)
		audio_mem_free(MEM_AUDCP_DSPBIN, data->base_addr_phy,
			       data->lnode.size);

	return 0;
}

static struct platform_driver sprd_audcp_boot_driver = {
	.probe    = sprd_audcp_boot_probe,
	.remove   = sprd_audcp_boot_remove,
	.driver   = {
		.name = "sprd_audcp_boot",
		.of_match_table = sprd_audcp_boot_match_table,
	},
};

module_platform_driver(sprd_audcp_boot_driver);

MODULE_AUTHOR("Lei Ning <lei.ning@unisoc.com>");
MODULE_DESCRIPTION("SPRD Audio CP boot Driver");
MODULE_LICENSE("GPL v2");
