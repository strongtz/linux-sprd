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
	"pd_mm_state",
	"anlg_apb_eb",
	"qos_threshold_mm"
};

enum  {
	CAMSYS_SHUTDOWN_EN = 0,
	CAMSYS_FORCE_SHUTDOWN,
	CAMSYS_PD_MM_STATE,
	CAMSYS_ANLG_APB_EB,
	CAMSYS_QOS_THRESHOLD_MM
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
	struct clk *cam_ckg_eb;
	struct clk *cam_mm_eb;

	struct clk *cam_ahb_clk;
	struct clk *cam_ahb_clk_default;
	struct clk *cam_ahb_clk_parent;

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

	pw_info->cam_ckg_eb = devm_clk_get(&pdev->dev, "clk_gate_eb");
	if (IS_ERR(pw_info->cam_ckg_eb))
		return PTR_ERR(pw_info->cam_ckg_eb);

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
		pr_info("dts[%s] 0x%x 0x%x\n", pname,
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
	unsigned int pd_off_state = 0;
	struct register_gpr *preg_gpr;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	pd_off_state = 0x7 << 10;

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
			preg_gpr = &pw_info->syscon_regs[CAMSYS_PD_MM_STATE];

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_off;
			power_state1 = val & preg_gpr->mask;

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_off;
			power_state2 = val & preg_gpr->mask;

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_off;
			power_state3 = val & preg_gpr->mask;
		} while (((power_state1 != pd_off_state) && read_count < 10) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));

		if (power_state1 != pd_off_state) {
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
	struct register_gpr *preg_gpr;

	ret = sprd_campw_check_drv_init();
	if (ret) {
		pr_info("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	mutex_lock(&pw_info->mlock);
	if (atomic_inc_return(&pw_info->users_pw) == 1) {
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
			preg_gpr = &pw_info->syscon_regs[CAMSYS_PD_MM_STATE];

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_on;
			power_state1 = val & preg_gpr->mask;

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_on;
			power_state2 = val & preg_gpr->mask;

			ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
			if (ret)
				goto err_pw_on;
			power_state3 = val & preg_gpr->mask;

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
	unsigned int rst_bit;
	struct register_gpr *preg_gpr;

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
		/* mm bus enable */
		clk_prepare_enable(pw_info->cam_mm_eb);

		/* cam CKG enable */
		clk_prepare_enable(pw_info->cam_ckg_eb);

		clk_prepare_enable(pw_info->cam_clk_cphy_cfg_gate_eb);

		/* config cam ahb clk */
		clk_set_parent(pw_info->cam_ahb_clk,
			pw_info->cam_ahb_clk_parent);
		clk_prepare_enable(pw_info->cam_ahb_clk);

		/* clock for anlg_phy_g7_controller */
		preg_gpr = &pw_info->syscon_regs[CAMSYS_ANLG_APB_EB];
		regmap_update_bits(preg_gpr->gpr,
				preg_gpr->reg,
				preg_gpr->mask,
				preg_gpr->mask);

		rst_bit = (0xd << 4) | 0xd;
		preg_gpr = &pw_info->syscon_regs[CAMSYS_QOS_THRESHOLD_MM];
		regmap_update_bits(preg_gpr->gpr,
				preg_gpr->reg,
				preg_gpr->mask,
				rst_bit);
	}
	mutex_unlock(&pw_info->mlock);

	return 0;
}
EXPORT_SYMBOL(sprd_cam_domain_eb);

int sprd_cam_domain_disable(void)
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
	if (atomic_dec_return(&pw_info->users_clk) == 0) {
		clk_set_parent(pw_info->cam_ahb_clk,
			       pw_info->cam_ahb_clk_default);
		clk_disable_unprepare(pw_info->cam_ahb_clk);
		clk_disable_unprepare(pw_info->cam_clk_cphy_cfg_gate_eb);
		clk_disable_unprepare(pw_info->cam_ckg_eb);
		clk_disable_unprepare(pw_info->cam_mm_eb);
	}
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

