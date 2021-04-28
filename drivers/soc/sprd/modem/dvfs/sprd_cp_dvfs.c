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

#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include "sprd_cp_dvfs.h"

static bool sbuf_created;
static int cp_dvfs_send_data(struct cpdvfs_data *cpdvfs, u8 *buf, u32 len)
{
	int nwrite, dbg_i;
	int sent_len = 0, timeout = 100, retry = 0;

	for (dbg_i = 0; dbg_i < len; dbg_i++)
		dev_dbg(cpdvfs->dev, "buf[%d]=0x%02x\n", dbg_i, buf[dbg_i]);
	do {
		nwrite =
			sbuf_write(SIPC_ID_LTE, SMSG_CH_DVFS, CP_DVFS_SBUFID,
					(void *)(buf + sent_len), len - sent_len,
						msecs_to_jiffies(timeout));
		if (nwrite > 0)
			sent_len += nwrite;
		if (nwrite < len || nwrite < 0)
			dev_err(cpdvfs->dev, "nwrite=%d,len=%d,sent_len=%d,timeout=%dms\n",
					nwrite, len, sent_len, timeout);

		/* only handle boot exception */
		if (nwrite < 0) {
			if (nwrite == -ENODEV) {
				msleep(SBUF_TRY_WAIT_MS);
				if (++retry > SBUF_TRY_WAIT_TIEMS)
					break;
			} else {
				dev_dbg(cpdvfs->dev, "nwrite=%d\n", nwrite);
				dev_dbg(cpdvfs->dev, "task #: %s, pid = %d, tgid = %d\n",
						current->comm, current->pid, current->tgid);
				WARN_ONCE(1, "%s timeout: %dms\n",
						__func__, timeout);
				break;
			}
		}
	} while (sent_len  < len);

	return nwrite;
}

/**
 * This function send sent_cmd data to CP
 * this interface is conciser for highest layer SYSFS
 * @return 0 : success
 */
static int cp_dvfs_send_cmd(struct cpdvfs_data *cpdvfs)
{
	int nwrite;

	dev_dbg(cpdvfs->dev, "cmd_len=%d\n", cpdvfs->cmd_len);
	if (cpdvfs->cmd_len > 0) {
		dev_dbg(cpdvfs->dev, "core_id=%d, cmd=%d\n", cpdvfs->sent_cmd->core_id, cpdvfs->sent_cmd->cmd);
		nwrite = cp_dvfs_send_data(cpdvfs, (u8 *)cpdvfs->sent_cmd, cpdvfs->cmd_len);
		if (nwrite > 0) {
			return 0;
		} else {
			dev_err(cpdvfs->dev, "send failed,nwrite=%d\n", nwrite);
			return nwrite;
		}
	} else {
		dev_err(cpdvfs->dev, "no data need to send\n");
		return -EINVAL;
	}
}

static int cp_dvfs_send_cmd_nopara(
		struct cpdvfs_data *cpdvfs,
		enum dvfs_cmd_type cmd)
{
	struct cmd_pkt *sent_cmd = cpdvfs->sent_cmd;

	/* build this cmd */
	sent_cmd->core_id = cpdvfs->core_id;
	sent_cmd->cmd = cmd;
	cpdvfs->cmd_len = sizeof(struct cmd_pkt);

	/* send this cmd */
	return cp_dvfs_send_cmd(cpdvfs);
}

static int cp_dvfs_send_cmd_onepara(struct cpdvfs_data *cpdvfs, enum dvfs_cmd_type cmd, u8 para)
{
	cpdvfs->sent_cmd->core_id = cpdvfs->core_id;
	cpdvfs->sent_cmd->cmd = cmd;
	cpdvfs->sent_cmd->para[0] = para;
	/* include cmd field size */
	cpdvfs->cmd_len = 3;

	return cp_dvfs_send_cmd(cpdvfs);
}

