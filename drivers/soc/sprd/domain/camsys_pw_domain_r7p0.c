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
#include <asm/cacheflush.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "cam_sys_pw: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static const char * const syscon_name[] = {
	"force_shutdown",
	"shutdown_en", /* clear */
	"power_state", /* on: 0; off:7 */
	"qos_ar",
	"qos_aw",
};

#define PD_MM_DOWN_FLAG    0x7
#define ARQOS_THRESHOLD    0x0D
#define AWQOS_THRESHOLD    0x0D
#define SHIFT_MASK(a)      (ffs(a) ? ffs(a) - 1 : 0)

enum  {
	FORCE_SHUTDOWN = 0,
	SHUTDOWN_EN,
	PWR_STATUS0,
	QOS_AR,
	QOS_AW,
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
	uint8_t mm_qos_ar;
	uint8_t mm_qos_aw;
	struct mutex mlock;

	struct clk *cam_mm_eb;
	struct clk *cam_mm_ahb_eb;

	struct clk *cam_ahb_clk;
	struct clk *cam_ahb_clk_parent;
	struct clk *cam_ahb_clk_default;

	struct clk *cam_mtx_clk;
	struct clk *cam_mtx_clk_parent;
	struct clk *cam_mtx_clk_default;

	struct clk *isppll_clk;

	struct register_gpr regs[ARRAY_SIZE(syscon_name)];
};

static struct camsys_power_info *pw_info;
static BLOCKING_NOTIFIER_HEAD(mmsys_chain);
/* register */
int sprd_mm_pw_notify_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mmsys_chain, nb);
}
EXPORT_SYMBOL(sprd_mm_pw_notify_register);

/* unregister */
int sprd_mm_pw_notify_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mmsys_chain, nb);
}
EXPORT_SYMBOL(sprd_mm_pw_notify_unregister);

static int mmsys_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&mmsys_chain, val, v);
}

static void regmap_update_bits_mmsys(struct register_gpr *p, uint32_t val)
{
	if ((!p) || (!(p->gpr)))
		return;

	regmap_update_bits(p->gpr, p->reg, p->mask, val);
}

static int regmap_read_mmsys(struct register_gpr *p, uint32_t *val)
{
	int ret = 0;

	if ((!p) || (!(p->gpr)) || (!val))
		return -1;
	ret = regmap_read(p->gpr, p->reg, val);
	if (!ret)
		*val &= (uint32_t)p->mask;

	return ret;
}

static int check_drv_init(void)
{
	int ret = 0;

	if (!pw_info)
		ret = -1;
	if (atomic_read(&pw_info->inited) == 0)
		ret = -2;

	return ret;
}

