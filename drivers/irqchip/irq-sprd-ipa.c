/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define SPRD_IPA_IRQ_STATUS	0x00
#define SPRD_IPA_RAW_STATUS	0x04
#define SPRD_IPA_IRQ_ENABLE	0x08
#define SPRD_IPA_IRQ_DISABLE	0x0c

struct sprd_ipa_intc_soc_data {
	u32 valid_mask;
};

struct sprd_ipa_intc_syscon {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

struct sprd_ipa_intc_data {
	int irq;
	u32 used_irqs;
	void __iomem *base;
	struct irq_chip irq_chip;
	struct irq_domain *irq_domain;
	struct platform_device *pdev;
	struct sprd_ipa_intc_syscon ipa_intc_eb;
	const struct sprd_ipa_intc_soc_data *socdata;
};

#define SPRD_IPA_INTC_MAX_IRQS    32
/* For ud710 SoC, hwirq numbers are from bit2 to bit23 */
#define SPRD_IPA_INTC_UD710_VALID_MASK GENMASK(23, 2)
static const struct sprd_ipa_intc_soc_data ipa_intc_ud710_data = {
	.valid_mask = SPRD_IPA_INTC_UD710_VALID_MASK,
};

static int sprd_ipa_intc_get_syscon(struct platform_device *pdev,
		struct sprd_ipa_intc_syscon *reg, const char *name)
{
	struct regmap *regmap;
	u32 syscon_args[2];
	int ret;
	struct device_node *np = pdev->dev.of_node;

	regmap = syscon_regmap_lookup_by_name(pdev->dev.of_node, name);
	if (IS_ERR(regmap)) {
		dev_err(&pdev->dev, "Lookup %s failed\n", name);
		return PTR_ERR(regmap);
	}

	ret = syscon_get_args_by_name(np, name, 2, syscon_args);
	if (ret < 0) {
		dev_err(&pdev->dev, "Get args %s failed\n", name);
		return ret;
	} else if (ret != 2) {
		dev_err(&pdev->dev, "The args numbers of %s is not 2\n", name);
		return -EINVAL;
	}
	reg->regmap = regmap;
	reg->reg = syscon_args[0];
	reg->mask = syscon_args[1];

	return 0;
}

static int sprd_ipa_intc_init_syscon(struct platform_device *pdev,
					 struct sprd_ipa_intc_data *data)
{
	int ret;

	ret = sprd_ipa_intc_get_syscon(pdev, &data->ipa_intc_eb, "ipa_intc_eb");
	if (ret)
		return ret;

	ret = regmap_update_bits(data->ipa_intc_eb.regmap,
				 data->ipa_intc_eb.reg, data->ipa_intc_eb.mask,
				 data->ipa_intc_eb.mask);
	if (ret)
		dev_err(&pdev->dev, "Update regmap fail\n");

	return ret;
}

static void sprd_ipa_mask_irq(struct irq_data *d)
{
	struct sprd_ipa_intc_data *data = irq_data_get_irq_chip_data(d);
	u32 mask = BIT(d->hwirq);

	writel_relaxed(mask, data->base + SPRD_IPA_IRQ_DISABLE);
}

/*
 * sprd_ipa_unmask_irq - Generic irq chip callback to unmask level2 interrupts
 * @data:	pointer to irqdata associated to that interrupt
 */
static void sprd_ipa_unmask_irq(struct irq_data *d)
{
	struct sprd_ipa_intc_data *data = irq_data_get_irq_chip_data(d);
	u32 mask = BIT(d->hwirq);

	writel_relaxed(mask, data->base + SPRD_IPA_IRQ_ENABLE);
}

static struct irq_chip sprd_ipa_irq_chip = {
	.name			= "sprd-ipa-intc",
	.irq_mask		= sprd_ipa_mask_irq,
	.irq_unmask		= sprd_ipa_unmask_irq,
};

static int sprd_ipa_irq_domain_map(struct irq_domain *domain,
		unsigned int irq, irq_hw_number_t hwirq)
{
	struct sprd_ipa_intc_data *data = domain->host_data;

	/* Skip unused IRQs, only register handlers for the real ones */
	if (!(data->socdata->valid_mask & BIT(hwirq))) {
		dev_err(&data->pdev->dev,
			"Hwirq %d is not correct\n", (int)hwirq);
		return -EPERM;
	}

	irq_set_chip_and_handler(irq, &data->irq_chip, handle_level_irq);
	irq_set_chip_data(irq, data);
	irq_set_parent(irq, data->irq);
	irq_set_noprobe(irq);

	return 0;
}

static const struct irq_domain_ops sprd_ipa_irq_domain_ops = {
	.map = sprd_ipa_irq_domain_map,
};

static void sprd_ipa_irq_handle(struct irq_desc *desc)
{
	unsigned long pos, status;
	unsigned int irq;
	struct sprd_ipa_intc_data *data = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);

	if (!data) {
		dev_err(&data->pdev->dev, "Irq desc can't get handler data\n");
		return;
	}

	chained_irq_enter(chip, desc);

	status = readl_relaxed(data->base + SPRD_IPA_IRQ_STATUS);
	for_each_set_bit(pos, &status, SPRD_IPA_INTC_MAX_IRQS) {
		irq = irq_find_mapping(data->irq_domain, pos);
		generic_handle_irq(irq);
		writel_relaxed(BIT(pos), data->base + SPRD_IPA_IRQ_STATUS);
	}
	chained_irq_exit(chip, desc);
}

static int sprd_ipa_intc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct sprd_ipa_intc_data *data;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->pdev = pdev;
	platform_set_drvdata(pdev, data);

	ret = sprd_ipa_intc_init_syscon(pdev, data);
	if (ret < 0) {
		dev_err(dev, "Fail to get syscon resource\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	data->socdata =
		(struct sprd_ipa_intc_soc_data *)of_device_get_match_data(dev);
	if (!data->socdata)
		return -EINVAL;

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0) {
		dev_err(dev, "Fail to get IRQ resource\n");
		return data->irq;
	}

	data->irq_chip = sprd_ipa_irq_chip;

	data->irq_domain = irq_domain_add_linear(dev->of_node,
						fls(data->socdata->valid_mask),
						&sprd_ipa_irq_domain_ops, data);
	if (!data->irq_domain) {
		dev_err(dev, "Fail to get the SPRD level2 ipa IRQ domain\n");
		return -ENXIO;
	}

	irq_set_chained_handler_and_data(data->irq, sprd_ipa_irq_handle, data);

	return 0;
}

static const struct of_device_id sprd_ipa_intc_match_table[] = {
	{
		.compatible = "sprd,ud710-ipa-intc",
		.data = &ipa_intc_ud710_data,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, sprd_ipa_intc_match_table);

static struct platform_driver sprd_ipa_intc_driver = {
	.probe = sprd_ipa_intc_probe,
	.driver = {
		.name = "sprd,ipa-l2-intc",
		.of_match_table = sprd_ipa_intc_match_table,
	},
};

/* TODO: It's a good idea to use IRQCHIP_DECLARE() in future */
module_platform_driver(sprd_ipa_intc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Billows Wu <billows.wu@unisoc.com>");
MODULE_DESCRIPTION("Spreadtrum ipa level2 interrupt controller driver");
