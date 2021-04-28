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

#ifndef __SPRD_DVFS_APSYS_H__
#define __SPRD_DVFS_APSYS_H__

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>

typedef enum {
	DVFS_WORK = 0,
	DVFS_IDLE,
} set_freq_type;

struct dvfs_ops_entry {
	const char *ver;
	void *ops;
};

struct dvfs_ops_list {
	struct list_head head;
	struct dvfs_ops_entry *entry;
};

struct apsys_regmap {
	unsigned long apsys_base;
	unsigned long top_base;
};

struct apsys_dvfs_coffe {
	u32 sw_dvfs_en;
	u32 dvfs_hold_en;
	u32 dvfs_wait_window;
	u32 dvfs_min_volt;
	u32 dvfs_force_en;
	u32 dvfs_auto_gate;
	u32 sw_cgb_enable;
};

struct ip_dvfs_coffe {
	u32 gfree_wait_delay;
	u32 freq_upd_hdsk_en;
	u32 freq_upd_delay_en;
	u32 freq_upd_en_byp;
	u32 sw_trig_en;
	u32 hw_dfs_en;
	u32 work_index_def;
	u32 idle_index_def;
};

struct ip_dvfs_map_cfg {
	u32 map_index;
	u32 volt_level;
	u32 clk_level;
	u32 clk_rate;
	char *volt_val;
};

struct ip_dvfs_status {
	char *apsys_cur_volt;
	char *dpu_vote_volt;
	char *vsp_vote_volt;
	char *vdsp_vote_volt;
	char *dpu_cur_freq;
	char *vsp_cur_freq;
	char *vdsp_cur_freq;
	u32 vdsp_edap_div;
	u32 vdsp_m0_div;
};

struct apsys_dev {
	struct device dev;
	unsigned long base;
	const char *version;

	struct apsys_dvfs_coffe dvfs_coffe;
	struct apsys_dvfs_ops *dvfs_ops;
};

struct apsys_dvfs_ops {
	/* apsys common ops */
	int (*parse_dt)(struct apsys_dev *apsys, struct device_node *np);
	void (*dvfs_init)(struct apsys_dev *apsys);
	void (*apsys_hold_en)(u32 hold_en);
	void (*apsys_force_en)(u32 force_en);
	void (*apsys_auto_gate)(u32 gate_sel);
	void (*apsys_wait_window)(u32 wait_window);
	void (*apsys_min_volt)(u32 min_volt);

	/* top common ops */
	void (*top_dvfs_init)(void);
	int (*top_cur_volt)(void);
};

void *dvfs_ops_attach(const char *str, struct list_head *head);
int dvfs_ops_register(struct dvfs_ops_entry *entry, struct list_head *head);

extern struct list_head apsys_dvfs_head;
extern struct apsys_regmap regmap_ctx;
extern struct mutex apsys_glb_reg_lock;

#define apsys_dvfs_ops_register(entry) \
	dvfs_ops_register(entry, &apsys_dvfs_head)

#define apsys_dvfs_ops_attach(str) \
	dvfs_ops_attach(str, &apsys_dvfs_head)

#endif /* __SPRD_DVFS_APSYS_H__ */
