#ifndef SPRD_AUDCPDVFS_H
#define SPRD_AUDCPDVFS_H

/*****************communicate data********************/
#define AUDRECORDS_MAX_NUM 20

enum dvfs_index {
	AUDDVFS_INDEX_MODE_LEAVE = -1,
	AUDDVFS_INDEX_0 = 0,
	AUDDVFS_INDEX_1,
	AUDDVFS_INDEX_2,
	AUDDVFS_INDEX_3,
	AUDDVFS_INDEX_4,
	AUDDVFS_INDEX_5,
	AUDDVFS_INDEX_6,
	AUDDVFS_INDEX_7,
	AUDDVFS_INDEX_MAX
};

/* AUDDVFS_GET_ENABLE/AUDDVFS_SET_ENABLE */
struct audcp_hw_dvfs_enable {
	u32 enable;
	u32 result;
};

enum {
	AUDCP_CLK_DSP_CORE_26M,
	AUDCP_CLK_DSP_CORE_153_6M,
	AUDCP_CLK_DSP_CORE_256M,
	AUDCP_CLK_DSP_CORE_384M,
	AUDCP_CLK_DSP_CORE_512M,
	AUDCP_CLK_DSP_CORE_614M,
	AUDCP_CLK_DSP_CORE_MAX
};

enum {
	AUDCP_VOLTAGE_0_7V,
	AUDCP_VOLTAGE_0_75V,
	AUDCP_VOLTAGE_0_8V,
	AUDCP_VOLTAGE_MAX
};

/* AUDDVFS_GET_TABLE */
#define AUDCP_DVFS_TABLE_MAX 8
struct audcp_dvfs_index_map {
	u32 index;
	u32 voltage;
	u32 clock;
};

struct audcp_dvfs_table {
	u32 total_index_cnt;
	struct audcp_dvfs_index_map table[AUDCP_DVFS_TABLE_MAX];
};

/*
 * AUDDVFS_GET_INDEX/AUDDVFS_SET_INDEX
 * AUDDVFS_GET_IDLE_INDEX/AUDDVFS_SET_IDLE_INDEX
 */
struct audcp_dvfs_index {
	u32 index;
	u32 result;
};

/* AUDDVFS_GET_AUTO/AUDDVFS_SET_AUTO */
struct audcp_dvfs_auto {
	u32 enable;
	u32 result;
};

/* AUDDVFS_GET_DVFS_FORCE_EN/AUDDVFS_SET_DVFS_FORCE_EN */
/* auto gating mode enable of dvfs clock ? */
struct audcp_dvfs_force_en {
	u32 enable;
};

/* AUDDVFS_GET_HOLD/AUDDVFS_SET_HOLD */
struct audcp_dvfs_hold {
	u32 enable;
};

/* AUDDVFS_GET_SYS_DVFS_BUSY */
struct audcp_dvfs_busy {
	u32 enable;
};

/* AUDDVFS_GET_WINDOW_CNT */
struct audcp_dvfs_window_cnt {
	u32 count;
};

/* AUDDVFS_GET_AUDCP_DVFS_STATUS */
/* to do, spec no define status enum, just return a value */
struct audcp_dvfs_status {
	u32 status;
};

/* AUDDVFS_GET_CURRENT_VOLTAGE */
struct audcp_dvfs_current_voltage {
	u32 voltage;
};

/* AUDDVFS_GET_INTERNAL_VOTE_VOLTAGE */
struct audcp_dvfs_inter_vote_voltage {
	u32 voltage;
};

/* AUDDVFS_GET_FIXED_VOLTAGE / AUDDVFS_SET_FIXED_VOLTAGE */
struct dvfs_fixed_voltage {
	u32 enabled;
	u32 voltage;
	u32 result;
};

/* with dcdc mm */
/* AUDDVFS_GET_AUDCP_SYS_VOLTAGE_MEET */
struct audcp_sys_voltage_meet {
	u32 meeted;
};

/*
 * AUDDVFS_GET_AUDCP_SYS_VOLTAGE_MEET_BYP/
 * AUDDVFS_SET_AUDCP_SYS_VOLTAGE_MEET_BYP
 */
struct audcp_sys_voltage_meet_byp {
	u32 meet_bypass;
};

/* idle voltage of dcdc modem if audcp sys shutdown  */
/* AUDDVFS_GET_AUDCP_SYS_IDLE_VOLTAGE/AUDDVFS_SET_AUDCP_SYS_IDLE_VOLTAGE */
struct audcp_sys_idle_voltage {
	u32 voltage;
};

