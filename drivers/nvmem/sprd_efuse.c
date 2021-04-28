// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Spreadtrum Communications Inc.

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/hwspinlock.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/nvmem-provider.h>

#define SPRD_EFUSE_ALL0_INDEX		0x8
#define SPRD_EFUSE_MODE_CTRL		0xc
#define SPRD_EFUSE_IP_VER		0x14
#define SPRD_EFUSE_CFG0			0x18
#define SPRD_EFUSE_NS_EN		0x20
#define SPRD_EFUSE_NS_ERR_FLAG		0x24
#define SPRD_EFUSE_NS_FLAG_CLR		0x28
#define SPRD_EFUSE_NS_MAGIC_NUM		0x2c
#define SPRD_EFUSE_FW_CFG		0x50
#define SPRD_EFUSE_PW_SWT		0x54
#define SPRD_EFUSE_MEM(val)		(0x1000 + ((val) << 2))

/* bits definitions for register EFUSE_MODE_CTRL */
#define SPRD_EFUSE_ALL0_CHECK_START	BIT(0)

/* bits definitions for register EFUSE_NS_EN */
#define SPRD_NS_VDD_EN			BIT(0)
#define SPRD_NS_AUTO_CHECK_ENABLE	BIT(1)
#define SPRD_DOUBLE_BIT_EN_NS		BIT(2)
#define SPRD_NS_MARGIN_RD_ENABLE	BIT(3)
#define SPRD_NS_LOCK_BIT_WR_EN		BIT(4)

/* bits definitions for register EFUSE_NS_ERR_FLAG */
#define SPRD_NS_WORD0_ERR_FLAG		BIT(0)
#define SPRD_NS_WORD1_ERR_FLAG		BIT(1)
#define SPRD_NS_WORD0_PROT_FLAG		BIT(4)
#define SPRD_NS_WORD1_PROT_FLAG		BIT(5)
#define SPRD_NS_PG_EN_WR_FLAG		BIT(8)
#define SPRD_NS_VDD_ON_RD_FLAG		BIT(9)
#define SPRD_NS_BLOCK0_RD_FLAG		BIT(10)
#define SPRD_NS_MAGNUM_WR_FLAG		BIT(11)
#define SPRD_NS_ENK_ERR_FLAG		BIT(12)
#define SPRD_NS_ALL0_CHECK_FLAG		BIT(13)

/* bits definitions for register EFUSE_NS_FLAG_CLR */
#define SPRD_NS_WORD0_ERR_CLR		BIT(0)
#define SPRD_NS_WORD1_ERR_CLR		BIT(1)
#define SPRD_NS_WORD0_PROT_CLR		BIT(4)
#define SPRD_NS_WORD1_PROT_CLR		BIT(5)
#define SPRD_NS_PG_EN_WR_CLR		BIT(8)
#define SPRD_NS_VDD_ON_RD_CLR		BIT(9)
#define SPRD_NS_BLOCK0_RD_CLR		BIT(10)
#define SPRD_NS_MAGNUM_WR_CLR		BIT(11)
#define SPRD_NS_ENK_ERR_CLR		BIT(12)
#define SPRD_NS_ALL0_CHECK_CLR		BIT(13)

/* bits definitions for register EFUSE_PW_SWT */
#define SPRD_EFS_ENK1_ON		BIT(0)
#define SPRD_EFS_ENK2_ON		BIT(1)
#define SPRD_NS_S_PG_EN			BIT(2)

#define SPRD_NS_AUTO_CHECK_FLAG		\
	(SPRD_NS_WORD0_ERR_FLAG | SPRD_NS_WORD1_ERR_FLAG)
#define SPRD_NS_EFUSE_MAGIC_NUMBER(x)	((x) & GENMASK(15, 0))
#define SPRD_ERR_CLR_MASK		GENMASK(13, 0)
#define SPRD_EFUSE_MAGIC_NUMBER		0x8810

/* Block number and block width (bytes) definitions */
#define SPRD_EFUSE_BLOCK_MAX		96
#define SPRD_EFUSE_BLOCK_WIDTH		4
#define SPRD_EFUSE_BLOCK_SIZE		(SPRD_EFUSE_BLOCK_WIDTH * BITS_PER_BYTE)
#define SPRD_EFUSE_BLOCK_START		72

/* Timeout (ms) for the trylock of hardware spinlocks */
#define SPRD_EFUSE_HWLOCK_TIMEOUT	5000