static int cp_dvfs_send_setreg(struct cpdvfs_data *cpdvfs,
		struct reg_t *reg)
{
	cpdvfs->sent_cmd->core_id = cpdvfs->core_id;
	cpdvfs->sent_cmd->cmd = DVFS_SET_REG;

	memcpy(cpdvfs->sent_cmd->para, reg, sizeof(struct reg_t));
	/* include cmd field size */
	cpdvfs->cmd_len = sizeof(struct cmd_pkt) + sizeof(struct reg_t);

	dev_dbg(cpdvfs->dev, "para[0]=0x%02x\n", cpdvfs->sent_cmd->para[0]);
	dev_dbg(cpdvfs->dev, "para[1]=0x%02x\n", cpdvfs->sent_cmd->para[1]);
	dev_dbg(cpdvfs->dev, "para[2]=0x%02x\n", cpdvfs->sent_cmd->para[2]);
	dev_dbg(cpdvfs->dev, "para[3]=0x%02x\n", cpdvfs->sent_cmd->para[3]);
	dev_dbg(cpdvfs->dev, "para[4]=0x%02x\n", cpdvfs->sent_cmd->para[4]);
	dev_dbg(cpdvfs->dev, "para[5]=0x%02x\n", cpdvfs->sent_cmd->para[5]);
	dev_dbg(cpdvfs->dev, "para[6]=0x%02x\n", cpdvfs->sent_cmd->para[6]);
	dev_dbg(cpdvfs->dev, "para[7]=0x%02x\n", cpdvfs->sent_cmd->para[7]);

	return cp_dvfs_send_cmd(cpdvfs);
}

static int cp_dvfs_send_inqreg(struct cpdvfs_data *cpdvfs, u32 *reg_addr)
{
	cpdvfs->sent_cmd->core_id = cpdvfs->core_id;
	cpdvfs->sent_cmd->cmd = DVFS_GET_REG;
	memcpy(cpdvfs->sent_cmd->para, reg_addr, sizeof(*reg_addr));
	/* include cmd field size */
	cpdvfs->cmd_len = sizeof(struct cmd_pkt) + sizeof(*reg_addr);

	dev_dbg(cpdvfs->dev, "para[0]=0x%02x\n", cpdvfs->sent_cmd->para[0]);
	dev_dbg(cpdvfs->dev, "para[1]=0x%02x\n", cpdvfs->sent_cmd->para[1]);
	dev_dbg(cpdvfs->dev, "para[2]=0x%02x\n", cpdvfs->sent_cmd->para[2]);
	dev_dbg(cpdvfs->dev, "para[3]=0x%02x\n", cpdvfs->sent_cmd->para[3]);

	return cp_dvfs_send_cmd(cpdvfs);
}

static int cp_dvfs_recv(struct cpdvfs_data *cpdvfs, int timeout)
{
	int nread, retry = 0;

	do {
		nread =
			sbuf_read(SIPC_ID_LTE, SMSG_CH_DVFS, CP_DVFS_SBUFID,
					(void *)cpdvfs->rd_buf,
					CP_DVFS_RXBUFSIZE, msecs_to_jiffies(timeout));
		if (nread < 0) {
			msleep(SBUF_TRY_WAIT_MS);
			if (++retry > SBUF_TRY_WAIT_TIEMS)
				break;
			dev_info(cpdvfs->dev, "nread=%d,retry=%d\n", nread, retry);
		}
	} while (nread < 0);

	return nread;
}

static int cp_dvfs_recv_cmd_onepara(
		struct cpdvfs_data *cpdvfs, enum dvfs_cmd_type recv_cmd)
{
	u8 core_id_ack, cmd_ack;
	int read_len, para0 = -EINVAL;

