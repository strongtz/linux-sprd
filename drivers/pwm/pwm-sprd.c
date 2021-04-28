// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/pwm.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/math64.h>
#include <linux/err.h>

#define NUM_PWM		4
#define PWM_REGS_SHIFT	5
#define PWM_MOD_MAX	GENMASK(9, 0)
#define PWM_REG_MSK	GENMASK(15, 0)

#define PWM_PRESCALE	0x0
#define PWM_MOD		0x4
#define PWM_DUTY	0x8
#define PWM_DIV		0xc
#define PWM_PAT_LOW	0x10
#define PWM_PAT_HIGH	0x14
#define PWM_ENABLE	0x18
#define PWM_VERSION	0x1c

#define BIT_ENABLE	BIT(0)
#define PWM_CLK_PARENT	"clk_parent"
#define PWM_CLK		"clk_pwm"

struct sprd_pwm_chip {
	void __iomem *mmio_base;
	int num_pwms;
	struct clk *clk_pwm[NUM_PWM];
	struct clk *clk_eb[NUM_PWM];
	bool eb_enabled[NUM_PWM];
	struct pwm_chip chip;
};

static inline u32 sprd_pwm_readl(struct sprd_pwm_chip *chip, u32 num,
					u32 offset)
{
	return readl_relaxed((void __iomem *)(chip->mmio_base +
					(num << PWM_REGS_SHIFT) + offset));
}

static inline void sprd_pwm_writel(struct sprd_pwm_chip *chip,
					u32 num, u32 offset,
					u32 val)
{
	writel_relaxed(val, (void __iomem *)(chip->mmio_base +
			(num << PWM_REGS_SHIFT) + offset));
}

static int sprd_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
				int duty_ns, int period_ns)
{
	struct sprd_pwm_chip *spc = container_of(chip,
		struct sprd_pwm_chip, chip);
	u64 clk_rate, div, val, tmp;
	int rc, prescale, level;

	if (!duty_ns)
		return 0;

	if (!spc->eb_enabled[pwm->hwpwm]) {
		rc = clk_prepare_enable(spc->clk_eb[pwm->hwpwm]);
		if (rc) {
			dev_err(chip->dev, "enable pwm%u failed\n", pwm->hwpwm);
			return rc;
		}
		spc->eb_enabled[pwm->hwpwm] = true;
	}

	tmp = duty_ns * PWM_MOD_MAX;
	level = DIV_ROUND_CLOSEST_ULL(tmp, period_ns);
	dev_dbg(chip->dev, "duty_ns = %d, period_ns = %d, level = %d\n",
		duty_ns, period_ns, level);

	clk_rate = clk_get_rate(spc->clk_pwm[pwm->hwpwm]);
	/*
	 * Find pv, dc and prescale to suit duty_ns and period_ns.
	 * This is done according to formulas described below:
	 *
	 * period_ns = 10^9 * (PRESCALE + 1) * PV / PWM_CLK_RATE
	 * duty_ns = 10^9 * (PRESCALE + 1) * DC / PWM_CLK_RATE
	 *
	 * PV = (PWM_CLK_RATE * period_ns) / (10^9 * (PRESCALE + 1))
	 * DC = (PWM_CLK_RATE * duty_ns) / (10^9 * (PRESCALE + 1))
	 */
	div = 1000000000;
	div = div * PWM_MOD_MAX;
	val = clk_rate * period_ns;
	prescale = div64_u64(val, div) - 1;
	if (prescale < 0)
		prescale = 0;
	sprd_pwm_writel(spc, pwm->hwpwm, PWM_MOD, PWM_MOD_MAX);
	sprd_pwm_writel(spc, pwm->hwpwm, PWM_DUTY, level);
	sprd_pwm_writel(spc, pwm->hwpwm, PWM_PAT_LOW, PWM_REG_MSK);
	sprd_pwm_writel(spc, pwm->hwpwm, PWM_PAT_HIGH, PWM_REG_MSK);
	sprd_pwm_writel(spc, pwm->hwpwm, PWM_PRESCALE, prescale);

	return 0;
}

