/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#ifndef __SPRD_DVFS_VDSP_H__
#define __SPRD_DVFS_VDSP_H__

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>
#include "sprd_dvfs_apsys.h"

typedef enum {
	VOLT70 = 0, //0.7v
	VOLT75, //0.75v
	VOLT80, //0.80v
} voltage_level;

typedef enum {
	EDAP_DIV_0 = 0,
	EDAP_DIV_1,
	EDAP_DIV_2,
	EDAP_DIV_3,
} edap_div_level;

typedef enum {
	M0_DIV_0 = 0,
	M0_DIV_1,
	M0_DIV_2,
	M0_DIV_3,
} m0_div_level;

typedef enum {
	VDSP_CLK_INDEX_192M = 0,
	VDSP_CLK_INDEX_307M2,
	VDSP_CLK_INDEX_468M,
	VDSP_CLK_INDEX_614M4,
	VDSP_CLK_INDEX_702M,
	VDSP_CLK_INDEX_768M,
} clock_level;

typedef enum {
	VDSP_CLK192M = 192000000,
	VDSP_CLK307M2 = 307200000,
	VDSP_CLK468M = 468000000,
	VDSP_CLK614M4 = 614400000,
	VDSP_CLK702M = 702000000,
	VDSP_CLK768M = 768000000,
} clock_rate;

struct vdsp_dvfs_map_cfg {
	u32 map_index;
	u32 volt_level;
	u32 clk_level;
	u32 clk_rate;
	u32 edap_div;
	u32 m0_div;
};

struct vdsp_dvfs {
	int dvfs_enable;
	struct device dev;

	struct devfreq *devfreq;
	struct opp_table *opp_table;
	struct devfreq_event_dev *edev;
	struct notifier_block vdsp_dvfs_nb;

	u32 work_freq;
	u32 idle_freq;
	set_freq_type freq_type;

	struct ip_dvfs_coffe dvfs_coffe;
	struct ip_dvfs_status dvfs_status;
	struct vdsp_dvfs_ops *dvfs_ops;
};

struct vdsp_dvfs_ops {
	/* initialization interface */
	int (*parse_dt)(struct vdsp_dvfs *vdsp, struct device_node *np);
	int (*parse_pll)(struct vdsp_dvfs *vdsp, struct device *dev);
	int (*dvfs_init)(struct vdsp_dvfs *vdsp);
	void (*hw_dfs_en)(bool dfs_en);

	/* work-idle dvfs index ops */
	void  (*set_work_index)(int index);
	int  (*get_work_index)(void);
	void  (*set_idle_index)(int index);
	int  (*get_idle_index)(void);

	/* work-idle dvfs freq ops */
	void (*set_work_freq)(u32 freq);
	u32 (*get_work_freq)(void);
	void (*set_idle_freq)(u32 freq);
	u32 (*get_idle_freq)(void);

	/* work-idle dvfs map ops */
	int  (*get_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*set_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*get_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*set_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*get_dvfs_status)(struct ip_dvfs_status *dvfs_status);

	/* coffe setting ops */
	void (*set_gfree_wait_delay)(u32 para);
	void (*set_freq_upd_en_byp)(bool enable);
	void (*set_freq_upd_delay_en)(bool enable);
	void (*set_freq_upd_hdsk_en)(bool enable);
	void (*set_dvfs_swtrig_en)(bool enable);
};

extern struct list_head vdsp_dvfs_head;
extern struct blocking_notifier_head vdsp_dvfs_chain;

#if IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
int vdsp_dvfs_notifier_call_chain(void *data);
#else
static inline int vdsp_dvfs_notifier_call_chain(void *data)
{
	return 0;
}
#endif

#define vdsp_dvfs_ops_register(entry) \
	dvfs_ops_register(entry, &vdsp_dvfs_head)

#define vdsp_dvfs_ops_attach(str) \
	dvfs_ops_attach(str, &vdsp_dvfs_head)

#endif /* __SPRD_DVFS_VDSP_H__ */
