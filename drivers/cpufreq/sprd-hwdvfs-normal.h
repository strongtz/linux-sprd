/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 * updated at 2018-03-22 11:21:44
 *
 */
#ifndef SPRD_HWDVFS_NORMAL_H
#define SPRD_HWDVFS_NORMAL_H

#include "sprd-hwdvfs-archdata.h"

enum cpu_dvfs_cluster {
	DVFS_CLUSTER_LIT_CORE,	/*including ANANKE CORES*/
	DVFS_CLUSTER_BIG_CORE,	/*including PROMETHEUS CORE*/
	DVFS_CLUSTER_SCU,	/*including SCU & ACE*/
	DVFS_CLUSTER_PERIPH,	/*including PERIPH */
	DVFS_CLUSTER_GIC,	/*including GIC*/
	DVFS_CLUSTER_ATB,	/*including ATB & DEBUG APB*/
	DVFS_CLUSTER_MAX,
};

struct plat_opp {
	unsigned long freq;	/*hz*/
	unsigned long volt;	/*uV*/
};

struct cpudvfs_phy_ops {
	int (*dvfs_module_eb)(void *data);
	int (*mpll_relock_enable)(void *data, u32 num, bool enable);
	int (*mpll_pd_enable)(void *data, u32 num, bool enable);
	int (*auto_tuning_enable)(void *data,
				  u32 cluster_id, bool enable);
	int (*hw_dvfs_map_table_init)(void *data);
	int (*set_dvfs_work_index)(void *data, u32 cluster_id, u32 opp_idx);
	int (*set_dvfs_idle_index)(void *data, u32 cluster_id, u32 opp_idx);
	int (*get_dvfs_index)(void *data, u32 cluster_id, bool is_work);
	int (*get_cgm_sel_value)(void *data, u32 cluster_id, u32 device_id);
	int (*get_cgm_div_value)(void *data, u32 cluster_id, u32 device_id);
	int (*get_cgm_voted_volt)(void *data, u32 cluster_id, u32 device_id);
	int (*dfs_idle_disable)(void *data, u32 cluster_id, u32 device_id);
	int (*get_index_entry_info)(void *data, int index,
				    u32 cluster_id, u32 **pdvfs_tbl_entry);
	int (*get_index_freq)(void *data, u32 cluster_id, int index);
	int (*coordinate_dcdc_current_voltage)(void *data, u32 dcdc_num);
	int (*dcdc_vol_grade_value_setup)(void *data, u32 dcdc_num);
	int (*get_sys_dcdc_dvfs_state)(void *data, u32 dcdc_num);
	int (*get_top_dcdc_dvfs_state)(void *data, u32 dcdc_num);
	int (*setup_i2c_channel)(void *data, u32 dcdc_num);
	int (*dcdc_vol_delay_time_setup)(void *data, u32 dcdc_num);
	int (*set_dcdc_idle_voltage)(void *data, u32 dcdc_num, u32 grade);
	int (*mpll_index_table_init)(void *data, u32 mpll_num);
	int (*hw_dvfs_misc_config)(void *data);
};

struct device_name {
	char name[8];
};

struct common_clk_volt {
	u32 sel;
	u32 div;
	u32 voted_volt;
};

struct host_clk_volt {
	struct common_clk_volt comm_entry;
	u32 voted_scu_idx;
	u32 voted_peri_idx;
	u32 voted_gic_idx;
};

struct lit_core_tbl_entry {
	struct host_clk_volt comm_host_entry;
	u32 voted_mpll_idx;
};

struct big_core_tbl_entry {
	struct host_clk_volt comm_host_entry;
	u32 voted_mpll_idx;
	u32 voted_lit_core_cluster_volt;
};

struct scu_tbl_entry {
	struct common_clk_volt comm_entry;
	u32 div_ace;
	u32 voted_mpll_idx;
};

struct periph_tbl_entry {
	struct common_clk_volt comm_entry;
};

struct gic_tbl_entry {
	struct common_clk_volt comm_entry;
};

struct atb_tbl_entry {
	struct common_clk_volt comm_entry;
};

struct dvfs_cluster_driver {
	int (*parse)(void *cluster);
	int (*map_tbl_init)(void *cluster);
	int (*set_index)(void *cluster, u32 index, bool work);
	int (*get_index)(void *cluster, bool work);
	int (*get_cgm_sel)(void *cluster, u32 device_id);
	int (*get_cgm_div)(void *cluster, u32 device_id);
	int (*get_voted_volt)(void *cluster, u32 device_id);
	unsigned long (*get_freq)(void *cluster, int index);
	int (*get_entry_info)(void *cluster, u32 index,
			      u32 **entry);
	int (*set_dfs_idle_disable)(void *cluster, u32 device_id);
};

struct sub_device {
	const char *name;
	u32 device_id;
	struct device_node *of_node;
	int curr_index;
	u32 sel_reg;
	u32 sel_bit;
	u32 sel_mask;
	u32 div_reg;
	u32 div_bit;
	u32 div_mask;
	u32 vol_reg;
	u32 vol_bit;
	u32 vol_mask;
	int idle_dis_reg;
	int idle_dis_off;
	int idle_dis_en;
};

