/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>
#include <dt-bindings/soc/sprd,pike2-mask.h>
#include <dt-bindings/soc/sprd,pike2-regs.h>
#include <video/sprd_mmsys_pw_domain.h>

struct cam_pw_domain_info {
	atomic_t users_pw;
	atomic_t users_clk;

	unsigned int chip_id0;
	unsigned int chip_id1;

	struct mutex client_lock;
	struct regmap *cam_ahb_gpr;
	struct regmap *pmu_apb_gpr;
	struct regmap *aon_apb_gpr;

	struct clk *cam_ckg_eb;
	struct clk *cam_mm_eb;
	struct clk *cam_ahb_clk;
	struct clk *cam_ahb_clk_default;
	struct clk *cam_ahb_clk_parent;
};

#define PD_MM_STAT_BIT_SHIFT 28
#define BIT_PMU_APB_PD_MM_SYS_STATE(x)	(((x) & 0xf) << PD_MM_STAT_BIT_SHIFT)
#define PD_MM_DOWN_FLAG (0x7 << PD_MM_STAT_BIT_SHIFT)

static struct cam_pw_domain_info *cam_pw;

static int sprd_cam_pw_domain_init(struct platform_device *pdev)
{
	int ret = 0;
	struct regmap *cam_ahb_gpr = NULL;
	struct regmap *aon_apb_gpr = NULL;
	struct regmap *pmu_apb_gpr = NULL;
	unsigned int chip_id0 = 0, chip_id1 = 0;

	pr_info("cam_pw_domain: cam_pw_domain_init.\n");

	cam_pw = devm_kzalloc(&pdev->dev, sizeof(*cam_pw), GFP_KERNEL);
	if (!cam_pw)
		return -ENOMEM;

	cam_pw->cam_ckg_eb = devm_clk_get(&pdev->dev, "clk_gate_eb");
	if (IS_ERR(cam_pw->cam_ckg_eb)) {
		pr_err("cam pw domain init fail, cam_ckg_eb\n");
		return PTR_ERR(cam_pw->cam_ckg_eb);
	}

	cam_pw->cam_mm_eb = devm_clk_get(&pdev->dev, "clk_mm_eb");
	if (IS_ERR(cam_pw->cam_mm_eb)) {
		pr_err("cam pw domain init fail, cam_mm_eb\n");
		return PTR_ERR(cam_pw->cam_mm_eb);
	}

	cam_pw->cam_ahb_clk = devm_clk_get(&pdev->dev, "clk_mm_ahb");
	if (IS_ERR(cam_pw->cam_ahb_clk)) {
		pr_err("cam pw domain init fail, cam_ahb_clk\n");
		return PTR_ERR(cam_pw->cam_ahb_clk);
	}

	cam_pw->cam_ahb_clk_parent =
		devm_clk_get(&pdev->dev, "clk_mm_ahb_parent");
	if (IS_ERR(cam_pw->cam_ahb_clk_parent)) {
		pr_err("cam pw domain init fail, cam_ahb_clk_parent\n");
		return PTR_ERR(cam_pw->cam_ahb_clk_parent);
	}

	cam_pw->cam_ahb_clk_default = clk_get_parent(cam_pw->cam_ahb_clk);
	if (IS_ERR(cam_pw->cam_ahb_clk_default)) {
		pr_err("cam pw domain init fail, cam_ahb_clk_default\n");
		return PTR_ERR(cam_pw->cam_ahb_clk_default);
	}

	cam_ahb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						      "sprd,cam-ahb-syscon");
	if (IS_ERR(cam_ahb_gpr)) {
		pr_err("cam pw domain init fail, cam_ahb_gpr\n");
		return PTR_ERR(cam_ahb_gpr);
	}
	cam_pw->cam_ahb_gpr = cam_ahb_gpr;

	aon_apb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						      "sprd,aon-apb-syscon");
	if (IS_ERR(aon_apb_gpr)) {
		pr_err("cam pw domain init fail, aon_apb_gpr\n");
		return PTR_ERR(aon_apb_gpr);
	}
	cam_pw->aon_apb_gpr = aon_apb_gpr;

	pmu_apb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						      "sprd,syscon-pmu-apb");
	if (IS_ERR(pmu_apb_gpr)) {
		pr_err("cam pw domain init fail, pmu_apb_gpr\n");
		return PTR_ERR(pmu_apb_gpr);
	}
	cam_pw->pmu_apb_gpr = pmu_apb_gpr;

	ret = regmap_read(aon_apb_gpr, REG_AON_APB_AON_CHIP_ID0, &chip_id0);
	if (ret) {
		cam_pw->chip_id0 = 0;
		pr_err("Read chip id0 error\n");
	} else
		cam_pw->chip_id0 = chip_id0;

	ret = regmap_read(aon_apb_gpr, REG_AON_APB_AON_CHIP_ID1, &chip_id1);
	if (ret) {
		cam_pw->chip_id1 = 0;
		pr_err("Read chip id1 error\n");
	} else
		cam_pw->chip_id1 = chip_id1;

	pr_info("chip_id0 %x, chip_id1 %x\n", chip_id0, chip_id1);

	mutex_init(&cam_pw->client_lock);

	return 0;
}