static int sprd_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct sprd_pwm_chip *spc = container_of(chip,
		struct sprd_pwm_chip, chip);
	int rc;

	if (!spc->eb_enabled[pwm->hwpwm]) {
		rc = clk_prepare_enable(spc->clk_eb[pwm->hwpwm]);
		if (rc) {
			dev_err(chip->dev, "enable pwm%u failed\n", pwm->hwpwm);
			return rc;
		}
		spc->eb_enabled[pwm->hwpwm] = true;
	}

	rc = clk_prepare_enable(spc->clk_pwm[pwm->hwpwm]);
	if (rc) {
		dev_err(chip->dev, "enable pwm%u clock failed\n", pwm->hwpwm);
		clk_disable_unprepare(spc->clk_eb[pwm->hwpwm]);
		spc->eb_enabled[pwm->hwpwm] = false;
		return rc;
	}

	sprd_pwm_writel(spc, pwm->hwpwm, PWM_ENABLE, 1);

	return rc;
}

static void sprd_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct sprd_pwm_chip *spc = container_of(chip,
		struct sprd_pwm_chip, chip);

	sprd_pwm_writel(spc, pwm->hwpwm, PWM_ENABLE, 0);
	clk_disable_unprepare(spc->clk_pwm[pwm->hwpwm]);
	if (spc->eb_enabled[pwm->hwpwm]) {
		clk_disable_unprepare(spc->clk_eb[pwm->hwpwm]);
		spc->eb_enabled[pwm->hwpwm] = false;
	}
}

static void sprd_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
			struct pwm_state *state)
{
	int rc, duty_ns, period_ns;
	u32 enabled, duty, prescale;
	u64 clk_rate, val;
	struct sprd_pwm_chip *spc = container_of(chip,
		struct sprd_pwm_chip, chip);

	rc = clk_prepare_enable(spc->clk_eb[pwm->hwpwm]);
	if (rc) {
		dev_err(chip->dev,
				"enable pwm%u eb failed\n",
				pwm->hwpwm);
		return;
	}
	spc->eb_enabled[pwm->hwpwm] = true;
	rc = clk_prepare_enable(spc->clk_pwm[pwm->hwpwm]);
	if (rc) {
		clk_disable_unprepare(spc->clk_eb[pwm->hwpwm]);
		spc->eb_enabled[pwm->hwpwm] = false;
		dev_err(chip->dev,
				"enable pwm%u clk failed\n",
				pwm->hwpwm);
		return;
	}

	duty = sprd_pwm_readl(spc, pwm->hwpwm, PWM_DUTY) & PWM_REG_MSK;
	prescale = sprd_pwm_readl(spc, pwm->hwpwm, PWM_PRESCALE) & PWM_REG_MSK;
	enabled = sprd_pwm_readl(spc, pwm->hwpwm, PWM_ENABLE) & BIT_ENABLE;

	clk_rate = clk_get_rate(spc->clk_pwm[pwm->hwpwm]);
	if (!clk_rate) {
		dev_err(chip->dev, "get pwm%d clk rate failed\n", pwm->hwpwm);
		goto out;
	}

	/*
	 * Find pv, dc and prescale to suit duty_ns and period_ns.
	 * This is done according to formulas described below:
	 *
	 * period_ns = 10^9 * (PRESCALE + 1) * PV / PWM_CLK_RATE
	 * duty_ns = 10^9 * (PRESCALE + 1) * DC / PWM_CLK_RATE
	 *
	 * PV = (PWM_CLK_RATE * period_ns) / (10^9 * (PRESCALE + 1))
	 * DC = (PWM_CLK_RATE * duty_ns) / (10^9 * (PRESCALE + 1))
	 */
	val = ((u64)prescale + 1) * NSEC_PER_SEC * PWM_MOD_MAX;
	period_ns = div64_u64(val, clk_rate);
	val = ((u64)prescale + 1) * NSEC_PER_SEC * duty;
	duty_ns = div64_u64(val, clk_rate);

