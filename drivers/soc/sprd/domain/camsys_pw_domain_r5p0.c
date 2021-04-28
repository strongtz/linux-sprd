/*
 * Copyright (C) 2017-2018 Spreadtrum Communications Inc.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <video/sprd_mmsys_pw_domain.h>


#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "cam_sys_pw: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static const char * const syscon_name[] = {
	"shutdown_en",
	"force_shutdown",
	"pwr_status0",
	"bus_status0",
	"init_dis_bits"
};

enum  {
	CAMSYS_SHUTDOWN_EN = 0,
	CAMSYS_FORCE_SHUTDOWN,
	CAMSYS_PWR_STATUS0,
	CAMSYS_BUS_STATUS0,
	CAMSYS_INIT_DIS_BITS
};

struct register_gpr {
	struct regmap *gpr;
	uint32_t reg;
	uint32_t mask;
};

struct camsys_power_info {
	atomic_t users_pw;
	atomic_t users_clk;
	atomic_t inited;
	struct mutex mlock;

	struct clk *cam_clk_cphy_cfg_gate_eb;
	struct clk *cam_mm_eb;

	struct clk *cam_ahb_clk;
	struct clk *cam_ahb_clk_default;
	struct clk *cam_ahb_clk_parent;

	struct clk *cam_emc_clk;
	struct clk *cam_emc_clk_default;
	struct clk *cam_emc_clk_parent;

	struct register_gpr syscon_regs[ARRAY_SIZE(syscon_name)];
};

static struct camsys_power_info *pw_info;

static int sprd_campw_check_drv_init(void)
{
	int ret = 0;

	if (!pw_info) {
		ret = -1;
		return ret;
	}
	if (atomic_read(&pw_info->inited) == 0) {
		ret = -2;
		return ret;
	}
	return ret;
}

static int sprd_campw_init(struct platform_device *pdev)
{
	int i, ret = 0;
	struct device_node *np = pdev->dev.of_node;
	const char *pname;
	struct regmap *tregmap;
	uint32_t args[2];

	pw_info = devm_kzalloc(&pdev->dev, sizeof(*pw_info), GFP_KERNEL);
	if (!pw_info)
		return -ENOMEM;

	pw_info->cam_clk_cphy_cfg_gate_eb =
		devm_clk_get(&pdev->dev, "clk_cphy_cfg_gate_eb");
	if (IS_ERR_OR_NULL(pw_info->cam_clk_cphy_cfg_gate_eb))
		return PTR_ERR(pw_info->cam_clk_cphy_cfg_gate_eb);

	pw_info->cam_mm_eb = devm_clk_get(&pdev->dev, "clk_mm_eb");
	if (IS_ERR_OR_NULL(pw_info->cam_mm_eb))
		return PTR_ERR(pw_info->cam_mm_eb);

	pw_info->cam_ahb_clk = devm_clk_get(&pdev->dev, "clk_mm_ahb");
	if (IS_ERR_OR_NULL(pw_info->cam_ahb_clk))
		return PTR_ERR(pw_info->cam_ahb_clk);

	pw_info->cam_ahb_clk_parent =
		devm_clk_get(&pdev->dev, "clk_mm_ahb_parent");
	if (IS_ERR_OR_NULL(pw_info->cam_ahb_clk_parent))
		return PTR_ERR(pw_info->cam_ahb_clk_parent);

	pw_info->cam_ahb_clk_default = clk_get_parent(pw_info->cam_ahb_clk);
	if (IS_ERR_OR_NULL(pw_info->cam_ahb_clk_default))
		return PTR_ERR(pw_info->cam_ahb_clk_default);

	/* need set cgm_mm_emc_sel :512m , DDR  matrix clk*/
	pw_info->cam_emc_clk = devm_clk_get(&pdev->dev, "clk_mm_emc");
	if (IS_ERR_OR_NULL(pw_info->cam_emc_clk))
		return PTR_ERR(pw_info->cam_emc_clk);

	pw_info->cam_emc_clk_parent =
		devm_clk_get(&pdev->dev, "clk_mm_emc_parent");
	if (IS_ERR_OR_NULL(pw_info->cam_emc_clk_parent))
		return PTR_ERR(pw_info->cam_emc_clk_parent);

	pw_info->cam_emc_clk_default = clk_get_parent(pw_info->cam_emc_clk);
	if (IS_ERR_OR_NULL(pw_info->cam_emc_clk_default))
		return PTR_ERR(pw_info->cam_emc_clk_default);

	/* read global register */
	for (i = 0; i < ARRAY_SIZE(syscon_name); i++) {
		pname = syscon_name[i];
		tregmap =  syscon_regmap_lookup_by_name(np, pname);
		if (IS_ERR_OR_NULL(tregmap)) {
			pr_err("fail to read %s regmap\n", pname);
			continue;
		}
		ret = syscon_get_args_by_name(np, pname, 2, args);
		if (ret != 2) {
			pr_err("fail to read %s args, ret %d\n",
				pname, ret);
			continue;
		}
		pw_info->syscon_regs[i].gpr = tregmap;
		pw_info->syscon_regs[i].reg = args[0];
		pw_info->syscon_regs[i].mask = args[1];
		pr_debug("dts[%s] 0x%x 0x%x\n", pname,
			pw_info->syscon_regs[i].reg,
			pw_info->syscon_regs[i].mask);
	}

	mutex_init(&pw_info->mlock);
	atomic_set(&pw_info->inited, 1);

	return 0;
}