static int sprd_cam_pw_domain_deinit(struct platform_device *pdev)
{
	devm_kfree(&pdev->dev, cam_pw);
	cam_pw = NULL;

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

	pr_info("%s, count:%d, cb: %pS\n", __func__,
			atomic_read(&cam_pw->users_pw),
			__builtin_return_address(0));

	mutex_lock(&cam_pw->client_lock);

	if (atomic_dec_return(&cam_pw->users_pw) == 0) {
		clk_disable_unprepare(cam_pw->cam_mm_eb);
		usleep_range(300, 350);
		regmap_update_bits(cam_pw->pmu_apb_gpr,
				   REG_PMU_APB_PD_MM_TOP_CFG,
				   MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN,
				   ~(unsigned int)
				   MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN);
		regmap_update_bits(cam_pw->pmu_apb_gpr,
				   REG_PMU_APB_PD_MM_TOP_CFG,
				   MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN,
				   MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					  REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_off;
			power_state1 = val & BIT_PMU_APB_PD_MM_SYS_STATE(0xf);

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					  REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_off;
			power_state2 = val & BIT_PMU_APB_PD_MM_SYS_STATE(0xf);

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					  REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_off;
			power_state3 = val & BIT_PMU_APB_PD_MM_SYS_STATE(0xf);
		} while (((power_state1 != PD_MM_DOWN_FLAG) &&
			read_count < 10) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));

		if (power_state1 != PD_MM_DOWN_FLAG) {
			pr_err("cam domain pw off failed 0x%x\n", power_state1);
			ret = -1;
			goto err_pw_off;
		}
	} else {
		pr_info("cam domain, other camera module is working\n");
	}
	mutex_unlock(&cam_pw->client_lock);
	return 0;

err_pw_off:
	pr_err("cam domain pw off failed, ret: %d, count: %d!\n",
	       ret, read_count);
	mutex_unlock(&cam_pw->client_lock);
	return 0;
}
EXPORT_SYMBOL(sprd_cam_pw_off);

static void sprd_mm_lpc_ctrl(void)
{
	unsigned int val = 0;

	pr_info("open mm lpc\n");
	/* add lpc and light sleep */
	if (regmap_read(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL, &val))
		goto err_exit;
	val |= MASK_MM_AHB_CAM_MTX_LPC_EB | 0x20;
	regmap_write(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL, val);

	if (regmap_read(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL_M0, &val))
		goto err_exit;
	val |= MASK_MM_AHB_CAM_MTX_M0_LPC_EB | 0x20;
	regmap_write(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL_M0, val);

	if (regmap_read(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL_M1, &val))
		goto err_exit;
	val |= MASK_MM_AHB_CAM_MTX_M1_LPC_EB | 0x20;
	regmap_write(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL_M1, val);

	if (regmap_read(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_M0, &val))
		goto err_exit;
	val |= MASK_MM_AHB_VSP_MTX_M0_LPC_EB | 0x20;
	regmap_write(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_M0, val);

	if (regmap_read(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_M1, &val))
		goto err_exit;
	val |= MASK_MM_AHB_VSP_MTX_M1_LPC_EB | 0x20;
	regmap_write(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_M1, val);

	if (regmap_read(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_S0, &val))
		goto err_exit;
	val |= MASK_MM_AHB_VSP_MTX_S0_LPC_EB | 0x20;
	regmap_write(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_S0, val);

	if (regmap_read(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_GPV, &val))
		goto err_exit;
	val |= MASK_MM_AHB_VSP_MTX_GPV_LPC_EB | 0x20;
	regmap_write(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_GPV, val);

	if (regmap_read(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_MM_LIGHT_SLEEP_CTRL, &val))
		goto err_exit;
	val |= MASK_MM_AHB_REG_VSP_MTX_FRC_LSLP_M1 |
			MASK_MM_AHB_REG_VSP_MTX_FRC_LSLP_M0 |
			MASK_MM_AHB_REG_CAM_MTX_FRC_LSLP_M1 |
			MASK_MM_AHB_REG_CAM_MTX_FRC_LSLP_M0;
	regmap_write(cam_pw->cam_ahb_gpr,
			REG_MM_AHB_MM_LIGHT_SLEEP_CTRL, val);
	return;

err_exit:
	pr_err("reg config fail\n");
}

