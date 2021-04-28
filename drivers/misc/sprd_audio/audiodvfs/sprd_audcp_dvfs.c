/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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
#define pr_fmt(fmt) "audcp dvfs "fmt

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/sipc.h>

#include "audio-sipc.h"
#include "sprd_audcp_dvfs.h"

#define TO_STRING(e) #e

/* integer part and fraction part for clock */
static u32 dvfs_clock[AUDCP_CLK_DSP_CORE_MAX][2] = {
	[AUDCP_CLK_DSP_CORE_26M][0] = 26,
	[AUDCP_CLK_DSP_CORE_26M][1] = 0,
	[AUDCP_CLK_DSP_CORE_153_6M][0] = 153,
	[AUDCP_CLK_DSP_CORE_153_6M][1] = 6,
	[AUDCP_CLK_DSP_CORE_256M][0] = 256,
	[AUDCP_CLK_DSP_CORE_256M][1] = 0,
	[AUDCP_CLK_DSP_CORE_384M][0] = 384,
	[AUDCP_CLK_DSP_CORE_384M][1] = 0,
	[AUDCP_CLK_DSP_CORE_512M][0] = 512,
	[AUDCP_CLK_DSP_CORE_512M][1] = 0,
	[AUDCP_CLK_DSP_CORE_614M][0] = 614,
	[AUDCP_CLK_DSP_CORE_614M][1] = 0,
};

static u32 dvfs_voltage[AUDCP_VOLTAGE_MAX][2] = {
	[AUDCP_VOLTAGE_0_7V][0] = 0,
	[AUDCP_VOLTAGE_0_7V][1] = 7,
	[AUDCP_VOLTAGE_0_75V][0] = 0,
	[AUDCP_VOLTAGE_0_75V][1] = 75,
	[AUDCP_VOLTAGE_0_8V][0] = 0,
	[AUDCP_VOLTAGE_0_8V][1] = 8,
};

static const char * const running_mode_table[AUDCP_RUNNING_MODE_MAX] = {
	[ADCP_RUNNING_DEEP_SLEEP] = TO_STRING(ADCP_RUNNING_DEEP_SLEEP),
	[AUDCP_RUNNING_LIGHT_SLEEP] = TO_STRING(AUDCP_RUNNING_LIGHT_SLEEP),
	[AUDCP_RUNNING_NO_SLEEP] = TO_STRING(AUDCP_RUNNING_NO_SLEEP),
	[AUDCP_RUNNING_NORMAL] = TO_STRING(AUDCP_RUNNING_NORMAL),
};

static int enable_audcp_ipc(struct device *dev)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	int ret;

	if (!data->sipc_enabled) {
		ret = aud_ipc_ch_open(data->channel);
		if (ret) {
			dev_err(dev, "channel(%u)_open failed\n",
				data->channel);
			return ret;
		}
		data->sipc_enabled = 1;
	}

	if (data->sipc_enabled)
		dev_info(dev, "audcp dvfs ipc enabled, channel = %d\n",
			 data->channel);

	return 0;
}

static int disable_audcp_ipc(struct device *dev)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	int ret;

	if (data->sipc_enabled) {
		ret = aud_ipc_ch_close(data->channel);
		if (ret < 0) {
			dev_err(dev, "channel(%u) close failed\n",
				data->channel);
			return ret;
		}
		data->sipc_enabled = 0;
	}

	return 0;
}

/* attributes functions begin */
static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "audio cp not implement\n");
}

static DEVICE_ATTR_RO(status);

static struct attribute *audcp_sys_attrs[] = {
	&dev_attr_status.attr,
	NULL,
};

/* audio cp dvfs function enable */
static ssize_t enable_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	u32 enable = data->func_enable;

	return sprintf(buf, "%u\n", enable);
}

static ssize_t enable_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int ret;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	u32 enable;

	if (sscanf(buf, "%u\n", &enable) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}
	if (enable) {
		/* enable sipc */
		ret = enable_audcp_ipc(dev);
		if (ret < 0) {
			dev_err(dev, "enable audcp ipc failed\n");
			return ret;
		}
		data->func_enable = 1;
	} else {
		/* disable sipc */
		ret = disable_audcp_ipc(dev);
		if (ret < 0) {
			dev_err(dev, "disable audcp ipc failed\n");
			return ret;
		}
		data->func_enable = 0;
	}

	return count;
}

static DEVICE_ATTR_RW(enable);