	read_len = cp_dvfs_recv(cpdvfs, SBUF_RD_TIMEOUT_MS);
	if (read_len > 0) {
		core_id_ack = cpdvfs->rd_buf[0];
		cmd_ack = cpdvfs->rd_buf[1];

		dev_dbg(cpdvfs->dev, "sent core_id =%d,  cmd=%d\n", cpdvfs->sent_cmd->core_id, cpdvfs->sent_cmd->cmd);
		dev_dbg(cpdvfs->dev, "recv_cmd=%d\n", recv_cmd);
		dev_dbg(cpdvfs->dev, "core_id_ack=%d,  cmd_ack=%d\n", core_id_ack, cmd_ack);
		/* recevied data is response of corresponding cmd, parse it */
		if ((core_id_ack == cpdvfs->core_id) && (cmd_ack == recv_cmd)) {
			para0 =  cpdvfs->rd_buf[2];
			dev_dbg(cpdvfs->dev, "get resp cmd para0=%d\n", para0);
		} else {
			dev_err(cpdvfs->dev, "this recevied is not response cmd\n");
		}
	} else {
		dev_err(cpdvfs->dev, "read_len=%d\n", read_len);
	}

	return  para0;
}

static int cp_dvfs_recv_reg_val(struct cpdvfs_data *cpdvfs)
{
	int read_len;
	struct cmd_pkt *recv_pkt;

	read_len = cp_dvfs_recv(cpdvfs, SBUF_RD_TIMEOUT_MS);
	if (read_len > 0) {
		recv_pkt = (struct cmd_pkt *)cpdvfs->rd_buf;
		dev_dbg(cpdvfs->dev, "sent core_id=%d,cmd=%d\n", cpdvfs->sent_cmd->core_id, cpdvfs->sent_cmd->cmd);
		dev_dbg(cpdvfs->dev, "core_id_ack=%d,cmd_ack=%d\n", recv_pkt->core_id, recv_pkt->cmd);
		/* recevied data is response of the corresponding cmd, parse it */
		if ((recv_pkt->core_id == cpdvfs->core_id) && (recv_pkt->cmd == DVFS_GET_REG)) {
			memcpy(&cpdvfs->user_data->inq_reg,  recv_pkt->para, sizeof(struct reg_t));
			dev_dbg(cpdvfs->dev, "get reg_addr=0x%08x\n", cpdvfs->user_data->inq_reg.reg_addr);
			dev_dbg(cpdvfs->dev, "get reg_value=0x%08x\n", cpdvfs->user_data->inq_reg.reg_val);
			return  cpdvfs->user_data->inq_reg.reg_val;
		}
	}
	return -EIO;
}

static void cp_dvfs_recv_recorder(struct cpdvfs_data *cpdvfs)
{
	int read_len, expect_pkt_len;
	struct cmd_pkt *recv_pkt;

	expect_pkt_len = sizeof(struct cmd_pkt) + cpdvfs->record_num * sizeof(struct cp_dvfs_record);
	read_len = cp_dvfs_recv(cpdvfs, SBUF_RD_TIMEOUT_MS);
	recv_pkt = (struct cmd_pkt *)cpdvfs->rd_buf;

	dev_dbg(cpdvfs->dev, "dev_rcdnum=%d\n", cpdvfs->record_num);
	dev_dbg(cpdvfs->dev, "read_len=%d,  expect_pkt_len=%d\n", read_len, expect_pkt_len);
	dev_dbg(cpdvfs->dev, "sent core_id =%d,  cmd=%d\n", cpdvfs->sent_cmd->core_id, cpdvfs->sent_cmd->cmd);
	dev_dbg(cpdvfs->dev, "core_id_ack=%d,  cmd_ack=%d\n", recv_pkt->core_id, recv_pkt->cmd);
	if (read_len ==  expect_pkt_len) {
		/* recevied data is response of corresponding cmd, parse it */
		if (recv_pkt->core_id == cpdvfs->core_id && recv_pkt->cmd == DVFS_GET_RECORD)
			memcpy(cpdvfs->user_data->records,  recv_pkt->para,
					cpdvfs->record_num * sizeof(struct cp_dvfs_record));
	}
}