/*
 * Since different sprd chip can have different block max,
 * we should save address in the device data structure.
 */
struct sprd_efuse_variant_data {
	u32 blk_max;
	u32 blk_start;
	bool blk_double;
};

struct sprd_efuse {
	struct device *dev;
	struct clk *clk;
	struct hwspinlock *hwlock;
	struct mutex mutex;
	void __iomem *base;
	const struct sprd_efuse_variant_data *var_data;
};

static const struct sprd_efuse_variant_data sharkl5_data = {
	.blk_max = 95,
	.blk_start = 72,
	.blk_double = 0,
};

static const struct sprd_efuse_variant_data roc1_data = {
	.blk_max = 79,
	.blk_start = 37,
	.blk_double = 1,
};

static const struct sprd_efuse_variant_data sharkl3_data = {
	.blk_max = 46,
	.blk_start = 36,
	.blk_double = 1,
};

static const struct sprd_efuse_variant_data orca_data = {
	.blk_max = 46,
	.blk_start = 2,
	.blk_double = 1,
};

static const struct sprd_efuse_variant_data pike2_data = {
	.blk_max = 47,
	.blk_start = 36,
	.blk_double = 1,
};

static int sprd_efuse_lock(struct sprd_efuse *efuse)
{
	int ret;

	mutex_lock(&efuse->mutex);

	ret = hwspin_lock_timeout_raw(efuse->hwlock,
				      SPRD_EFUSE_HWLOCK_TIMEOUT);
	if (ret) {
		dev_err(efuse->dev, "timeout get the hwspinlock\n");
		mutex_unlock(&efuse->mutex);
		return ret;
	}

	return 0;
}

static void sprd_efuse_unlock(struct sprd_efuse *efuse)
{
	hwspin_unlock_raw(efuse->hwlock);
	mutex_unlock(&efuse->mutex);
}

static void sprd_efuse_prog_power(struct sprd_efuse *efuse, bool en)
{
	u32 val = readl(efuse->base + SPRD_EFUSE_PW_SWT);

	if (en)
		val &= ~SPRD_EFS_ENK2_ON;
	else
		val &= ~SPRD_EFS_ENK1_ON;
	writel(val, efuse->base + SPRD_EFUSE_PW_SWT);

	/* Open or close efuse power need wait 1000us make power stability. */
	udelay(1000);
	if (en)
		val |= SPRD_EFS_ENK1_ON;
	else
		val |= SPRD_EFS_ENK2_ON;
	writel(val, efuse->base + SPRD_EFUSE_PW_SWT);

	/* Open or close efuse power need wait 1000us make power stability. */
	udelay(1000);
}

static void sprd_efuse_read_power(struct sprd_efuse *efuse, bool en)
{
	u32 val = readl(efuse->base + SPRD_EFUSE_NS_EN);

	if (en)
		val |= SPRD_NS_VDD_EN;
	else
		val &= ~SPRD_NS_VDD_EN;

	writel(val, efuse->base + SPRD_EFUSE_NS_EN);

	/* Open or close efuse power need wait 1000us make power stability. */
	udelay(1000);
}

static void sprd_efuse_set_prog_lock(struct sprd_efuse *efuse, bool en)
{
	u32 val = readl(efuse->base + SPRD_EFUSE_NS_EN);

	if (en)
		val |= SPRD_NS_LOCK_BIT_WR_EN;
	else
		val &= ~SPRD_NS_LOCK_BIT_WR_EN;

	writel(val, efuse->base + SPRD_EFUSE_NS_EN);
}

static void sprd_efuse_set_auto_check(struct sprd_efuse *efuse, bool en)
{
	u32 val = readl(efuse->base + SPRD_EFUSE_NS_EN);

	if (en)
		val |= SPRD_NS_AUTO_CHECK_ENABLE;
	else
		val &= ~SPRD_NS_AUTO_CHECK_ENABLE;

	writel(val, efuse->base + SPRD_EFUSE_NS_EN);
}

static void sprd_efuse_set_data_double(struct sprd_efuse *efuse, bool en)
{
	u32 val = readl(efuse->base + SPRD_EFUSE_NS_EN);

	if (en)
		val |= SPRD_DOUBLE_BIT_EN_NS;
	else
		val &= ~SPRD_DOUBLE_BIT_EN_NS;

	writel(val, efuse->base + SPRD_EFUSE_NS_EN);
}