static int sprd_campw_init(struct platform_device *pdev)
{
	int i, ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *np_qos;

	const char *pname;
	struct regmap *tregmap;
	uint32_t args[2];

	pr_info("cam power init begin\n");

	pw_info = devm_kzalloc(&pdev->dev, sizeof(*pw_info), GFP_KERNEL);
	if (!pw_info)
		return -ENOMEM;

	pw_info->cam_mm_eb =
		of_clk_get_by_name(np, "clk_mm_eb");
	if (IS_ERR_OR_NULL(pw_info->cam_mm_eb))
		return PTR_ERR(pw_info->cam_mm_eb);

	pw_info->cam_mm_ahb_eb =
		of_clk_get_by_name(np, "clk_mm_ahb_eb");
	if (IS_ERR_OR_NULL(pw_info->cam_mm_ahb_eb))
		return PTR_ERR(pw_info->cam_mm_ahb_eb);

	pw_info->cam_ahb_clk =
		of_clk_get_by_name(np, "clk_mm_ahb");
	if (IS_ERR_OR_NULL(pw_info->cam_ahb_clk))
		return PTR_ERR(pw_info->cam_ahb_clk);

	pw_info->cam_ahb_clk_parent =
		of_clk_get_by_name(np, "clk_mm_ahb_parent");
	if (IS_ERR_OR_NULL(pw_info->cam_ahb_clk_parent))
		return PTR_ERR(pw_info->cam_ahb_clk_parent);

	pw_info->cam_ahb_clk_default =
		clk_get_parent(pw_info->cam_ahb_clk);
	if (IS_ERR_OR_NULL(pw_info->cam_ahb_clk_default))
		return PTR_ERR(pw_info->cam_ahb_clk_default);

	/* need set cgm_mm_emc_sel :512m , DDR  matrix clk*/
	pw_info->cam_mtx_clk =
		of_clk_get_by_name(np, "clk_mm_mtx");
	if (IS_ERR_OR_NULL(pw_info->cam_mtx_clk))
		return PTR_ERR(pw_info->cam_mtx_clk);

	pw_info->cam_mtx_clk_parent =
		of_clk_get_by_name(np, "clk_mm_mtx_parent");
	if (IS_ERR_OR_NULL(pw_info->cam_mtx_clk_parent))
		return PTR_ERR(pw_info->cam_mtx_clk_parent);

	pw_info->cam_mtx_clk_default =
		clk_get_parent(pw_info->cam_mtx_clk);
	if (IS_ERR_OR_NULL(pw_info->cam_mtx_clk_default))
		return PTR_ERR(pw_info->cam_mtx_clk_default);

	pw_info->isppll_clk = of_clk_get_by_name(np, "clk_isppll");
	if (IS_ERR_OR_NULL(pw_info->isppll_clk))
		return PTR_ERR(pw_info->isppll_clk);

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
		pw_info->regs[i].gpr = tregmap;
		pw_info->regs[i].reg = args[0];
		pw_info->regs[i].mask = args[1];
		pr_info("dts[%s] 0x%x 0x%x\n", pname,
			pw_info->regs[i].reg,
			pw_info->regs[i].mask);
	}

	np_qos = of_parse_phandle(np, "mm_qos_threshold", 0);
	if (!IS_ERR_OR_NULL(np_qos)) {
		/* read qos ar aw */
		ret = of_property_read_u8(np_qos, "arqos-threshold",
			&pw_info->mm_qos_ar);
		if (ret) {
			pw_info->mm_qos_ar = ARQOS_THRESHOLD;
			pr_warn("read arqos-threshold fail, default %d\n",
				pw_info->mm_qos_ar);
		}
		ret = of_property_read_u8(np_qos, "awqos-threshold",
			&pw_info->mm_qos_aw);
		if (ret) {
			pw_info->mm_qos_aw = AWQOS_THRESHOLD;
			pr_warn("read awqos-threshold fail, default %d\n",
				pw_info->mm_qos_aw);
		}
	} else {
		pw_info->mm_qos_ar = ARQOS_THRESHOLD;
		pw_info->mm_qos_aw = AWQOS_THRESHOLD;
		pr_info("read mm qos threshold fail, default[%x %x]\n",
			pw_info->mm_qos_ar, pw_info->mm_qos_aw);
	}

	mutex_init(&pw_info->mlock);
	atomic_set(&pw_info->inited, 1);

	pr_info("cam power init end\n");

	return 0;
}

int sprd_cam_pw_off(void)
{
	int ret = 0;
	unsigned int power_state1;
	unsigned int power_state2;
	unsigned int power_state3;
	unsigned int read_count = 0;
	int shift = 0;

	ret = check_drv_init();
	if (ret) {
		pr_err("uses: %d, cb: %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	mutex_lock(&pw_info->mlock);
	if (atomic_dec_return(&pw_info->users_pw) == 0) {
		/* 1:auto shutdown en, shutdown with ap; 0: control by b25 */
		regmap_update_bits_mmsys(&pw_info->regs[SHUTDOWN_EN],
			0);
		/* set 1 to shutdown */
		regmap_update_bits_mmsys(&pw_info->regs[FORCE_SHUTDOWN],
			~((uint32_t)0));
		/* shift for power off status bits */
		if (pw_info->regs[PWR_STATUS0].gpr != NULL)
			shift = SHIFT_MASK(pw_info->regs[PWR_STATUS0].mask);
		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state1);
			if (ret)
				goto err_pw_off;
			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state2);
			if (ret)
				goto err_pw_off;
			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state3);
			if (ret)
				goto err_pw_off;
		} while (((power_state1 != (PD_MM_DOWN_FLAG << shift)) &&
			(read_count < 30)) ||
				(power_state1 != power_state2) ||
				(power_state2 != power_state3));
		if (power_state1 != (PD_MM_DOWN_FLAG << shift)) {
			pr_err("failed, power_state1=0x%x\n", power_state1);
			ret = -1;
			goto err_pw_off;
		}
	}
	mutex_unlock(&pw_info->mlock);
	/* if count != 0, other using */
	pr_info("Done, read count %d, cb: %p\n",
		read_count, __builtin_return_address(0));

	return 0;

err_pw_off:
	mutex_unlock(&pw_info->mlock);
	pr_err("failed, ret: %d, count: %d, cb: %p\n", ret, read_count,
		__builtin_return_address(0));

	return ret;
}
EXPORT_SYMBOL(sprd_cam_pw_off);