/* attributes functions begin */
static ssize_t name_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	struct cpdvfs_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", data->name);
}
static DEVICE_ATTR_RO(name);

static ssize_t core_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cpdvfs_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->core_id);
}
static DEVICE_ATTR_RO(core_id);

static ssize_t dev_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	return
		sprintf(buf, "name:%s\ncore_id:%d\nrecord_num:%d\n",
				cpdvfs->name, cpdvfs->core_id, cpdvfs->record_num);
}
static DEVICE_ATTR_RO(dev_info);

static ssize_t index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int index = -1, ret;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	mutex_lock(&cpdvfs->lock);
	ret =  cp_dvfs_send_cmd_nopara(cpdvfs, DVFS_GET_INDEX);
	if (ret == 0) {
		index = cp_dvfs_recv_cmd_onepara(cpdvfs, DVFS_GET_INDEX);
		cpdvfs->user_data->index =  index;
	} else {
		dev_err(dev, "send command fail\n");
	}
	mutex_unlock(&cpdvfs->lock);

	return sprintf(buf, "%d\n", index);
}

static ssize_t index_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 index;
	int ret;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	dev_dbg(dev, "buf=%s\n", buf);
	if (sscanf(buf, "%hhu\n", &index) != 1)
		return -EINVAL;
	if (index > 7) {
		dev_err(dev, "input is %d, pls input right index [0,7]\n", index);
		return -EINVAL;
	}
	mutex_lock(&cpdvfs->lock);
	ret = cp_dvfs_send_cmd_onepara(cpdvfs, DVFS_SET_INDEX, index);
	if (ret != 0)
		dev_err(dev, "send command fail\n");
	mutex_unlock(&cpdvfs->lock);

	return count;
}
static DEVICE_ATTR_RW(index);

static ssize_t idle_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int idle_index = -1, ret;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	mutex_lock(&cpdvfs->lock);
	ret = cp_dvfs_send_cmd_nopara(cpdvfs, DVFS_GET_IDLE_INDEX);
	if (ret == 0) {
		idle_index = cp_dvfs_recv_cmd_onepara(cpdvfs, DVFS_GET_IDLE_INDEX);
		cpdvfs->user_data->idle_index =  idle_index;
	} else {
		dev_err(dev, "send command fail\n");
	}
	mutex_unlock(&cpdvfs->lock);

	return sprintf(buf, "%d\n", idle_index);
}

static ssize_t idle_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 idle_index;
	int ret;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	dev_dbg(dev, "buf=%s\n", buf);
	if (sscanf(buf, "%hhu\n", &idle_index) != 1)
		return -EINVAL;
	if (idle_index > 7) {
		dev_err(dev, "input is %d, pls input right idx [0,7]\n",
				idle_index);
		return -EINVAL;
	}
	mutex_lock(&cpdvfs->lock);
	ret = cp_dvfs_send_cmd_onepara(cpdvfs, DVFS_SET_IDLE_INDEX, idle_index);

	if (ret != 0)
		dev_err(dev, "send command fail\n");
	mutex_unlock(&cpdvfs->lock);

	return count;
}
static DEVICE_ATTR_RW(idle_index);

static ssize_t auto_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int auto_en = -1, ret;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	mutex_lock(&cpdvfs->lock);
	ret = cp_dvfs_send_cmd_nopara(cpdvfs, DVFS_GET_AUTO);
	if (ret == 0) {
		auto_en = cp_dvfs_recv_cmd_onepara(cpdvfs, DVFS_GET_AUTO);
		cpdvfs->user_data->auto_enable =  auto_en;
	} else {
		dev_err(dev, "send command fail\n");
	}
	mutex_unlock(&cpdvfs->lock);

	return sprintf(buf, "%d\n", auto_en);
}