int sprd_cam_pw_off(void)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;
	unsigned int pmu_mm_bit = 0, pmu_mm_state = 0;
	unsigned int mm_off = 0;
	struct register_gpr *preg_gpr;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	pmu_mm_bit = 27;
	pmu_mm_state = 0x1f;
	mm_off = 0x38000000;

	mutex_lock(&pw_info->mlock);
	if (atomic_dec_return(&pw_info->users_pw) == 0) {

		usleep_range(300, 350);

		preg_gpr = &pw_info->syscon_regs[CAMSYS_SHUTDOWN_EN];
		regmap_update_bits(preg_gpr->gpr,
				preg_gpr->reg,
				preg_gpr->mask,
				~preg_gpr->mask);
		preg_gpr = &pw_info->syscon_regs[CAMSYS_FORCE_SHUTDOWN];
		regmap_update_bits(preg_gpr->gpr,
				preg_gpr->reg,
				preg_gpr->mask,
				preg_gpr->mask);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;
			preg_gpr = &pw_info->syscon_regs[CAMSYS_PWR_STATUS0];

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_off;
			power_state1 = val & (pmu_mm_state << pmu_mm_bit);

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_off;
			power_state2 = val & (pmu_mm_state << pmu_mm_bit);

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_off;
			power_state3 = val & (pmu_mm_state << pmu_mm_bit);
		} while (((power_state1 != mm_off) && read_count < 10) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));

		if (power_state1 != mm_off) {
			pr_err("fail to get power state 0x%x\n", power_state1);
			ret = -1;
			goto err_pw_off;
		}
	}
	mutex_unlock(&pw_info->mlock);
	return 0;

err_pw_off:
	pr_err("fail to power off cam sys, ret %d, read count %d\n",
		ret, read_count);
	mutex_unlock(&pw_info->mlock);

	return ret;
}
EXPORT_SYMBOL(sprd_cam_pw_off);

int sprd_cam_pw_on(void)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;
	unsigned int pmu_mm_bit = 0, pmu_mm_state = 0;
	struct register_gpr *preg_gpr;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_info("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	pmu_mm_bit = 27;
	pmu_mm_state = 0x1f;

	mutex_lock(&pw_info->mlock);
	if (atomic_inc_return(&pw_info->users_pw) == 1) {
		preg_gpr = &pw_info->syscon_regs[CAMSYS_INIT_DIS_BITS];
		regmap_update_bits(preg_gpr->gpr,
				preg_gpr->reg,
				preg_gpr->mask,
				~preg_gpr->mask);

		/* cam domain power on */
		preg_gpr = &pw_info->syscon_regs[CAMSYS_SHUTDOWN_EN];
		regmap_update_bits(preg_gpr->gpr,
				preg_gpr->reg,
				preg_gpr->mask,
				~preg_gpr->mask);
		preg_gpr = &pw_info->syscon_regs[CAMSYS_FORCE_SHUTDOWN];
		regmap_update_bits(preg_gpr->gpr,
				preg_gpr->reg,
				preg_gpr->mask,
				~preg_gpr->mask);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;
			preg_gpr = &pw_info->syscon_regs[CAMSYS_PWR_STATUS0];

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_on;
			power_state1 = val & (pmu_mm_state << pmu_mm_bit);

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_on;
			power_state2 = val & (pmu_mm_state << pmu_mm_bit);

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_on;
			power_state3 = val & (pmu_mm_state << pmu_mm_bit);

		} while ((power_state1 && read_count < 10) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));

		if (power_state1) {
			pr_err("fail to get power state 0x%x\n", power_state1);
			ret = -1;
			goto err_pw_on;
		}
	}
	mutex_unlock(&pw_info->mlock);
	return 0;