int sprd_cache_flush(void *work_buf_vaddr, int pase_size)
{
#ifdef CONFIG_64BIT
	__flush_dcache_area(work_buf_vaddr, pase_size);
#else
	flush_kernel_vmap_range(work_buf_vaddr, pase_size);
#endif
	return 0;
}
EXPORT_SYMBOL(sprd_cache_flush);

int sprd_cam_pw_on(void)
{
	int ret = 0;
	unsigned int power_state1;
	unsigned int power_state2;
	unsigned int power_state3;
	unsigned int read_count = 0;

	pr_info("sprd cam pw on\n");

	ret = check_drv_init();
	if (ret) {
		pr_info("uses: %d, cb: %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	mutex_lock(&pw_info->mlock);
	if (atomic_inc_return(&pw_info->users_pw) == 1) {
		/* clear force shutdown */
		regmap_update_bits_mmsys(&pw_info->regs[FORCE_SHUTDOWN], 0);
		/* power on */
		regmap_update_bits_mmsys(&pw_info->regs[SHUTDOWN_EN], 0);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state1);
			if (ret)
				goto err_pw_on;
			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state2);
			if (ret)
				goto err_pw_on;
			ret = regmap_read_mmsys(&pw_info->regs[PWR_STATUS0],
					&power_state3);
			if (ret)
				goto err_pw_on;
		} while ((power_state1 && read_count < 30) ||
				(power_state1 != power_state2) ||
				(power_state2 != power_state3));

		if (power_state1) {
			pr_err("cam domain pw on failed 0x%x\n", power_state1);
			ret = -1;
			goto err_pw_on;
		}
	}
	mutex_unlock(&pw_info->mlock);
	/* if count != 0, other using */
	pr_info("Done, uses: %d, read count %d, cb: %p\n",
		atomic_read(&pw_info->users_pw), read_count,
		__builtin_return_address(0));
	pr_info("sprd cam pw on end\n");
	return 0;
err_pw_on:
	atomic_dec_return(&pw_info->users_pw);
	mutex_unlock(&pw_info->mlock);
	pr_info("cam domain, failed to power on, ret = %d\n", ret);
	return ret;
}
EXPORT_SYMBOL(sprd_cam_pw_on);

int sprd_cam_domain_eb(void)
{
	int ret = 0;
	uint32_t tmp = 0;
	ret = check_drv_init();
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
		clk_prepare_enable(pw_info->cam_mm_ahb_eb);
		/* config cam ahb clk */
		clk_set_parent(pw_info->cam_ahb_clk,
			pw_info->cam_ahb_clk_parent);
		clk_prepare_enable(pw_info->cam_ahb_clk);

		/* config cam mtx clk */
		clk_set_parent(pw_info->cam_mtx_clk,
			pw_info->cam_mtx_clk_parent);
		clk_prepare_enable(pw_info->cam_mtx_clk);

		clk_prepare_enable(pw_info->isppll_clk);

		/* Qos ar */
		tmp = pw_info->mm_qos_ar;
		regmap_update_bits_mmsys(&pw_info->regs[QOS_AR],
			tmp << SHIFT_MASK(pw_info->regs[QOS_AR].mask));
		/* Qos aw */
		tmp = pw_info->mm_qos_aw;
		regmap_update_bits_mmsys(&pw_info->regs[QOS_AW],
			tmp << SHIFT_MASK(pw_info->regs[QOS_AW].mask));

		mmsys_notifier_call_chain(_E_PW_ON, NULL);
	}
	mutex_unlock(&pw_info->mlock);
	return 0;
}
EXPORT_SYMBOL(sprd_cam_domain_eb);

int sprd_cam_domain_disable(void)
{
	int ret = 0;
	ret = check_drv_init();
	if (ret) {
		pr_err("fail to get init state %d, cb %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
	}

	pr_debug("users count %d, cb %p\n",
		atomic_read(&pw_info->users_clk),
		__builtin_return_address(0));

	mutex_lock(&pw_info->mlock);
	if (atomic_dec_return(&pw_info->users_clk) == 0) {
		mmsys_notifier_call_chain(_E_PW_OFF, NULL);

		clk_disable_unprepare(pw_info->isppll_clk);

		clk_set_parent(pw_info->cam_ahb_clk,
			pw_info->cam_ahb_clk_default);
		clk_disable_unprepare(pw_info->cam_ahb_clk);

		clk_set_parent(pw_info->cam_mtx_clk,
			pw_info->cam_mtx_clk_default);
		clk_disable_unprepare(pw_info->cam_mtx_clk);

		clk_disable_unprepare(pw_info->cam_mm_ahb_eb);
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

	pr_info("cam power probe begin\n");
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
	{ .compatible = "sprd,mm-domain", },
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