static ssize_t hw_dvfs_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_hw_dvfs_enable hw_en = {};
	struct audcp_hw_dvfs_enable hw_en_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_ENABLE, &hw_en,
				  sizeof(struct audcp_hw_dvfs_enable),
				  &hw_en_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%u\n", hw_en_out.enable);
}

static ssize_t hw_dvfs_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int ret;
	u32 enable;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_hw_dvfs_enable hw_en = {};
	struct audcp_hw_dvfs_enable hw_en_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &enable) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}
	if (enable)
		hw_en.enable = 1;
	else
		hw_en.enable = 0;

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_SET_ENABLE, &hw_en,
				  sizeof(struct audcp_hw_dvfs_enable),
				  &hw_en_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR_RW(hw_dvfs_enable);

static ssize_t audcp_running_mode_table_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	u32 i, len = 0;

	for (i = 0; i < AUDCP_RUNNING_MODE_MAX; i++)
		len += sprintf(buf + len, "%u-----%s\n", i,
			       running_mode_table[i]);

	return len;
}

static DEVICE_ATTR_RO(audcp_running_mode_table);

static ssize_t audcp_running_mode_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct running_mode run_mode = {};
	struct running_mode run_mode_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_RUNNING_MODE, &run_mode,
				  sizeof(struct running_mode),
				  &run_mode_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}
	if (run_mode_out.mode > AUDCP_RUNNING_MODE_MAX) {
		dev_err(dev, "audio cp returned invalid running mode\n");
		return -EINVAL;
	}

	return sprintf(buf, "%s\n", running_mode_table[run_mode_out.mode]);
}

static ssize_t audcp_running_mode_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	u32 mode;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct running_mode run_mode = {};
	struct running_mode run_mode_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &mode) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}
	if (mode > AUDCP_RUNNING_MODE_MAX) {
		dev_err(dev, "invalid running mode\n");
		return -EINVAL;
	}
	run_mode.mode = mode;
	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_SET_RUNNING_MODE, &run_mode,
				  sizeof(struct running_mode),
				  &run_mode_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}
	if (run_mode_out.mode > AUDCP_RUNNING_MODE_MAX) {
		dev_err(dev, "audio cp returned invalid running mode\n");
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR_RW(audcp_running_mode);

static ssize_t table_info_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_table table = {};
	struct audcp_dvfs_table table_out = {};
	int ret, i, len = 0;
	u32 index_cnt;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0, AUDDVFS_GET_TABLE,
				  &table, sizeof(struct audcp_dvfs_table),
				  &table_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}
	index_cnt = table_out.total_index_cnt;
	if (index_cnt >= AUDCP_DVFS_TABLE_MAX) {
		dev_err(dev, "invalid index count\n");
		return -EINVAL;
	}
	len += sprintf(buf, "index count=%u\n", index_cnt);
	for (i = 0; i < table_out.total_index_cnt; i++) {
		len += sprintf(buf + len, "index-%d => %u.%uv:%u.%uMhz\n",
			table_out.table[i].index,
			dvfs_voltage[table_out.table[i].voltage][0],
			dvfs_voltage[table_out.table[i].voltage][1],
			dvfs_clock[table_out.table[i].clock][0],
			dvfs_clock[table_out.table[i].clock][1]);
	}

	return len;
}

static DEVICE_ATTR_RO(table_info);

static ssize_t running_record_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_records record = {};
	struct audcp_records record_out = {};
	int ret, i, len = 0;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0, AUDDVFS_GET_RECORD,
				  &record, sizeof(struct audcp_records),
				  &record_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	if (record_out.total_index_cnt >= AUDCP_DVFS_TABLE_MAX)
		return sprintf(buf, "invalid total index count\n");
	len += sprintf(buf + len, "index ---- times\n");
	for (i = 0; i < record_out.total_index_cnt; i++)
		len += sprintf(buf + len, "%u---- %u\n",
		record_out.records[i].index, record_out.records[i].times);
	len += sprintf(buf + len, "index changed history\n");
	for (i = 0; i < AUDRECORDS_MAX_NUM; i++)
		len += sprintf(buf + len, "%u ", record_out.index_history[i]);
	len += sprintf(buf + len, "\n");

	return len;
}

static DEVICE_ATTR_RO(running_record);

static ssize_t voltage_table_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	u32 i, len = 0;

	for (i = 0; i < AUDCP_VOLTAGE_MAX; i++)
		len += sprintf(buf + len, "%u == %u.%u\n", i,
		dvfs_voltage[i][0], dvfs_voltage[i][1]);

	return len;
}