err_pw_on:
	atomic_dec_return(&pw_info->users_pw);
	pr_err("fail to power on cam sys\n");
	mutex_unlock(&pw_info->mlock);

	return ret;
}
EXPORT_SYMBOL(sprd_cam_pw_on);

int sprd_cam_domain_eb(void)
{
	int ret = 0;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	pr_debug("users count %d, cb %p\n",
		atomic_read(&pw_info->users_clk),
		__builtin_return_address(0));

	mutex_lock(&pw_info->mlock);
	if (atomic_inc_return(&pw_info->users_clk) == 1) {
		/* config cam ahb clk */
		clk_set_parent(pw_info->cam_ahb_clk,
			pw_info->cam_ahb_clk_parent);
		clk_prepare_enable(pw_info->cam_ahb_clk);

		/* config cam emc clk */
		clk_set_parent(pw_info->cam_emc_clk,
			pw_info->cam_emc_clk_parent);
		clk_prepare_enable(pw_info->cam_emc_clk);

		/* mm bus enable */
		clk_prepare_enable(pw_info->cam_mm_eb);

		clk_prepare_enable(pw_info->cam_clk_cphy_cfg_gate_eb);
	}
	mutex_unlock(&pw_info->mlock);

	return 0;
}
EXPORT_SYMBOL(sprd_cam_domain_eb);

int sprd_cam_domain_disable(void)
{
	int ret = 0;
	unsigned int domain_state = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;
	unsigned int pmu_mm_handshake_bit = 0;
	unsigned int pmu_mm_handshake_state = 0;
	unsigned int mm_domain_disable = 0;
	struct register_gpr *preg_gpr;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
	}

	pr_debug("users count %d, cb %p\n",
		atomic_read(&pw_info->users_clk),
		__builtin_return_address(0));

	pmu_mm_handshake_bit = 19;
	pmu_mm_handshake_state = 0x1;
	mm_domain_disable = 0x80000;

	mutex_lock(&pw_info->mlock);
	if (atomic_dec_return(&pw_info->users_clk) == 0) {

		clk_disable_unprepare(pw_info->cam_clk_cphy_cfg_gate_eb);
		clk_disable_unprepare(pw_info->cam_mm_eb);

		while (read_count < 10) {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;
			preg_gpr = &pw_info->syscon_regs[CAMSYS_BUS_STATUS0];

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret) {
				pr_err("fail to read mm handshake %d\n", ret);
				goto err_domain_disable;
			}
			domain_state = val & (pmu_mm_handshake_state <<
				pmu_mm_handshake_bit);
			if (domain_state) {
				pr_debug("wait for done pmu mm handshake0x%x\n",
				domain_state);
				break;
			}
		}
		if (read_count == 10) {
			pr_err("fail to wait for pmu mm handshake 0x%x\n",
				domain_state);
			ret = -1;
			goto err_domain_disable;
		}

		clk_set_parent(pw_info->cam_emc_clk,
			pw_info->cam_emc_clk_default);
		clk_disable_unprepare(pw_info->cam_emc_clk);

		clk_set_parent(pw_info->cam_ahb_clk,
			pw_info->cam_ahb_clk_default);
		clk_disable_unprepare(pw_info->cam_ahb_clk);
	}
	mutex_unlock(&pw_info->mlock);

	return 0;

err_domain_disable:
	pr_err("fail to disable cam power domain, ret %d, count %d\n",
		ret, read_count);
	mutex_unlock(&pw_info->mlock);

	return 0;
}
EXPORT_SYMBOL(sprd_cam_domain_disable);

static int sprd_campw_deinit(struct platform_device *pdev)
{
	devm_kfree(&pdev->dev, pw_info);
	return 0;
}

static int sprd_campw_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = sprd_campw_init(pdev);
	if (ret) {
		pr_err("fail to init cam power domain\n");
		return -ENODEV;
	}

	return ret;
}

static int sprd_campw_remove(struct platform_device *pdev)
{
	sprd_campw_deinit(pdev);

	return 0;
}

static const struct of_device_id sprd_campw_match_table[] = {
	{ .compatible = "sprd,cam-domain", },
	{},
};

static struct platform_driver sprd_campw_driver = {
	.probe = sprd_campw_probe,
	.remove = sprd_campw_remove,
	.driver = {
		.name = "camsys-power",
		.of_match_table = of_match_ptr(sprd_campw_match_table),
	},
};

module_platform_driver(sprd_campw_driver);

MODULE_DESCRIPTION("Camsys Power Driver");
MODULE_AUTHOR("Multimedia_Camera@unisoc.com");
MODULE_LICENSE("GPL");