static ssize_t auto_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u8 auto_enable;
	int ret;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	dev_dbg(dev, "buf=%s\n", buf);
	if (sscanf(buf, "%hhu\n", &auto_enable) != 1)
		return -EINVAL;
	mutex_lock(&cpdvfs->lock);
	if (auto_enable == 0 || auto_enable == 1) {
		ret = cp_dvfs_send_cmd_onepara(cpdvfs, DVFS_SET_AUTO, auto_enable);
		if (ret)
			dev_err(dev, "send command fail\n");
	} else
		dev_err(dev, "input is %d, pls input right val:[0,1]\n",
				auto_enable);
	mutex_unlock(&cpdvfs->lock);

	return count;
}
static DEVICE_ATTR_RW(auto_status);

static ssize_t set_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u32 ret;
	struct reg_t reg;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	dev_dbg(dev, "buf=%s\n", buf);
	if (sscanf(buf, "%x %x", &reg.reg_addr, &reg.reg_val) != 2)
		return -EINVAL;
	dev_dbg(dev, "reg_addr=0x%x reg_val=0x%x\n", reg.reg_addr, reg.reg_val);
	mutex_lock(&cpdvfs->lock);
	ret = cp_dvfs_send_setreg(cpdvfs, &reg);
	if (ret != 0)
		dev_err(dev, "send command fail\n");
	memcpy(&cpdvfs->user_data->set_reg, &reg, sizeof(struct reg_t));
	mutex_unlock(&cpdvfs->lock);

	return count;
}

static ssize_t set_reg_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	return sprintf(buf, "addr:0x%08x, value:0x%08x\n",
			cpdvfs->user_data->set_reg.reg_addr,
			cpdvfs->user_data->set_reg.reg_val);
}
static DEVICE_ATTR_RW(set_reg);

static ssize_t inq_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	return sprintf(buf, "addr:0x%08x, value:0x%08x\n",
			cpdvfs->user_data->inq_reg.reg_addr,
			cpdvfs->user_data->inq_reg.reg_val);
}

static ssize_t inq_reg_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	u32 reg_addr, reg_val, ret;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	dev_dbg(dev, "buf=%s\n", buf);
	if (sscanf(buf, "%x\n", &reg_addr) != 1)
		return -EINVAL;
	mutex_lock(&cpdvfs->lock);
	ret = cp_dvfs_send_inqreg(cpdvfs, &reg_addr);
	if (ret != 0)
		dev_err(dev, "send command fail\n");
	reg_val = cp_dvfs_recv_reg_val(cpdvfs);
	dev_dbg(dev, "reg_addr=0x%08x, reg_val=0x%08x\n", reg_addr, reg_val);
	mutex_unlock(&cpdvfs->lock);

	return count;
}
static DEVICE_ATTR_RW(inq_reg);

static ssize_t running_recorder_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct cp_dvfs_record *cur_record;
	int ret, record_idx, len = 0;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	mutex_lock(&cpdvfs->lock);
	/* this cmd don't own parameters  */
	ret = cp_dvfs_send_cmd_nopara(cpdvfs, DVFS_GET_RECORD);
	len += sprintf(buf + len, "---time_32k  index----\n");
	if (ret == 0) {
		cp_dvfs_recv_recorder(cpdvfs);
		cur_record = cpdvfs->user_data->records;
		for (record_idx = 0; record_idx < cpdvfs->record_num; record_idx++) {
			len += sprintf(buf + len, "0x%x, %d\n",
					cur_record->time_32k,
					cur_record->index);
			cur_record++;
		}
	} else {
		dev_err(dev, "send command fail\n");
	}
	mutex_unlock(&cpdvfs->lock);

	return len;
}
static DEVICE_ATTR_RO(running_recorder);

