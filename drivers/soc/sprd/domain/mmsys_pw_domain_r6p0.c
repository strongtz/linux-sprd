/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
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
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <video/sprd_mmsys_pw_domain.h>


/* Macro Definitions */
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "MM_PW: %d %d %s: " \
	fmt, current->pid, __LINE__, __func__


static const char * const tb_name[] = {
	"force_shutdown",
	"shutdown_en", /* clear */
	"power_state", /* on: 0; off:7 */
	"qos_ar",
	"qos_aw",

};
enum  {
	_e_force_shutdown,
	_e_auto_shutdown,
	_e_power_state,
	_e_qos_ar,
	_e_qos_aw,
};

struct register_gpr {
	struct regmap *gpr;
	uint32_t reg;
	uint32_t mask;
};

struct mmsys_power_info {
	struct mutex mlock;
	atomic_t users_pw;
	atomic_t users_clk;
	atomic_t inited;
	uint8_t mm_qos_ar, mm_qos_aw;
	struct register_gpr regs[sizeof(tb_name) / sizeof(void *)];

	struct clk *mm_eb;
	struct clk *mm_ahb_eb;
	struct clk *ahb_clk;
	struct clk *ahb_clk_parent;
	struct clk *ahb_clk_default;

	struct clk *mm_mtx_eb;
	struct clk *mtx_clk;
	struct clk *mtx_clk_parent;
	struct clk *mtx_clk_default;
};

#define PD_MM_DOWN_FLAG			0x7
#define ARQOS_THRESHOLD			0x0D
#define AWQOS_THRESHOLD			0x0D
#define SHIFT_MASK(a)			(ffs(a) ? ffs(a) - 1 : 0)
static struct mmsys_power_info *pw_info;
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