int sprd_cam_pw_on(void)
{
	int ret = 0;
	unsigned int power_state1, power_state2, power_state3;
	unsigned int read_count = 0;
	unsigned int val = 0;

	pr_info("%s, count:%d, cb: %pS\n", __func__,
			atomic_read(&cam_pw->users_pw),
			__builtin_return_address(0));

	mutex_lock(&cam_pw->client_lock);

	if (atomic_inc_return(&cam_pw->users_pw) == 1) {
		/* cam domain power on */
		regmap_update_bits(cam_pw->pmu_apb_gpr,
				   REG_PMU_APB_PD_MM_TOP_CFG,
				   MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN,
				   ~(unsigned int)
				   MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN);
		regmap_update_bits(cam_pw->pmu_apb_gpr,
				   REG_PMU_APB_PD_MM_TOP_CFG,
				   MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN,
				   ~(unsigned int)
				   MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					  REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_on;
			power_state1 = val & BIT_PMU_APB_PD_MM_SYS_STATE(0xf);

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					  REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_on;
			power_state2 = val & BIT_PMU_APB_PD_MM_SYS_STATE(0xf);

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					  REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_on;
			power_state3 = val & BIT_PMU_APB_PD_MM_SYS_STATE(0xf);

		} while ((power_state1 && read_count < 10) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));

		if (power_state1) {
			pr_err("cam domain pw on failed 0x%x\n", power_state1);
			ret = -1;
			goto err_pw_on;
		}

		/* mm bus enable */
		clk_prepare_enable(cam_pw->cam_mm_eb);
		udelay(50);
		sprd_mm_lpc_ctrl();
		pr_info("cam_pw_domain:cam_pw_on set OK.\n");
	} else {
		pr_info("cam_pw_domain:cam_domain is already power on.\n");
	}
	mutex_unlock(&cam_pw->client_lock);

	return 0;
err_pw_on:
	atomic_dec_return(&cam_pw->users_pw);
	pr_info("cam domain, failed to power on\n");
	mutex_unlock(&cam_pw->client_lock);

	return ret;
}
EXPORT_SYMBOL(sprd_cam_pw_on);

int sprd_cam_domain_eb(void)
{
	unsigned int rst_bit = 0;
	unsigned int eb_bit = 0;

	pr_info("%s, count:%d, cb: %pS\n", __func__,
			atomic_read(&cam_pw->users_clk),
			__builtin_return_address(0));

	mutex_lock(&cam_pw->client_lock);
	if (atomic_inc_return(&cam_pw->users_clk) == 1) {
		/* mm bus enable */
		/*clk_prepare_enable(cam_pw->cam_mm_eb);*/

		/* cam CKG enable */
		clk_prepare_enable(cam_pw->cam_ckg_eb);

		/* config cam ahb clk */
		clk_set_parent(cam_pw->cam_ahb_clk, cam_pw->cam_ahb_clk_parent);
		clk_prepare_enable(cam_pw->cam_ahb_clk);

		eb_bit = MASK_MM_AHB_DCAM_EB | MASK_MM_AHB_ISP_EB |
			MASK_MM_AHB_CSI_EB | MASK_MM_AHB_CKG_EB;
		regmap_update_bits(cam_pw->cam_ahb_gpr,
				REG_MM_AHB_AHB_EB,
				eb_bit,
				eb_bit);

		rst_bit =
			MASK_MM_AHB_AHB_CKG_SOFT_RST |
			MASK_MM_AHB_AXI_CAM_MTX_SOFT_RST;

		regmap_update_bits(cam_pw->cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				rst_bit,
				rst_bit);
		udelay(1);
		regmap_update_bits(cam_pw->cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				rst_bit,
				~rst_bit);
		/* clock for anlg_phy_g7_controller */
		regmap_update_bits(cam_pw->aon_apb_gpr,
			REG_AON_APB_APB_EB2,
			MASK_AON_APB_ANLG_APB_EB,
			MASK_AON_APB_ANLG_APB_EB);

	}
	mutex_unlock(&cam_pw->client_lock);

	return 0;
}
EXPORT_SYMBOL(sprd_cam_domain_eb);

int sprd_cam_domain_disable(void)
{
	pr_info("%s, count:%d, cb: %pS\n", __func__,
			atomic_read(&cam_pw->users_clk),
			__builtin_return_address(0));

	mutex_lock(&cam_pw->client_lock);

	if (atomic_dec_return(&cam_pw->users_clk) == 0) {
		clk_set_parent(cam_pw->cam_ahb_clk,
			       cam_pw->cam_ahb_clk_default);
		clk_disable_unprepare(cam_pw->cam_ahb_clk);

		clk_disable_unprepare(cam_pw->cam_ckg_eb);

		/*clk_disable_unprepare(cam_pw->cam_mm_eb);*/
	}
	mutex_unlock(&cam_pw->client_lock);

	return 0;
}
EXPORT_SYMBOL(sprd_cam_domain_disable);

static int sprd_campw_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = sprd_cam_pw_domain_init(pdev);
	if (ret) {
		pr_err("fail to init cam power domain\n");
		return -ENODEV;
	}

	return ret;
}

static int sprd_campw_remove(struct platform_device *pdev)
{
	sprd_cam_pw_domain_deinit(pdev);

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
MODULE_AUTHOR("langbiao.tan@unisoc.com");
MODULE_LICENSE("GPL");