struct mpll_cfg {
	struct regmap *anag_map;
	u32 anag_reg;
	u32 POST_DIV;
	u32 ICP;
	u32 N;
	u32 relock_reg;
	u32 relock_bit;
	u32 relock_eb;
	u32 pd_reg;
	u32 pd_bit;
	u32 pd_eb;
};

struct voltage_info {
	u32 grade_nr;
	u32 vol_value;
	u32 vol_reg;
	u32 vol_bit;
	u32 vol_mask;
};

struct voltage_delay_cfg {
	u32 voltage_span;
	u32 reg;
	u32 reg_offset;
	u32 reg_mask;
	u32 reg_value;
};

struct dcdc_pwr {
	char name[10];
	struct regmap *blk_sd_map;
	u32 blk_reg;	/* dvfs block shutdown */
	u32 blk_off;
	u32 dvfs_ctl_reg;
	u32 dvfs_ctl_bit;
	u32 dvfs_eb;
	bool dialog_used;
	u32 pmic_num;		/* sequence number in pmic_array*/
	u32 supply_sel_reg;
	u32 supply_sel_bit;
	u32 supply_sel_dialog;
	u32 tuning_latency_us;
	u32 subsys_tune_ctl_reg;
	u32 subsys_tune_ctl_bit;
	u32 subsys_tune_eb;	/*just for host cluster*/
	u32 judge_vol_sw_reg;
	u32 judge_vol_sw_bit;
	u32 judge_vol_sw_mask;
	u32 judge_vol_val;	/*real voltage needed to tell dvfs module*/
	u32 voltage_grade_num;
	u32 slew_rate;		/* mv/us */
	unsigned long grade_volt_val_array[MAX_VOLT_GRADE_NUM];	/* in uV*/
	struct voltage_info *vol_info;
	struct voltage_delay_cfg *up_delay_array;
	struct voltage_delay_cfg *down_delay_array;
	u32 up_delay_array_size;
	u32 down_delay_array_size;
	u32 subsys_dcdc_vol_sw_reg;
	u32 subsys_dcdc_vol_sw_bit;
	u32 subsys_dcdc_vol_sw_mask;
	u32 subsys_dcdc_vol_sw_vol_val;
	bool i2c_used;
	u32 top_dvfs_state_reg;
	u32 top_dvfs_state_bit;
	u32 top_dvfs_state_mask;
	u32 subsys_dvfs_state_reg;
	u32 subsys_dvfs_state_bit;
	u32 subsys_dvfs_state_mask;
	bool fix_dcdc_pd_volt;
	u32 idle_vol_reg;
	u32 idle_vol_off;
	u32 idle_vol_msk;
	u32 idle_vol_val;
	struct i2c_client *i2c_client;
};

struct dvfs_cluster {
	u32 id;			/*cluster id*/
	char *name;		/*cluster name*/
	bool is_host_cluster;
	enum dcdc_name dcdc;
	unsigned long pre_grade_volt;
	u32 tmp_vol_grade;
	u32 max_vol_grade;
	void *parent_dev;	/*parent device*/
	enum cpu_dvfs_cluster enum_name;
	struct dvfs_cluster_driver *driver;
	struct plat_opp *freqvolt; /*store the opp for the cluster*/
	char *tbl_column_num_property;
	char *tbl_row_num_property;
	struct device_node *of_node;
	char dts_tbl_name[30];	/*map table name in dts for this cluster*/
	char default_tbl_name[30];
	u32 tbl_column_num;	/*the column num in map table*/
	u32 tbl_row_num;	/*the row num in map table*/
	u32 device_num;		/*the device num in this cluster*/
	u32 map_idx_max;
	u32 *opp_map_tbl;
	bool auto_tuning;
	u32 needed_judge;
	bool existed;
	struct sub_device *subdevs;
	u32 work_index_reg, work_index_mask;
	u32 idle_index_reg, idle_index_mask;
	u32 tuning_fun_reg, tuning_fun_bit; /*just for slave cluster*/
	u32 *map_tbl_regs;
	u32 *column_entry_bit;
	u32 *column_entry_mask;
	int (*auto_tuning_enable)(void *cluster, bool en);/*hw dvfs en*/
};

struct cpudvfs_archdata {
	struct regmap *aon_apb_reg_base;
	void __iomem *membase;
	const struct dvfs_private_data *priv;
	struct device_node *topdvfs_of_node;
	struct regmap *topdvfs_map;
	struct device_node *of_node;
	struct dvfs_cluster *phost_cluster;
	struct dvfs_cluster *pslave_cluster;
	struct dvfs_cluster *cluster_array[16];
	struct cpudvfs_phy_ops *phy_ops;
	u32 host_cluster_num, slave_cluster_num;
	u32 total_cluster_num;
	u32 total_device_num;
	struct mpll_cfg *mplls;
	u32 mpll_num;
	struct dcdc_pwr *pwr;
	u32 dcdc_num;
	u32 pmic_type_sum; /* the total number of different pmics*/
	bool parse_done;
	bool module_eb;
	u32 module_eb_reg;
	u32 module_eb_bit;
	bool i2c_used[DVFS_CLUSTER_MAX];
	bool probed;
	bool enabled;
};

extern int cpudvfs_sysfs_create(struct cpudvfs_archdata *pdev);

extern const struct dvfs_private_data ums312_dvfs_private_data;

#endif /* DVFS_CTRL_H */