static DEVICE_ATTR_RO(voltage_table);

static ssize_t fixed_voltage_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct dvfs_fixed_voltage fixed_voltage = {};
	struct dvfs_fixed_voltage fixed_voltage_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}
	if (!data->user_data.fix_voltage_enable)
		return sprintf(buf, "please enable fix_voltage_enable first\n");
	if (!data->user_data.fixed_voltage_set)
		return sprintf(buf, "please set fixed valtatge first\n");
	fixed_voltage.enabled = 1;
	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_FIXED_VOLTAGE, &fixed_voltage,
				  sizeof(struct dvfs_fixed_voltage),
				  &fixed_voltage_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	if (fixed_voltage_out.voltage >= AUDCP_VOLTAGE_MAX)
		return sprintf(buf, "invalid voltage index from audio cp %u\n",
			      fixed_voltage_out.voltage);

	return sprintf(buf, "%u.%u\n",
		       dvfs_voltage[fixed_voltage_out.voltage][0],
		       dvfs_voltage[fixed_voltage_out.voltage][1]);
}

static ssize_t fixed_voltage_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	u32 voltage_idx;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct dvfs_fixed_voltage fixed_voltage = {};
	struct dvfs_fixed_voltage fixed_voltage_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (!data->user_data.fix_voltage_enable) {
		dev_err(dev, "please enable fix_voltage_enable first\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%u\n", &voltage_idx) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}
	if (voltage_idx >= AUDCP_VOLTAGE_MAX) {
		dev_err(dev, "invalid voltage index\n");
		return -EINVAL;
	}

	fixed_voltage.enabled = 1;
	fixed_voltage.voltage = voltage_idx;
	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_SET_FIXED_VOLTAGE, &fixed_voltage,
				  sizeof(struct dvfs_fixed_voltage),
				  &fixed_voltage_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	data->user_data.fixed_voltage_set = 1;

	return count;
}

static DEVICE_ATTR_RW(fixed_voltage);

static ssize_t fixed_voltage_enable_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	return sprintf(buf, "%u\n", data->user_data.fix_voltage_enable);
}

static ssize_t fixed_voltage_enable_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int ret;
	u32 enable;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &enable) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}
	data->user_data.fix_voltage_enable = enable;

	return count;
}

static DEVICE_ATTR_RW(fixed_voltage_enable);

static ssize_t auto_dvfs_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_auto auto_en = {};
	struct audcp_dvfs_auto auto_en_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0, AUDDVFS_GET_AUTO,
				  &auto_en, sizeof(struct audcp_dvfs_auto),
				  &auto_en_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}
	return sprintf(buf, "%u\n", auto_en_out.enable);
}

static ssize_t auto_dvfs_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int ret;
	u32 enable;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_auto auto_en = {};
	struct audcp_dvfs_auto auto_en_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &enable) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}

	if (enable)
		auto_en.enable = 1;
	else
		auto_en.enable = 0;

	ret = aud_send_cmd_result(data->channel, 0, 0, AUDDVFS_SET_AUTO,
				  &auto_en, sizeof(struct audcp_dvfs_auto),
				  &auto_en_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR_RW(auto_dvfs);

static ssize_t force_dvfs_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_force_en force_en = {};
	struct audcp_dvfs_force_en force_en_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_DVFS_FORCE_EN, &force_en,
				  sizeof(struct audcp_dvfs_force_en),
				  &force_en_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%u\n", force_en_out.enable);
}

static ssize_t force_dvfs_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u32 enable;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_force_en force_en = {};
	struct audcp_dvfs_force_en force_en_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &enable) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}

	if (enable)
		force_en.enable = 1;
	else
		force_en.enable = 0;

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_SET_DVFS_FORCE_EN, &force_en,
				  sizeof(struct audcp_dvfs_force_en),
				  &force_en_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR_RW(force_dvfs);

static ssize_t hold_fsm_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_hold hold_en = {};
	struct audcp_dvfs_hold hold_en_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_HOLD, &hold_en,
				  sizeof(struct audcp_dvfs_hold),
				  &hold_en_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%u\n", hold_en_out.enable);
}

static ssize_t hold_fsm_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int ret;
	u32 enable;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_hold hold_en = {};
	struct audcp_dvfs_hold hold_en_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &enable) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}

	if (enable)
		hold_en.enable = 1;
	else
		hold_en.enable = 0;

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_SET_HOLD, &hold_en,
				  sizeof(struct audcp_dvfs_hold),
				  &hold_en_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR_RW(hold_fsm);

