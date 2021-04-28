/*
 * Copyright (C) 2015--2016 Spreadtrum Communications Inc.
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

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <dt-bindings/soc/sprd,sharkl3-regs.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <uapi/video/sprd_vsp_pw_domain.h>
#include "vsp_common.h"

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "sprd-vsp-pw: " fmt
#if IS_ENABLED(CONFIG_SPRD_VSP_CALL_CAM_PW_DOMAIN)
#include <linux/platform_device.h>
#include <uapi/video/sprd_mmsys_pw_domain.h>
#endif

struct vsp_pw_domain_info_t *vsp_pw_domain_info;

#if IS_ENABLED(CONFIG_SPRD_VSP_PW_DOMAIN)
static int is_vsp_domain_power_on(void)
{
	int power_count = 0;
	int ins_count = 0;

	for (ins_count = 0; ins_count < VSP_PW_DOMAIN_COUNT_MAX; ins_count++) {
		power_count +=
		    vsp_pw_domain_info->pw_vsp_info[ins_count].pw_count;
	}

	return (power_count > 0) ? 1 : 0;
}

#define __SPRD_VSP_TIMEOUT            (30 * 1000)

int vsp_pw_on(u8 client)
{
	int ret = 0;
	u32 power_state1, power_state2, power_state3;
	unsigned long timeout = jiffies + msecs_to_jiffies(__SPRD_VSP_TIMEOUT);
	u32 read_count = 0;

	pr_info("%s Enter client %d\n", __func__, client);
	if (client >= VSP_PW_DOMAIN_COUNT_MAX) {
		pr_err("%s with error client\n", __func__);
		return -1;
	}

	mutex_lock(&vsp_pw_domain_info->client_lock);

	if (regs[PMU_VSP_AUTO_SHUTDOWN].gpr == NULL) {
		pr_info("skip power on\n");
		ret = -1;
		goto pw_on_exit;
	}
	if (is_vsp_domain_power_on() == 0) {

		ret = regmap_update_bits(regs[PMU_VSP_AUTO_SHUTDOWN].gpr,
				regs[PMU_VSP_AUTO_SHUTDOWN].reg,
				regs[PMU_VSP_AUTO_SHUTDOWN].mask,
				(unsigned int)
				(~regs[PMU_VSP_AUTO_SHUTDOWN].mask));

		if (ret) {
			pr_err("regmap_update_bits failed %s, %d\n",
				__func__, __LINE__);
			goto pw_on_exit;
		}

		ret = regmap_update_bits(regs[PMU_VSP_FORCE_SHUTDOWN].gpr,
			regs[PMU_VSP_FORCE_SHUTDOWN].reg,
			regs[PMU_VSP_FORCE_SHUTDOWN].mask,
			(unsigned int)(~regs[PMU_VSP_FORCE_SHUTDOWN].mask));

		if (ret) {
			pr_err("regmap_update_bits failed %s, %d\n",
				__func__, __LINE__);
			goto pw_on_exit;
		}

		do {
			cpu_relax();
			udelay(300);
			read_count++;

			regmap_read(regs[PMU_PWR_STATUS].gpr,
						regs[PMU_PWR_STATUS].reg,
						&power_state1);
			power_state1 &= regs[PMU_PWR_STATUS].mask;
			regmap_read(regs[PMU_PWR_STATUS].gpr,
						regs[PMU_PWR_STATUS].reg,
						&power_state2);
			power_state2 &= regs[PMU_PWR_STATUS].mask;
			regmap_read(regs[PMU_PWR_STATUS].gpr,
						regs[PMU_PWR_STATUS].reg,
						&power_state3);
			power_state3 &= regs[PMU_PWR_STATUS].mask;

			WARN_ON(time_after(jiffies, timeout));
		} while ((power_state1 && read_count < 100)
			 || (power_state1 != power_state2)
			 || (power_state2 != power_state3));

		if (power_state1) {
			pr_err("%s set failed 0x%x\n", __func__,
			       power_state1);
			mutex_unlock(&vsp_pw_domain_info->client_lock);
			return -1;
		}
		pr_info("%s set OK\n", __func__);
	} else {
		pr_info("vsp_domain is already power on\n");
	}

	vsp_pw_domain_info->pw_vsp_info[client].pw_state = VSP_PW_DOMAIN_ON;
	vsp_pw_domain_info->pw_vsp_info[client].pw_count++;

pw_on_exit:
	mutex_unlock(&vsp_pw_domain_info->client_lock);
	return ret;
}
EXPORT_SYMBOL(vsp_pw_on);

int vsp_pw_off(u8 client)
{
	int ret = 0;

	pr_info("%s Enter client %d\n", __func__, client);
	if (client >= VSP_PW_DOMAIN_COUNT_MAX) {
		pr_err("%s with error client\n", __func__);
		return -1;
	}
	mutex_lock(&vsp_pw_domain_info->client_lock);

	if (regs[PMU_VSP_AUTO_SHUTDOWN].gpr == NULL) {
		pr_info("skip power off\n");
		ret = -1;
		goto pw_off_exit;
	}

	if (vsp_pw_domain_info->pw_vsp_info[client].pw_count >= 1) {

		vsp_pw_domain_info->pw_vsp_info[client].pw_count--;
		if (vsp_pw_domain_info->pw_vsp_info[client].pw_count == 0) {
			vsp_pw_domain_info->pw_vsp_info[client].pw_state =
			    VSP_PW_DOMAIN_OFF;
		}

		if (is_vsp_domain_power_on() == 0) {

			ret = regmap_update_bits(
				regs[PMU_VSP_AUTO_SHUTDOWN].gpr,
				regs[PMU_VSP_AUTO_SHUTDOWN].reg,
				regs[PMU_VSP_AUTO_SHUTDOWN].mask,
				(unsigned int)
				(~regs[PMU_VSP_AUTO_SHUTDOWN].mask));

			if (ret) {
				pr_err("regmap_update_bits failed %s, %d\n",
					__func__, __LINE__);
				goto pw_off_exit;
			}

			ret = regmap_update_bits(
				regs[PMU_VSP_FORCE_SHUTDOWN].gpr,
				regs[PMU_VSP_FORCE_SHUTDOWN].reg,
				regs[PMU_VSP_FORCE_SHUTDOWN].mask,
				regs[PMU_VSP_FORCE_SHUTDOWN].mask);

			if (ret) {
				pr_err("regmap_update_bits failed %s, %d\n",
					__func__, __LINE__);
				goto pw_off_exit;
			}

			pr_info("%s set OK\n", __func__);
		}
	} else {
		vsp_pw_domain_info->pw_vsp_info[client].pw_count = 0;
		vsp_pw_domain_info->pw_vsp_info[client].pw_state =
		    VSP_PW_DOMAIN_OFF;
		pr_info("vsp_domain is already power off\n");
	}

pw_off_exit:

	mutex_unlock(&vsp_pw_domain_info->client_lock);
	return ret;
}
EXPORT_SYMBOL(vsp_pw_off);

#elif IS_ENABLED(CONFIG_SPRD_VSP_CALL_CAM_PW_DOMAIN)
int vsp_pw_on(u8 client)
{
	return sprd_cam_pw_on();
}
EXPORT_SYMBOL(vsp_pw_on);

int vsp_pw_off(u8 client)
{
	return sprd_cam_pw_off();
}
EXPORT_SYMBOL(vsp_pw_off);

#else
int vsp_pw_on(u8 client)
{
	return 0;
}
EXPORT_SYMBOL(vsp_pw_on);

int vsp_pw_off(u8 client)
{;
	return 0;
}
EXPORT_SYMBOL(vsp_pw_off);
#endif
static int __init vsp_pw_domain_init(void)
{
	int i = 0;

	pr_info("%s\n", __func__);
	vsp_pw_domain_info =
	    kmalloc(sizeof(struct vsp_pw_domain_info_t), GFP_KERNEL);

	for (i = 0; i < VSP_PW_DOMAIN_COUNT_MAX; i++) {
		vsp_pw_domain_info->pw_vsp_info[i].pw_state = VSP_PW_DOMAIN_OFF;
		vsp_pw_domain_info->pw_vsp_info[i].pw_count = 0;
	}
	mutex_init(&vsp_pw_domain_info->client_lock);

	return 0;
}

fs_initcall(vsp_pw_domain_init);