	state->period = period_ns;
	state->duty_cycle = duty_ns;
	state->enabled = !!enabled;

out:
	if (!enabled) {
		clk_disable_unprepare(spc->clk_pwm[pwm->hwpwm]);
		clk_disable_unprepare(spc->clk_eb[pwm->hwpwm]);
		spc->eb_enabled[pwm->hwpwm] = false;
	}
}

static const struct pwm_ops sprd_pwm_ops = {
	.config = sprd_pwm_config,
	.enable = sprd_pwm_enable,
	.disable = sprd_pwm_disable,
	.get_state = sprd_pwm_get_state,
	.owner = THIS_MODULE,
};

static int sprd_pwm_clk_init(struct platform_device *pdev)
{
	struct sprd_pwm_chip *spc = platform_get_drvdata(pdev);
	struct clk *clk_parent;
	char clk_name[64];
	int i;

	clk_parent = devm_clk_get(&pdev->dev, PWM_CLK_PARENT);
	if (IS_ERR(clk_parent)) {
		dev_err(&pdev->dev, "get clk parent failed\n");
		return PTR_ERR(clk_parent);
	}

	for (i = 0; i < NUM_PWM; i++) {
		memset(clk_name, 0, sizeof(clk_name));
		snprintf(clk_name, sizeof(clk_name) - 1, PWM_CLK"%d", i);
		spc->clk_pwm[i] = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(spc->clk_pwm[i])) {
			if (PTR_ERR(spc->clk_pwm[i]) == -ENOENT)
				break;

			dev_err(&pdev->dev, "get clk %d failed\n", i);
			return PTR_ERR(spc->clk_pwm[i]);
		}

		memset(clk_name, 0, sizeof(clk_name));
		snprintf(clk_name, sizeof(clk_name) - 1, PWM_CLK"%d_eb", i);
		spc->clk_eb[i] = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(spc->clk_eb[i])) {
			dev_err(&pdev->dev, "get clk_eb %d failed\n", i);
			return PTR_ERR(spc->clk_eb[i]);
		}

		clk_set_parent(spc->clk_pwm[i], clk_parent);
	}

	spc->num_pwms = i;

	return 0;
}

static const struct of_device_id sprd_pwm_of_match[] = {
	{ .compatible = "sprd,sharkl5-pwm", },
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pwm_of_match);

static int sprd_pwm_probe(struct platform_device *pdev)
{
	struct sprd_pwm_chip *spc;
	struct resource *res;
	int ret;

	spc = devm_kzalloc(&pdev->dev, sizeof(*spc), GFP_KERNEL);
	if (!spc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	spc->mmio_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(spc->mmio_base))
		return PTR_ERR(spc->mmio_base);

	platform_set_drvdata(pdev, spc);

	ret = sprd_pwm_clk_init(pdev);
	if (ret)
		return ret;

	spc->chip.dev = &pdev->dev;
	spc->chip.ops = &sprd_pwm_ops;
	spc->chip.base = -1;
	spc->chip.npwm = spc->num_pwms;

	ret = pwmchip_add(&spc->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "add pwm chip failed\n");
		return ret;
	}

	return 0;
}

static int sprd_pwm_remove(struct platform_device *pdev)
{
	struct sprd_pwm_chip *spc = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < spc->num_pwms; i++)
		pwm_disable(&spc->chip.pwms[i]);

	return pwmchip_remove(&spc->chip);
}

static struct platform_driver sprd_pwm_driver = {
	.driver = {
		.name = "sprd-pwm",
		.owner = THIS_MODULE,
		.of_match_table = sprd_pwm_of_match,
	},
	.probe = sprd_pwm_probe,
	.remove = sprd_pwm_remove,
};

module_platform_driver(sprd_pwm_driver);

MODULE_DESCRIPTION("Spreadtrum PWM Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Neo Hou <neo.hou@unisoc.com>");