static ssize_t audcp_sys_busy_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_busy busy = {};
	struct audcp_dvfs_busy busy_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_SYS_DVFS_BUSY, &busy,
				  sizeof(struct audcp_dvfs_busy),
				  &busy_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%u\n", busy_out.enable);
}

static DEVICE_ATTR_RO(audcp_sys_busy);

static ssize_t window_cnt_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_window_cnt win_cnt = {};
	struct audcp_dvfs_window_cnt win_cnt_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_WINDOW_CNT, &win_cnt,
				  sizeof(struct audcp_dvfs_window_cnt),
				  &win_cnt_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%#x\n", win_cnt_out.count);
}

static DEVICE_ATTR_RO(window_cnt);

static ssize_t dvfs_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_status status = {};
	struct audcp_dvfs_status status_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_AUDCP_DVFS_STATUS, &status,
				  sizeof(struct audcp_dvfs_status),
				  &status_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%#x\n", status_out.status);
}

static DEVICE_ATTR_RO(dvfs_status);

static ssize_t current_voltage_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_current_voltage cur_voltage = {};
	struct audcp_dvfs_current_voltage cur_voltage_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_CURRENT_VOLTAGE, &cur_voltage,
				  sizeof(struct audcp_dvfs_current_voltage),
				  &cur_voltage_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}
	if (cur_voltage_out.voltage > AUDCP_VOLTAGE_MAX)
		return sprintf(buf, "invalid voltage index %d\n",
			      cur_voltage_out.voltage);

	return sprintf(buf, "%u.%u\n", dvfs_voltage[cur_voltage_out.voltage][0],
		      dvfs_voltage[cur_voltage_out.voltage][1]);
}

static DEVICE_ATTR_RO(current_voltage);

static ssize_t vote_voltage_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_inter_vote_voltage vote_voltage = {};
	struct audcp_dvfs_inter_vote_voltage vote_voltage_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_INTERNAL_VOTE_VOLTAGE,
				  &vote_voltage,
				  sizeof(struct audcp_dvfs_inter_vote_voltage),
				  &vote_voltage_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}
	if (vote_voltage_out.voltage > AUDCP_VOLTAGE_MAX)
		return sprintf(buf, "invalid  voltage index %d\n",
			      vote_voltage_out.voltage);

	return sprintf(buf, "%u.%u\n",
		       dvfs_voltage[vote_voltage_out.voltage][0],
		       dvfs_voltage[vote_voltage_out.voltage][1]);
}

static DEVICE_ATTR_RO(vote_voltage);

static ssize_t voltage_meet_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_sys_voltage_meet meet = {};
	struct audcp_sys_voltage_meet meet_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_AUDCP_SYS_VOLTAGE_MEET, &meet,
				  sizeof(struct audcp_sys_voltage_meet),
				  &meet_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%#x\n", meet_out.meeted);
}

static DEVICE_ATTR_RO(voltage_meet);

static ssize_t voltage_meet_bypass_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_sys_voltage_meet_byp meet_byp = {};
	struct audcp_sys_voltage_meet_byp meet_byp_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_AUDCP_SYS_VOLTAGE_MEET_BYP,
				  &meet_byp,
				  sizeof(struct audcp_sys_voltage_meet_byp),
				  &meet_byp_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%#x\n", meet_byp.meet_bypass);
}

static ssize_t voltage_meet_bypass_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;
	u32 enable;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_sys_voltage_meet_byp bypass_en = {};
	struct audcp_sys_voltage_meet_byp bypass_en_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &enable) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}

	if (enable)
		bypass_en.meet_bypass = 1;
	else
		bypass_en.meet_bypass = 0;

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_SET_AUDCP_SYS_VOLTAGE_MEET_BYP,
				  &bypass_en,
				  sizeof(struct audcp_sys_voltage_meet_byp),
				  &bypass_en_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR_RW(voltage_meet_bypass);

static ssize_t audcp_idle_voltage_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_sys_idle_voltage voltage = {};
	struct audcp_sys_idle_voltage voltage_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_AUDCP_SYS_IDLE_VOLTAGE, &voltage,
				  sizeof(struct audcp_sys_idle_voltage),
				  &voltage_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%#x\n", voltage_out.voltage);
}

static ssize_t audcp_idle_voltage_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	u32 value;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_sys_idle_voltage voltage = {};
	struct audcp_sys_idle_voltage voltage_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &value) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}

	if (value)
		voltage.voltage = 1;
	else
		voltage.voltage = 0;

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_SET_AUDCP_SYS_IDLE_VOLTAGE, &voltage,
				  sizeof(struct audcp_sys_idle_voltage),
				  &voltage_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR_RW(audcp_idle_voltage);