static void sprd_efuse_set_prog_en(struct sprd_efuse *efuse, bool en)
{
	u32 val = readl(efuse->base + SPRD_EFUSE_PW_SWT);

	if (en)
		val |= SPRD_NS_S_PG_EN;
	else
		val &= ~SPRD_NS_S_PG_EN;

	writel(val, efuse->base + SPRD_EFUSE_PW_SWT);
}

static int sprd_efuse_raw_prog(struct sprd_efuse *efuse, u32 blk, bool doub,
			       bool lock, u32 *data)
{
	int ret;

	/* We need set the magic number before writing the efuse. */
	writel(SPRD_NS_EFUSE_MAGIC_NUMBER(SPRD_EFUSE_MAGIC_NUMBER),
	       efuse->base + SPRD_EFUSE_NS_MAGIC_NUM);

	/* Power on the efuse and enbale programme. */
	sprd_efuse_prog_power(efuse, true);
	sprd_efuse_set_prog_en(efuse, true);
	sprd_efuse_set_data_double(efuse, doub);

	/*
	 * Enable the auto-check function to valid if the programming is
	 * successful.
	 */
	sprd_efuse_set_auto_check(efuse, true);

	writel(*data, efuse->base + SPRD_EFUSE_MEM(blk));
	sprd_efuse_set_auto_check(efuse, false);
	sprd_efuse_set_data_double(efuse, false);

	/*
	 * Check the efuse error status, if the programming is successful,
	 * we should lock this efuse block to avoid programming again.
	 */
	ret = readl(efuse->base + SPRD_EFUSE_NS_ERR_FLAG);
	if (ret) {
		dev_err(efuse->dev, "error status %d of block %d\n", ret, blk);
		ret = -EINVAL;
	} else {
		sprd_efuse_set_prog_lock(efuse, lock);
		writel(*data, efuse->base + SPRD_EFUSE_MEM(blk));
		sprd_efuse_set_prog_lock(efuse, false);
	}

	writel(SPRD_ERR_CLR_MASK, efuse->base + SPRD_EFUSE_NS_FLAG_CLR);
	sprd_efuse_prog_power(efuse, false);
	writel(0, efuse->base + SPRD_EFUSE_NS_MAGIC_NUM);

	return ret;
}

static int sprd_efuse_raw_read(struct sprd_efuse *efuse, int blk, u32 *val,
			       bool doub)
{
	int ret;

	ret = sprd_efuse_lock(efuse);
	if (ret)
		return ret;

	ret = clk_enable(efuse->clk);
	if (ret) {
		sprd_efuse_unlock(efuse);
		return ret;
	}

	/*
	 * Need power on the efuse before reading data from efuse, and will
	 * power off the efuse after reading process.
	 */
	sprd_efuse_read_power(efuse, true);

	/* Start to read data from efuse block. */
	sprd_efuse_set_data_double(efuse, doub);
	*val = readl(efuse->base + SPRD_EFUSE_MEM(blk));
	sprd_efuse_set_data_double(efuse, false);

	sprd_efuse_read_power(efuse, false);

	/*
	 * Check the efuse error status and clear the error status if there are
	 * some errors occured.
	 */
	ret = readl(efuse->base + SPRD_EFUSE_NS_ERR_FLAG);
	if (ret) {
		dev_err(efuse->dev, "error status %d of block %d\n", ret, blk);
		ret = -EINVAL;
	}
	writel(SPRD_ERR_CLR_MASK, efuse->base + SPRD_EFUSE_NS_FLAG_CLR);

	clk_disable(efuse->clk);
	sprd_efuse_unlock(efuse);

	return ret;
}

static int sprd_efuse_prog(struct sprd_efuse *efuse, int blk, bool doub,
			   bool lock, u32 *val)
{
	int ret;

	ret = sprd_efuse_lock(efuse);
	if (ret)
		return ret;

	ret = clk_enable(efuse->clk);
	if (ret)
		goto unlock_hwlock;

	ret = sprd_efuse_raw_prog(efuse, blk, doub, lock, val);

	clk_disable(efuse->clk);
unlock_hwlock:
	sprd_efuse_unlock(efuse);
	return ret;
}