static struct attribute *cpdvfs_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_core_id.attr,
	&dev_attr_dev_info.attr,
	&dev_attr_index.attr,
	&dev_attr_idle_index.attr,
	&dev_attr_auto_status.attr,
	&dev_attr_set_reg.attr,
	&dev_attr_inq_reg.attr,
	&dev_attr_running_recorder.attr,
	NULL,
};
ATTRIBUTE_GROUPS(cpdvfs);

static int
cp_dvfs_get_of_pdata(struct device *dev, struct cpdvfs_data *pdata)
{
	struct device_node *np = dev->of_node;
	int err;

	err = of_property_read_u32(np, "sprd,core_id", &pdata->core_id);
	if (err) {
		dev_err(dev, "fail to get core id\n");
		return err;
	}

	err = of_property_read_string(dev->of_node, "compatible", &pdata->name);
	if (err) {
		dev_err(dev, "fail to get name\n");
		return err;
	}

	err = of_property_read_u32(np, "sprd,record_num", &pdata->record_num);
	if (err) {
		dev_err(dev, "fail to get core id\n");
		return err;
	}

	if (pdata->record_num > RECORDS_MAX_NUM)
		return -EINVAL;

	return 0;
}

static int cp_dvfs_init(struct cpdvfs_data *cpdvfs)
{
	int ret = 0;
	struct userspace_data *usr_data;
	struct cmd_pkt *cmd;

	usr_data = devm_kzalloc(cpdvfs->dev, sizeof(*usr_data), GFP_KERNEL);
	if (!usr_data)
		return -ENOMEM;
	cpdvfs->user_data = usr_data;

	cmd = devm_kzalloc(cpdvfs->dev, sizeof(*cmd) + CMD_PARA_MAX_LEN, GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;
	cpdvfs->sent_cmd = cmd;

	if (!sbuf_created) {
		ret = sbuf_create(SIPC_ID_LTE, SMSG_CH_DVFS, CP_DVFS_SBUF_NUM, CP_DVFS_TXBUFSIZE, CP_DVFS_RXBUFSIZE);
		if (ret < 0) {
			dev_err(cpdvfs->dev, "create sbuf fail!\n");
			return ret;
		}
		sbuf_created = true;
	}

	return ret;
}

static int cp_dvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cpdvfs_data *data;
	int err;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->dev = dev;
	err = cp_dvfs_get_of_pdata(dev, data);
	if (err)
		return err;

	platform_set_drvdata(pdev, data);
	err = cp_dvfs_init(data);
	if (err)
		return err;
	mutex_init(&data->lock);

	err =
		sysfs_create_groups(&dev->kobj, cpdvfs_groups);
	if (err) {
		dev_err(dev, "failed to create sysfs device attributes\n");
		return err;
	}

	return 0;
}

static int cp_dvfs_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sbuf_created = false;
	sysfs_remove_groups(&dev->kobj, cpdvfs_groups);
	return 0;
}

static const struct of_device_id cp_dvfs_match[] = {
	{ .compatible = "sprd,sharkl5-pubcp-dvfs" },
	{ .compatible = "sprd,sharkl5-wtlcp-dvfs" },
	{ .compatible = "sprd,roc1-pubcp-dvfs" },
	{ .compatible = "sprd,roc1-wtlcp-dvfs" },
	{ .compatible = "sprd,sharkl5Pro-pubcp-dvfs" },
	{ .compatible = "sprd,sharkl5Pro-wtlcp-dvfs" },
	{},
};

MODULE_DEVICE_TABLE(of, cp_dvfs_match);

static struct platform_driver cp_dvfs_driver = {
	.probe = cp_dvfs_probe,
	.remove = cp_dvfs_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_cp_dvfs",
		.of_match_table = of_match_ptr(cp_dvfs_match),
	},
};

module_platform_driver(cp_dvfs_driver);

MODULE_AUTHOR("Bao Yue <bao.yue@unisoc.com>");
MODULE_DESCRIPTION("cp dvfs driver for pubcp and wtlcp");
MODULE_LICENSE("GPL");