static ssize_t sw_tune_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct sw_tune_en sw_tune = {};
	struct sw_tune_en sw_tune_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_SW_TUNE_EN, &sw_tune,
				  sizeof(struct sw_tune_en), &sw_tune_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%#x\n", sw_tune_out.enable);
}

static ssize_t sw_tune_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int ret;
	u32 enable;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct sw_tune_en sw_tune = {};
	struct sw_tune_en sw_tune_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &enable) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}

	if (enable)
		sw_tune.enable = 1;
	else
		sw_tune.enable = 0;

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_SET_SW_TUNE_EN, &sw_tune,
				  sizeof(struct sw_tune_en), &sw_tune_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR_RW(sw_tune);

static ssize_t sw_dvfs_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_sw_dvfs_enable sw_dvfs = {};
	struct audcp_sw_dvfs_enable sw_dvfs_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_GET_AUDCP_SW_DVFS_ENABLE, &sw_dvfs,
				  sizeof(struct audcp_sw_dvfs_enable),
				  &sw_dvfs_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%#x\n", sw_dvfs_out.enable);
}

static ssize_t sw_dvfs_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int ret;
	u32 enable;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_sw_dvfs_enable sw_dvfs = {};
	struct audcp_sw_dvfs_enable sw_dvfs_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &enable) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}

	if (enable)
		sw_dvfs.enable = 1;
	else
		sw_dvfs.enable = 0;

	ret = aud_send_cmd_result(data->channel, 0, 0,
				  AUDDVFS_SET_AUDCP_SW_DVFS_ENABLE, &sw_dvfs,
				  sizeof(struct audcp_sw_dvfs_enable),
				  &sw_dvfs_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR_RW(sw_dvfs);

static ssize_t dvfs_register_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_reg reg_val = {};
	struct audcp_reg reg_val_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}
	reg_val.reg = data->user_data.reg;
	ret = aud_send_cmd_result(data->channel, 0, 0, AUDDVFS_GET_REG,
				  &reg_val, sizeof(struct audcp_reg),
				  &reg_val_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "%#x:%#x\n", reg_val_out.reg, reg_val_out.val);
}

static ssize_t dvfs_register_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret, reg, val;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_reg reg_val = {};
	struct audcp_reg reg_val_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u:%u\n", &reg, &val) != 2) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}

	/*
	 * check reg value,
	 * it may be a danger hols, just for debug !!!!!!!!!!
	 */
	dev_warn(dev, "audio cp dvfs it may be a danger hols, just for debug !!!!!");

	ret = aud_send_cmd_result(data->channel, 0, 0, AUDDVFS_SET_REG,
				  &reg_val, sizeof(struct audcp_reg),
				  &reg_val_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return -EINVAL;
	}

	data->user_data.reg = reg_val_out.reg;

	return count;
}

static DEVICE_ATTR_RW(dvfs_register);

/* get current index */
static ssize_t index_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_index index = {};
	struct audcp_dvfs_index index_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}
	/* init for invalid value, update by audio dsp */
	index_out.index = AUDDVFS_INDEX_MAX;
	ret = aud_send_cmd_result(data->channel, 0, 0, AUDDVFS_GET_INDEX,
				  &index, sizeof(struct audcp_dvfs_index),
				  &index_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "index %u\n", index_out.index);
}