static int sprd_efuse_read(void *context, u32 offset, void *val, size_t bytes)
{
	struct sprd_efuse *efuse = context;
	bool blk_double = efuse->var_data->blk_double;
	u32 data, index = offset / SPRD_EFUSE_BLOCK_WIDTH;
	u32 blk_offset = (offset % SPRD_EFUSE_BLOCK_WIDTH) * BITS_PER_BYTE;
	int ret;

	/* efuse has two parts secure efuse block and public efuse block.
	 * public eFuse starts at SPRD_EFUSE_BLOCK_STAR block.
	 */
	index += efuse->var_data->blk_start;

	if (of_device_is_compatible(efuse->dev->of_node, "sprd,sharkl3-efuse") ||
	    of_device_is_compatible(efuse->dev->of_node, "sprd,sharkle-efuse") ||
	    of_device_is_compatible(efuse->dev->of_node, "sprd,orca-efuse")) {
		if (index == 95 || index == 94)
			blk_double = 0;
	}

	ret = sprd_efuse_raw_read(efuse, index, &data, blk_double);
	if (!ret) {
		data >>= blk_offset;
		memcpy(val, &data, bytes);
	}

	return ret;
}

static int sprd_efuse_write(void *context, u32 offset, void *val, size_t bytes)
{
	struct sprd_efuse *efuse = context;

	return sprd_efuse_prog(efuse, offset, false, false, val);
}

static int sprd_efuse_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct nvmem_device *nvmem;
	struct nvmem_config econfig = { };
	struct resource *res;
	struct sprd_efuse *efuse;
	const struct sprd_efuse_variant_data *pdata;
	u32 blk_num;
	int ret;

	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	efuse = devm_kzalloc(&pdev->dev, sizeof(*efuse), GFP_KERNEL);
	if (!efuse)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	efuse->base = devm_ioremap_resource(&pdev->dev, res);
	if (!efuse->base)
		return -ENOMEM;

	ret = of_hwspin_lock_get_id(np, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get hwlock id\n");
		return ret;
	}

	efuse->hwlock = hwspin_lock_request_specific(ret);
	if (!efuse->hwlock) {
		dev_err(&pdev->dev, "failed to request hwlock\n");
		return -ENXIO;
	}

	efuse->clk = devm_clk_get(&pdev->dev, "enable");
	if (IS_ERR(efuse->clk)) {
		dev_err(&pdev->dev, "failed to get enable clock\n");
		ret = PTR_ERR(efuse->clk);
		goto free_hwlock;
	}
	clk_prepare(efuse->clk);

	mutex_init(&efuse->mutex);
	efuse->dev = &pdev->dev;
	efuse->var_data = pdata;
	blk_num = efuse->var_data->blk_max - efuse->var_data->blk_start + 1;

	econfig.stride = 1;
	econfig.word_size = 1;
	econfig.read_only = false;
	econfig.name = "sprd-efuse";
	econfig.size = blk_num * SPRD_EFUSE_BLOCK_WIDTH;
	econfig.reg_read = sprd_efuse_read;
	econfig.reg_write = sprd_efuse_write;
	econfig.priv = efuse;
	econfig.dev = &pdev->dev;
	nvmem = devm_nvmem_register(&pdev->dev, &econfig);
	if (IS_ERR(nvmem)) {
		dev_err(&pdev->dev, "failed to register nvmem\n");
		ret = IS_ERR(nvmem);
		goto unprepare_clk;
	}

	platform_set_drvdata(pdev, efuse);
	return 0;

unprepare_clk:
	clk_unprepare(efuse->clk);
free_hwlock:
	hwspin_lock_free(efuse->hwlock);
	return ret;
}

static int sprd_efuse_remove(struct platform_device *pdev)
{
	struct sprd_efuse *efuse = platform_get_drvdata(pdev);

	hwspin_lock_free(efuse->hwlock);
	clk_unprepare(efuse->clk);
	return 0;
}

static const struct of_device_id sprd_efuse_of_match[] = {
	{ .compatible = "sprd,sharkl5-efuse", .data = &sharkl5_data},
	{ .compatible = "sprd,roc1-efuse", .data = &roc1_data},
	{ .compatible = "sprd,sharkl3-efuse", .data = &sharkl3_data},
	{ .compatible = "sprd,orca-efuse", .data = &orca_data},
	{ .compatible = "sprd,pike2-efuse", .data = &pike2_data},
	{ }
};

static struct platform_driver sprd_efuse_driver = {
	.probe = sprd_efuse_probe,
	.remove = sprd_efuse_remove,
	.driver = {
		.name = "sprd-efuse",
		.of_match_table = sprd_efuse_of_match,
	},
};

module_platform_driver(sprd_efuse_driver);

MODULE_AUTHOR("Freeman Liu <freeman.liu@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum AP efuse driver");
MODULE_LICENSE("GPL v2");