/* AUDDVFS_GET_SW_TUNE_EN/AUDDVFS_SET_SW_TUNE_EN */
struct sw_tune_en {
	u32 enable;
};

/* AUDDVFS_GET_AUDCP_SW_DVFS_ENABLE/ AUDDVFS_SET_AUDCP_SW_DVFS_ENABLE */
struct audcp_sw_dvfs_enable {
	u32 enable;
};

/* AUDDVFS_GET_RECORD */
struct audcp_record {
	u32 times;
	u32 index;
};

struct audcp_records {
	u32 total_index_cnt;
	/* record how many times for each index */
	struct audcp_record records[AUDCP_DVFS_TABLE_MAX];
	/*
	 * a ringbuf, dsp shuold maintain a same index_history array to
	 * record the index when inddex changed.
	 */
	u32 index_history[AUDRECORDS_MAX_NUM];
};

/* AUDDVFS_GET_REG /AUDDVFS_SET_REG */
struct audcp_reg {
	u32 reg;
	u32 val;
};

enum {
	/* only deep */
	ADCP_RUNNING_DEEP_SLEEP,
	/* only light */
	AUDCP_RUNNING_LIGHT_SLEEP,
	/* no sleep */
	AUDCP_RUNNING_NO_SLEEP,
	/* deep+light */
	AUDCP_RUNNING_NORMAL,
	AUDCP_RUNNING_MODE_MAX
};

/* AUDDVFS_SET_TEST_MODE */
struct running_mode {
	u32 mode;
};

enum dvfs_cmd_type {
	AUDDVFS_GET_RUNNING_MODE,
	AUDDVFS_SET_RUNNING_MODE,
	/* hw dvfs enable */
	AUDDVFS_GET_ENABLE,
	AUDDVFS_SET_ENABLE,
	AUDDVFS_GET_TABLE,
	AUDDVFS_GET_INDEX,
	AUDDVFS_SET_INDEX,
	AUDDVFS_GET_IDLE_INDEX,
	AUDDVFS_SET_IDLE_INDEX,
	/* auto gating mode enable of dvfs clock ? */
	AUDDVFS_GET_AUTO,
	AUDDVFS_SET_AUTO,
	AUDDVFS_GET_DVFS_FORCE_EN,
	AUDDVFS_SET_DVFS_FORCE_EN,
	AUDDVFS_GET_HOLD,
	AUDDVFS_SET_HOLD,
	AUDDVFS_GET_SYS_DVFS_BUSY,
	AUDDVFS_GET_WINDOW_CNT,
	/* to do, spec no define status enum, just return a value */
	AUDDVFS_GET_AUDCP_DVFS_STATUS,
	AUDDVFS_GET_CURRENT_VOLTAGE,
	AUDDVFS_GET_INTERNAL_VOTE_VOLTAGE,
	AUDDVFS_GET_FIXED_VOLTAGE,
	AUDDVFS_SET_FIXED_VOLTAGE,
	/* with dcdc mm */
	AUDDVFS_GET_AUDCP_SYS_VOLTAGE_MEET,
	AUDDVFS_GET_AUDCP_SYS_VOLTAGE_MEET_BYP,
	AUDDVFS_SET_AUDCP_SYS_VOLTAGE_MEET_BYP,
	/* idle voltage of dcdc modem if audcp sys shutdown  */
	AUDDVFS_GET_AUDCP_SYS_IDLE_VOLTAGE,
	AUDDVFS_SET_AUDCP_SYS_IDLE_VOLTAGE,
	AUDDVFS_GET_SW_TUNE_EN,
	AUDDVFS_SET_SW_TUNE_EN,
	AUDDVFS_GET_AUDCP_SW_DVFS_ENABLE,
	AUDDVFS_SET_AUDCP_SW_DVFS_ENABLE,
	AUDDVFS_GET_RECORD,
	AUDDVFS_GET_REG,
	AUDDVFS_SET_REG,
	AUDDVFS_CMD_MAX
};

/*******************ap data**************************/
struct auduserspace_data {
	u32 reg;
	u32 fix_voltage_enable;
	u32 fixed_voltage_set;
};

struct audcpdvfs_data {
	struct device *dev_sys;
	const char *name;
	struct kobject *obj_governor;
	u32 record_num;
	struct auduserspace_data user_data;
	u32 channel;
	u32 sipc_enabled;
	u32 func_enable;
};
#endif