static ssize_t index_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int ret;
	u32 val;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_index index = {};
	struct audcp_dvfs_index index_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &val) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}

	if (val >= AUDCP_DVFS_TABLE_MAX) {
		dev_err(dev, "invalid index\n");
		return -EINVAL;
	}
	index.index = val;
	ret = aud_send_cmd_result(data->channel, 0, 0, AUDDVFS_SET_INDEX,
				  &index, sizeof(struct audcp_dvfs_index),
				  &index_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR_RW(index);

static ssize_t idle_index_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_index index = {};
	struct audcp_dvfs_index index_out = {};
	int ret;

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	/* init for invalid value, update by audio dsp */
	index_out.index = AUDDVFS_INDEX_MAX;
	ret = aud_send_cmd_result(data->channel, 0, 0, AUDDVFS_GET_IDLE_INDEX,
				  &index, sizeof(struct audcp_dvfs_index),
				  &index_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return sprintf(buf, "index %u\n", index_out.index);
}

static ssize_t idle_index_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u32 val;
	struct audcpdvfs_data *data = dev_get_drvdata(dev);
	struct audcp_dvfs_index index = {};
	struct audcp_dvfs_index index_out = {};

	ret = enable_audcp_ipc(dev);
	if (ret < 0) {
		dev_err(dev, "enable audcp ipc failed\n");
		return ret;
	}

	if (sscanf(buf, "%u\n", &val) != 1) {
		dev_err(dev, "failed get val\n");
		return -EINVAL;
	}

	if (val >= AUDCP_DVFS_TABLE_MAX) {
		dev_err(dev, "invalid index\n");
		return -EINVAL;
	}
	index.index = val;
	ret = aud_send_cmd_result(data->channel, 0, 0, AUDDVFS_SET_IDLE_INDEX,
				  &index, sizeof(struct audcp_dvfs_index),
				  &index_out, -1);
	if (ret != 0) {
		dev_err(dev, "aud_send_cmd failed\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR_RW(idle_index);

static struct attribute *audcp_governor_attrs[] = {
	&dev_attr_audcp_running_mode_table.attr,
	&dev_attr_audcp_running_mode.attr,
	&dev_attr_hw_dvfs_enable.attr,
	&dev_attr_enable.attr,
	&dev_attr_table_info.attr,
	&dev_attr_index.attr,
	&dev_attr_idle_index.attr,
	&dev_attr_auto_dvfs.attr,
	&dev_attr_force_dvfs.attr,
	&dev_attr_hold_fsm.attr,
	&dev_attr_audcp_sys_busy.attr,
	&dev_attr_window_cnt.attr,
	&dev_attr_dvfs_status.attr,
	&dev_attr_current_voltage.attr,
	&dev_attr_vote_voltage.attr,
	&dev_attr_voltage_table.attr,
	&dev_attr_fixed_voltage_enable.attr,
	&dev_attr_fixed_voltage.attr,
	&dev_attr_voltage_meet.attr,
	&dev_attr_voltage_meet_bypass.attr,
	&dev_attr_audcp_idle_voltage.attr,
	&dev_attr_sw_tune.attr,
	&dev_attr_sw_dvfs.attr,
	&dev_attr_running_record.attr,
	&dev_attr_dvfs_register.attr,
	NULL,
};

static const struct attribute_group audcp_sys_group = {
	.attrs = audcp_sys_attrs,
};

static const struct attribute_group audcp_governor_group = {
	.attrs = audcp_governor_attrs,
	.name = "audcp-governor",
};

static const struct attribute_group *audcp_groups[] = {
	&audcp_sys_group,
	&audcp_governor_group,
	NULL,
};

static int audcp_dvfs_parse_dts(struct device *dev,
				struct audcpdvfs_data *pdata)
{
	struct device_node *np = dev->of_node;
	int err;

	err = of_property_read_string(dev->of_node, "compatible", &pdata->name);
	if (err) {
		dev_err(dev, "fail to get name\n");
		return err;
	}

	err = of_property_read_u32(np, "sprd,channel", &pdata->channel);
	if (err)
		dev_err(dev, "sprd,channel,core failed\n");

	return err;
}

static int audcp_dvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct audcpdvfs_data *data;
	int err;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	err = audcp_dvfs_parse_dts(dev, data);
	if (err) {
		dev_err(dev, "failed parse dts\n");
		return err;
	}
	platform_set_drvdata(pdev, data);
	err =
		sysfs_create_groups(&dev->kobj, audcp_groups);
	if (err) {
		dev_err(dev, "audio failed to create audcpdvfs_sys device attributes\n");
		return err;
	}
	data->dev_sys = dev;

	return err;
}

static int audcp_dvfs_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sysfs_remove_groups(&dev->kobj, audcp_groups);

	return 0;
}

static const struct of_device_id audcp_dvfs_match[] = {
	{ .compatible = "sprd,sharkl5-audcp-dvfs" },
	{},
};

MODULE_DEVICE_TABLE(of, audcp_dvfs_match);

static struct platform_driver audcp_dvfs_driver = {
	.probe = audcp_dvfs_probe,
	.remove = audcp_dvfs_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_audcp_dvfs",
		.of_match_table = audcp_dvfs_match,
	},
};

module_platform_driver(audcp_dvfs_driver);

MODULE_AUTHOR("Lei Ning <lei.ning@unisoc.com>");
MODULE_DESCRIPTION("dvfs driver for audio cp");
MODULE_LICENSE("GPL v2");