static int mmsys_power_init(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *np_qos;
	struct regmap *tregmap;
	uint32_t syscon_args[2];
	const char *pname;
	int i = 0;
	int ret = 0;

	pw_info = devm_kzalloc(&pdev->dev, sizeof(*pw_info), GFP_KERNEL);
	if (!pw_info)
		return -ENOMEM;

	pr_info("E\n");
	mutex_init(&pw_info->mlock);
	/* read global register */
	for (i = 0; i < ARRAY_SIZE(tb_name); i++) {
		pname = tb_name[i];
		tregmap =  syscon_regmap_lookup_by_name(np, pname);
		if (IS_ERR_OR_NULL(tregmap)) {
			/* domain work normal when remove some item from dts */
			pr_warn("Read DTS %s regmap fail\n", pname);
			continue;
		}
		ret = syscon_get_args_by_name(np, pname, 2, syscon_args);
		if (ret != 2) {
			pr_warn("Read DTS %s args fail, ret = %d\n",
				pname, ret);
			continue;
		}
		pw_info->regs[i].gpr = tregmap;
		pw_info->regs[i].reg = syscon_args[0];
		pw_info->regs[i].mask = syscon_args[1];
		pr_debug("DTS[%s]%p, 0x%x, 0x%x\n", pname,
			pw_info->regs[i].gpr, pw_info->regs[i].reg,
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
		pr_warn("read mm qos threshold fail, default[%x %x]\n",
			pw_info->mm_qos_ar, pw_info->mm_qos_aw);
	}

	ret = 0;
	/* read clk */
	pw_info->mm_eb = of_clk_get_by_name(np, "mm_eb");
	if (IS_ERR_OR_NULL(pw_info->mm_eb)) {
		pr_err("Get mm_eb clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->mm_eb));
		ret |= BIT(0);
	}
	pw_info->mm_ahb_eb = of_clk_get_by_name(np, "mm_ahb_eb");
	if (IS_ERR_OR_NULL(pw_info->mm_ahb_eb)) {
		pr_err("Get mm_ahb_eb clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->mm_ahb_eb));
		ret |= BIT(1);
	}
	pw_info->ahb_clk = of_clk_get_by_name(np, "clk_mm_ahb");
	if (IS_ERR_OR_NULL(pw_info->ahb_clk)) {
		pr_err("Get clk_mm_ahb clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->ahb_clk));
		ret |= BIT(2);
	}
	pw_info->ahb_clk_parent = of_clk_get_by_name(np, "clk_mm_ahb_parent");
	if (IS_ERR_OR_NULL(pw_info->ahb_clk_parent)) {
		pr_err("Get mm_ahb_parent clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->ahb_clk_parent));
		ret |= BIT(3);
	}
	pw_info->ahb_clk_default = clk_get_parent(pw_info->ahb_clk);
	/* read mm mtx clk */
	pw_info->mm_mtx_eb = of_clk_get_by_name(np, "mm_mtx_eb");
	if (IS_ERR_OR_NULL(pw_info->mm_mtx_eb)) {
		pr_err("Get mm_mtx_eb clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->mm_mtx_eb));
		ret |= BIT(4);
	}
	pw_info->mtx_clk = of_clk_get_by_name(np, "clk_mm_mtx");
	if (IS_ERR_OR_NULL(pw_info->mtx_clk)) {
		pr_err("Get clk_mm_mtx clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->mtx_clk));
		ret |= BIT(5);
		}
	pw_info->mtx_clk_parent = of_clk_get_by_name(np, "clk_mm_mtx_parent");
	if (IS_ERR_OR_NULL(pw_info->mtx_clk_parent)) {
		pr_err("Get clk_mm_mtx_parent clk fail, ret %d\n",
			(int)PTR_ERR(pw_info->mtx_clk_parent));
		ret |= BIT(6);
	}
	pw_info->mtx_clk_default = clk_get_parent(pw_info->mtx_clk);
	if (ret) {
		atomic_set(&pw_info->inited, 0);
		pr_err("ret = 0x%x\n", ret);
	} else {
		atomic_set(&pw_info->inited, 1);
		pr_info("Read DTS OK\n");
	}

	return ret;
}


static int mmsys_power_deinit(struct platform_device *pdev)
{
	pr_debug("Exit\n");
	/* kfree(pw_info); */
	devm_kfree(&pdev->dev, pw_info);

	return 0;
}

int sprd_cam_pw_on(void)
{
	int ret = 0;
	unsigned int power_state1;
	unsigned int power_state2;
	unsigned int power_state3;
	unsigned int read_count = 0;

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
		regmap_update_bits_mmsys(&pw_info->regs[_e_force_shutdown], 0);
		/* power on */
		regmap_update_bits_mmsys(&pw_info->regs[_e_auto_shutdown], 0);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(&pw_info->regs[_e_power_state],
					&power_state1);
			if (ret)
				goto err_pw_on;

			ret = regmap_read_mmsys(&pw_info->regs[_e_power_state],
					&power_state2);
			if (ret)
				goto err_pw_on;

			ret = regmap_read_mmsys(&pw_info->regs[_e_power_state],
					&power_state3);
			if (ret)
				goto err_pw_on;

		} while ((power_state1 && read_count < 10) ||
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

	return 0;

err_pw_on:
	atomic_dec_return(&pw_info->users_pw);
	mutex_unlock(&pw_info->mlock);
	pr_info("cam domain, failed to power on, ret = %d\n", ret);

	return ret;

}
EXPORT_SYMBOL(sprd_cam_pw_on);

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
		regmap_update_bits_mmsys(&pw_info->regs[_e_auto_shutdown], 0);
		/* set 1 to shutdown */
		regmap_update_bits_mmsys(&pw_info->regs[_e_force_shutdown],
			~((uint32_t)0));
		/* shift for power off status bits */
		if (pw_info->regs[_e_power_state].gpr != NULL)
			shift = SHIFT_MASK(pw_info->regs[_e_power_state].mask);
		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read_mmsys(&pw_info->regs[_e_power_state],
					&power_state1);
			if (ret)
				goto err_pw_off;
			ret = regmap_read_mmsys(&pw_info->regs[_e_power_state],
					&power_state2);
			if (ret)
				goto err_pw_off;

			ret = regmap_read_mmsys(&pw_info->regs[_e_power_state],
					&power_state3);
			if (ret)
				goto err_pw_off;

		} while (((power_state1 != (PD_MM_DOWN_FLAG << shift)) &&
			(read_count < 10)) ||
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


int sprd_cam_domain_eb(void)
{
	uint32_t tmp = 0;
	int ret = 0;

	ret = check_drv_init();
	if (ret) {
		pr_err("uses: %d, cb: %p, ret %d\n",
			atomic_read(&pw_info->users_pw),
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	pr_debug("clk users count:%d, cb: %p\n",
		atomic_read(&pw_info->users_clk),
		__builtin_return_address(0));

	mutex_lock(&pw_info->mlock);

	if (atomic_inc_return(&pw_info->users_clk) == 1) {
		/* enable */
		clk_prepare_enable(pw_info->mm_eb);
		clk_prepare_enable(pw_info->mm_ahb_eb);
		clk_prepare_enable(pw_info->mm_mtx_eb);
		/* ahb clk */
		clk_set_parent(pw_info->ahb_clk, pw_info->ahb_clk_parent);
		clk_prepare_enable(pw_info->ahb_clk);
		/* mm mtx clk */
		clk_set_parent(pw_info->mtx_clk, pw_info->mtx_clk_parent);
		clk_prepare_enable(pw_info->mtx_clk);

		/* Qos ar */
		tmp = pw_info->mm_qos_ar;
		regmap_update_bits_mmsys(&pw_info->regs[_e_qos_ar],
			tmp << SHIFT_MASK(pw_info->regs[_e_qos_ar].mask));
		/* Qos aw */
		tmp = pw_info->mm_qos_aw;
		regmap_update_bits_mmsys(&pw_info->regs[_e_qos_aw],
			tmp << SHIFT_MASK(pw_info->regs[_e_qos_aw].mask));

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
		pr_err("cb: %p, inited %d\n",
			__builtin_return_address(0), ret);
	}

	pr_debug("clk users count: %d, cb: %p\n",
		atomic_read(&pw_info->users_clk),
		__builtin_return_address(0));
	mutex_lock(&pw_info->mlock);
	if (atomic_dec_return(&pw_info->users_clk) == 0) {
		mmsys_notifier_call_chain(_E_PW_OFF, NULL);
		/* ahb clk */
		clk_set_parent(pw_info->ahb_clk, pw_info->ahb_clk_default);
		clk_disable_unprepare(pw_info->ahb_clk);
		/* mm mtx clk */
		clk_set_parent(pw_info->mtx_clk, pw_info->mtx_clk_default);
		clk_disable_unprepare(pw_info->mtx_clk);
		/* disable */
		clk_disable_unprepare(pw_info->mm_mtx_eb);
		clk_disable_unprepare(pw_info->mm_ahb_eb);
		clk_disable_unprepare(pw_info->mm_eb);
	}
	mutex_unlock(&pw_info->mlock);

	return 0;
}
EXPORT_SYMBOL(sprd_cam_domain_disable);

static int mmpw_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("E\n");
	/* read dts */
	ret = mmsys_power_init(pdev);
	if (ret) {
		pr_err("power init fail\n");
		return -ENODEV;
	}
	pr_info("OK\n");

	return ret;
}

static int mmpw_remove(struct platform_device *pdev)
{

	pr_debug("E\n");

	mmsys_power_deinit(pdev);

	return 0;
}

static const struct of_device_id mmpw_match_table[] = {
	{.compatible = "sprd,mm-domain", },
	{},
};

static struct platform_driver mmpw_driver = {
	.probe = mmpw_probe,
	.remove = mmpw_remove,
	.driver = {
		.name = "mmsys-power",
		.of_match_table = of_match_ptr(mmpw_match_table),
	},
};

/* module_platform_driver(mmpw_driver); */
static int __init mmpw_init(void)
{
	int ret;

	ret = platform_driver_register(&mmpw_driver);

	return ret;
}

static void __exit mmpw_exit(void)
{
	platform_driver_unregister(&mmpw_driver);
}

module_init(mmpw_init)
module_exit(mmpw_exit)


MODULE_DESCRIPTION("MMsys Power Driver");
MODULE_AUTHOR("Multimedia_Camera@unisoc.com");
MODULE_LICENSE("GPL");

