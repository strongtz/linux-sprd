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

#include <linux/fs.h>
#include <linux/devfreq.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/of_device.h>
#include <linux/suspend.h>
#include <linux/sipc.h>
#include <linux/list.h>
#include <linux/sprd_dfs_drv.h>

#define CREATE_TRACE_POINTS
#include "sprd_dfs_trace.h"

enum dfs_master_cmd {
	DFS_CMD_NORMAL		= 0x0000,
	DFS_CMD_ENABLE		= 0x0300,
	DFS_CMD_DISABLE		= 0x0305,
	DFS_CMD_AUTO_ENABLE	= 0x0310,
	DFS_CMD_AUTO_DISABLE	= 0x0315,
	DFS_CMD_AXI_ENABLE      = 0x0320,
	DFS_CMD_AXI_DISABLE     = 0x0330,
	DFS_CMD_INQ_DDR_FREQ	= 0x0500,
	DFS_CMD_INQ_AP_FREQ	= 0x0502,
	DFS_CMD_INQ_CP_FREQ	= 0x0503,
	DFS_CMD_INQ_DDR_TABLE	= 0x0505,
	DFS_CMD_INQ_COUNT	= 0x0507,
	DFS_CMD_INQ_STATUS	= 0x050A,
	DFS_CMD_INQ_AUTO_STATUS	= 0x050B,
	DFS_CMD_INQ_OVERFLOW	= 0x0510,
	DFS_CMD_INQ_UNDERFLOW	= 0x0520,
	DFS_CMD_INQ_TIMER	= 0x0530,
	DFS_CMD_INQ_AXI         = 0x0540,
	DFS_CMD_INQ_AXI_WLTC    = 0x0541,
	DFS_CMD_INQ_AXI_RLTC    = 0x0542,
	DFS_CMD_SET_DDR_FREQ	= 0x0600,
	DFS_CMD_SET_CAL_FREQ	= 0x0603,
	DFS_CMD_PARA_START	= 0x0700,
	DFS_CMD_PARA_OVERFLOW	= 0x0710,
	DFS_CMD_PARA_UNDERFLOW	= 0x0720,
	DFS_CMD_PARA_TIMER	= 0x0730,
	DFS_CMD_PARA_END	= 0x07FF,
	DFS_CMD_SET_AXI_WLTC    = 0x0810,
	DFS_CMD_SET_AXI_RLTC    = 0x0820,
	DFS_CMD_DEBUG		= 0x0FFF
};

enum dfs_slave_cmd {
	DFS_RET_ADJ_OK		= 0x0000,
	DFS_RET_ADJ_VER_FAIL	= 0x0001,
	DFS_RET_ADJ_BUSY	= 0x0002,
	DFS_RET_ADJ_NOCHANGE	= 0x0003,
	DFS_RET_ADJ_FAIL	= 0x0004,
	DFS_RET_DISABLE		= 0x0005,
	DFS_RET_ON_OFF_SUCCEED	= 0x0300,
	DFS_RET_ON_OFF_FAIL	= 0x0303,
	DFS_RET_INQ_SUCCEED	= 0x0500,
	DFS_RET_INQ_FAIL	= 0x0503,
	DFS_RET_SET_SUCCEED	= 0x0600,
	DFS_RET_SET_FAIL	= 0x0603,
	DFS_RET_PARA_OK		= 0x070F,
	DFS_RET_DEBUG_OK	= 0x0F00,
	DFS_RET_INVALID_CMD	= 0x0F0F
};

enum vote_mode {
	MODE_FREQ,
	MODE_BW,
	MODE_MAX
};

enum vote_master {
	DFS_MASTER_NULL,
	DFS_MASTER_DPU,
	DFS_MASTER_DCAM,
	DFS_MASTER_PUBCP,
	DFS_MASTER_WTLCP,
	DFS_MASTER_WTLCP1,
	DFS_MASTER_AGCP,
	DFS_MASTER_SW,
	DFS_MASTER_MAX
};

static unsigned int last_scene_freq[MODE_MAX][DFS_MASTER_MAX];
#define last_vote(magic) (last_scene_freq[magic>>8 & 0xf][magic & 0xff])


struct dfs_data {
	struct device *dev;
	struct devfreq *devfreq;
	struct devfreq_dev_profile *profile;
	struct task_struct *dfs_smsg_ch_open;
	spinlock_t lock;
	struct mutex sync_mutex;
	unsigned int freq_num;
	unsigned int *freq_table;
	unsigned int *overflow;
	unsigned int *underflow;
	int scene_num;
	struct scene_freq *scenes;
	unsigned int force_freq;
	unsigned int backdoor_freq;
	unsigned int init_done;
};

static struct dfs_data *g_dfs_data;

static int dfs_msg_recv(struct smsg *msg, int timeout)
{
	int err = 0;

	err = smsg_recv(SIPC_ID_PM_SYS, msg, timeout);
	if (err < 0) {
		pr_err("%s, dfs receive failed:%d\n", __func__, err);
		return err;
	}
	if (msg->channel == SMSG_CH_PM_CTRL &&
			msg->type == SMSG_TYPE_DFS_RSP)
		return 0;
	return -EINVAL;
}

static int dfs_msg_send(struct smsg *msg, unsigned int cmd, int timeout,
		 unsigned int value)
{
	int err = 0;

	smsg_set(msg, SMSG_CH_PM_CTRL, SMSG_TYPE_DFS, cmd, value);
	err = smsg_send(SIPC_ID_PM_SYS, msg, timeout);
	pr_debug("%s send: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
	__func__, msg->channel, msg->type, msg->flag, msg->value);
	if (err < 0) {
		pr_err("%s, dfs send failed, freq:%d, cmd:%d\n",
		       __func__, value, cmd);
		return err;
	}
	return 0;
}


static int dfs_msg_parse_ret(struct smsg *msg)
{
	unsigned int err;

	switch (msg->flag) {
	case DFS_RET_ADJ_OK:
	case DFS_RET_ON_OFF_SUCCEED:
	case DFS_RET_INQ_SUCCEED:
	case DFS_RET_SET_SUCCEED:
	case DFS_RET_PARA_OK:
	case DFS_RET_DEBUG_OK:
		err = 0;
		break;
	case DFS_RET_ADJ_VER_FAIL:
		pr_info("%s, dfs verify fail!current freq:%d\n", __func__,
						       msg->value);
		err = -EIO;
		break;
	case DFS_RET_ADJ_BUSY:
		pr_info("%s, dfs busy!current freq:%d\n", __func__,
							msg->value);
		err = -EBUSY;
		break;
	case DFS_RET_ADJ_NOCHANGE:
		pr_info("%s, dfs no change!current freq:%d\n", __func__,
						       msg->value);
		err = -EFAULT;
		break;
	case DFS_RET_ADJ_FAIL:
		pr_info("%s, dfs fail!current freq:%d\n", __func__,
							msg->value);
		err = -EFAULT;
		break;
	case DFS_RET_DISABLE:
		pr_info("%s, dfs is disabled!current freq:%d\n", __func__,
						       msg->value);
		err = -EPERM;
		break;
	case DFS_RET_ON_OFF_FAIL:
		pr_err("%s, dfs enable verify failed", __func__);
		err = -EINVAL;
		break;
	case DFS_RET_INQ_FAIL:
		pr_err("%s, dfs inquire failed", __func__);
		err = -EINVAL;
		break;
	case DFS_RET_SET_FAIL:
		pr_err("%s, dfs set failed", __func__);
		err = -EINVAL;
		break;
	case DFS_RET_INVALID_CMD:
		pr_err("%s, no this command", __func__);
		err = -EINVAL;
		break;
	default:
		pr_info("%s, dfs invalid cmd:%x!current freq:%d\n", __func__,
		       msg->flag, msg->value);
		err = -EINVAL;
		break;
	}

	return err;
}

static int dfs_msg(unsigned int *data, unsigned int value,
			unsigned int cmd, unsigned int wait)
{
	int err = 0;
	struct smsg msg;

	err = dfs_msg_send(&msg, cmd, msecs_to_jiffies(100), value);
	if (err < 0) {
		pr_err("%s, dfs msg send failed: %x\n", __func__, cmd);
		return err;
	}

	err = dfs_msg_recv(&msg, msecs_to_jiffies(wait));
	if (err < 0) {
		pr_err("%s, dfs msg receive failed: %x\n", __func__, cmd);
		return err;
	}

	err = dfs_msg_parse_ret(&msg);
	*data = msg.value;
	return err;
}

static int dfs_vote(unsigned int freq, unsigned int magic)
{
	unsigned int data;
#ifdef CONFIG_DEVFREQ_GOV_SPRD_EXT_VOTE
	if (magic == 0)
		return dfs_msg(&data, freq, DFS_CMD_NORMAL, 500);
	else
		return dfs_ext_vote(freq, magic);
#else
	return dfs_msg(&data, (freq | (magic << 16)), DFS_CMD_NORMAL, 500);
#endif
}


int dfs_enable(void)
{
	int err = 0;
	unsigned int data;

	err = dfs_msg(&data, 0, DFS_CMD_ENABLE, 2000);

	return err;
}

int dfs_disable(void)
{
	int err = 0;
	unsigned int data;

	err = dfs_msg(&data, 0, DFS_CMD_DISABLE, 2000);

	return err;
}

int dfs_auto_enable(void)
{
	int err = 0;
	unsigned int data;

	err = dfs_msg(&data, 0, DFS_CMD_AUTO_ENABLE, 2000);

	return err;
}

int dfs_auto_disable(void)
{
	int err = 0;
	unsigned int data;

	err = dfs_msg(&data, 0, DFS_CMD_AUTO_DISABLE, 2000);

	return err;
}

static struct scene_freq *find_scene(char *scenario)
{
	struct scene_freq *scene;
	int i, err;

	if ((g_dfs_data == NULL) || (g_dfs_data->scenes == NULL))
		return NULL;
	scene = g_dfs_data->scenes;
	for (i = 0; i < g_dfs_data->scene_num; i++) {
		err = strcmp(scenario, scene[i].scene_name);
		if (err == 0)
			return &scene[i];
	}
	return NULL;
}

static void add_scene(struct scene_freq *scene)
{
	spin_lock(&g_dfs_data->lock);
	scene->scene_count++;
	spin_unlock(&g_dfs_data->lock);
}

static void del_scene(struct scene_freq *scene)
{
	spin_lock(&g_dfs_data->lock);
	if (scene->scene_count > 0)
		scene->scene_count--;
	spin_unlock(&g_dfs_data->lock);
}

static int send_scene_request(unsigned int magic)
{
	unsigned int target_freq = 0;
	struct scene_freq *scene;
	int i, err;

	if ((g_dfs_data == NULL) || (g_dfs_data->scenes == NULL))
		return -ENOENT;
	scene = g_dfs_data->scenes;
	for (i = 0; i < g_dfs_data->scene_num; i++) {
		if ((scene[i].scene_count > 0) &&
				(scene[i].scene_freq > target_freq) &&
				(scene[i].vote_magic == magic)) {
			target_freq = scene[i].scene_freq;
		}
	}
	mutex_lock(&g_dfs_data->sync_mutex);
	if (target_freq != last_vote(magic)) {
		err = dfs_vote(target_freq, magic);
		if (err == 0)
			last_vote(magic) = target_freq;
	} else {
		err = 0;
	}
	mutex_unlock(&g_dfs_data->sync_mutex);
	return err;
}

int scene_dfs_request(char *scenario)
{
	struct scene_freq *scene;
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	if (g_dfs_data->init_done != 1)
		return -ENOENT;
	scene = find_scene(scenario);
	if (scene == NULL) {
		pr_err("%s, The scene: %s is invalid\n", __func__, scenario);
		return -EINVAL;
	}
	add_scene(scene);
	trace_sprd_scene(scene, 1);
	err = send_scene_request(scene->vote_magic);
	return err;
}
EXPORT_SYMBOL(scene_dfs_request);

int scene_exit(char *scenario)
{
	struct scene_freq *scene;
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	if (g_dfs_data->init_done != 1)
		return -ENOENT;
	scene = find_scene(scenario);
	if (scene == NULL) {
		pr_err("%s, The scene: %s is invalid\n", __func__, scenario);
		return -EINVAL;
	}
	del_scene(scene);
	trace_sprd_scene(scene, 0);
	err = send_scene_request(scene->vote_magic);
	return err;
}
EXPORT_SYMBOL(scene_exit);

int change_scene_freq(char *scenario, unsigned int freq)
{
	struct scene_freq *scene;
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	if (g_dfs_data->init_done != 1)
		return -ENOENT;
	scene = find_scene(scenario);
	if (scene == NULL) {
		pr_err("%s, The scene: %s is invalid\n", __func__, scenario);
		return -EINVAL;
	}
	spin_lock(&g_dfs_data->lock);
		scene->scene_freq = freq;
	spin_unlock(&g_dfs_data->lock);
	err = send_scene_request(scene->vote_magic);
	return err;
}
EXPORT_SYMBOL(change_scene_freq);

int force_freq_request(unsigned int freq)
{
	unsigned int data;
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	if (g_dfs_data->init_done != 1)
		return -ENOENT;
	mutex_lock(&g_dfs_data->sync_mutex);
	err = dfs_msg(&data, freq, DFS_CMD_SET_DDR_FREQ, 500);
	if (err == 0)
		g_dfs_data->force_freq = freq;
	mutex_unlock(&g_dfs_data->sync_mutex);
	return err;
}

int set_backdoor(void)
{
	if (g_dfs_data->backdoor_freq == 0)
		return -EINVAL;

	return force_freq_request(g_dfs_data->backdoor_freq);
}

int reset_backdoor(void)
{
	if (g_dfs_data->backdoor_freq == 0)
		return -EINVAL;

	return dfs_auto_enable();
}

int get_dfs_status(unsigned int *data)
{
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	err =  dfs_msg(data, 0, DFS_CMD_INQ_STATUS, 500);
	return err;
}

int get_dfs_auto_status(unsigned int *data)
{
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	err = dfs_msg(data, 0, DFS_CMD_INQ_AUTO_STATUS, 500);
	return err;
}

int get_freq_num(unsigned int *data)
{
	if (g_dfs_data == NULL)
		return -ENOENT;
	*data = g_dfs_data->freq_num;
	return 0;
}

int get_freq_table(unsigned int *data, unsigned int sel)
{
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	if (g_dfs_data->init_done != 1) {
		err = dfs_msg(data, sel, DFS_CMD_INQ_DDR_TABLE, 500);
	} else {
		*data = g_dfs_data->freq_table[sel];
		err = 0;
	}
	return err;
}

int get_cur_freq(unsigned int *data)
{
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	err = dfs_msg(data, 0, DFS_CMD_INQ_DDR_FREQ, 500);
	return err;
}

int get_ap_freq(unsigned int *data)
{
	if (g_dfs_data == NULL)
		return -ENOENT;
	if (g_dfs_data->init_done != 1)
		return -ENOENT;
	*data = last_vote(0);
	return 0;
}

int get_cp_freq(unsigned int *data)
{
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	err = dfs_msg(data, 0, DFS_CMD_INQ_CP_FREQ, 500);
	return err;
}

int get_force_freq(unsigned int *data)
{
	if (g_dfs_data == NULL)
		return -ENOENT;
	if (g_dfs_data->init_done != 1)
		return -ENOENT;
	*data = g_dfs_data->force_freq;
	return 0;
}

int get_overflow(unsigned int *data, unsigned int sel)
{
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	if (g_dfs_data->init_done != 1) {
		err = dfs_msg(data, sel, DFS_CMD_INQ_OVERFLOW, 500);
	} else {
		*data = g_dfs_data->overflow[sel];
		err = 0;
	}
	return err;
}

int get_underflow(unsigned int *data, unsigned int sel)
{
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	if (g_dfs_data->init_done != 1) {
		err = dfs_msg(data, sel, DFS_CMD_INQ_UNDERFLOW, 500);
	} else {
		*data = g_dfs_data->underflow[sel];
		err = 0;
	}
	return err;
}

int get_timer(unsigned int *data)
{
	int err;

	if (g_dfs_data == NULL)
		return -ENOENT;
	if (g_dfs_data->init_done != 1) {
		err = dfs_msg(data, 0, DFS_CMD_INQ_TIMER, 500);
	} else {
		*data = g_dfs_data->profile->polling_ms;
		err = 0;
	}
	return err;
}

int get_scene_num(unsigned int *data)
{
	if (g_dfs_data == NULL)
		return -ENOENT;
	*data = g_dfs_data->scene_num;
	return 0;
}

int get_scene_info(char **name, unsigned int *freq,
			unsigned int *count, unsigned int *magic, int index)
{
	struct scene_freq *scene;

	if (g_dfs_data == NULL)
		return -ENOENT;
	if (index >= g_dfs_data->scene_num)
		return -EINVAL;
	scene = &g_dfs_data->scenes[index];
	*name = scene->scene_name;
	*freq = scene->scene_freq;
	*count = scene->scene_count;
	*magic = scene->vote_magic;
	return 0;
}

int set_overflow(unsigned int value, unsigned int sel)
{
	int err;
	unsigned int data;

	if (g_dfs_data == NULL)
		return -ENOENT;
	mutex_lock(&g_dfs_data->sync_mutex);
	err = dfs_msg(&data, value, DFS_CMD_PARA_OVERFLOW+sel, 500);
	if ((err == 0) && (g_dfs_data->init_done == 1))
		g_dfs_data->overflow[sel] = value;
	mutex_unlock(&g_dfs_data->sync_mutex);
	return err;
}

int set_underflow(unsigned int value, unsigned int sel)
{
	int err;
	unsigned int data;

	if (g_dfs_data == NULL)
		return -ENOENT;
	mutex_lock(&g_dfs_data->sync_mutex);
	err = dfs_msg(&data, value, DFS_CMD_PARA_UNDERFLOW+sel, 500);
	if ((err == 0) && (g_dfs_data->init_done == 1))
		g_dfs_data->underflow[sel] = value;
	mutex_unlock(&g_dfs_data->sync_mutex);
	return err;
}

static int dfs_freq_target(struct device *dev, unsigned long *freq,
				u32 flags)
{
	int err;

	err = force_freq_request(*freq);
	if (err < 0)
		pr_err("%s: set freq fail: %d\n", __func__, err);
	return err;
}

static int dfs_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	return 0;
}

static void dfs_exit(struct device *dev)
{
}

static int dfs_smsg_thread(void *dfs_data)
{
	struct dfs_data *data = (struct dfs_data *)dfs_data;
	int i, err;

	err = smsg_ch_open(SIPC_ID_PM_SYS, SMSG_CH_PM_CTRL, -1);
	if (err < 0) {
		pr_err("%s, open sipc channel failed:%d\n", __func__, err);
		return 0;
	}

	do {
		err = dfs_enable();
	} while (err < 0);

	for (i = 0; i < data->freq_num; i++) {
		err = get_freq_table(&data->freq_table[i], i);
		if (err < 0)
			return 0;
	}
	if (data->overflow[0] != 0) {
		for (i = 0; i < data->freq_num; i++) {
			err = set_overflow(data->overflow[i], i);
			if (err < 0)
				return 0;
		}
	} else {
		for (i = 0; i < data->freq_num; i++) {
			err = get_overflow(&data->overflow[i], i);
			if (err < 0)
				return 0;
		}
	}
	if (data->underflow[0] != 0) {
		for (i = 0; i < data->freq_num; i++) {
			err = set_underflow(data->underflow[i], i);
			if (err < 0)
				return 0;
		}
	} else {
		for (i = 0; i < data->freq_num; i++) {
			err = get_underflow(&data->underflow[i], i);
			if (err < 0)
				return 0;
		}
	}


	err = get_cur_freq((unsigned int *)(&data->profile->initial_freq));
	if (err < 0)
		return 0;
	err = get_timer(&data->profile->polling_ms);
	if (err < 0)
		return 0;

	data->devfreq->min_freq = data->freq_table[0];
	data->devfreq->max_freq = data->freq_table[data->freq_num - 1];

	err = dfs_auto_enable();
	if (err < 0)
		return err;
	data->init_done = 1;

	return 0;
}

static int dfs_auto_freq_probe(struct platform_device *pdev)
{
	struct dfs_data *data;
	struct device *dev = &pdev->dev;
	unsigned int freq_num;
	int scene_num;
	void *p;
	int err, i;

	if (g_dfs_data != NULL) {
		pr_err("dfs core can used by single device only");
		return -ENOMEM;
	}

	err = of_property_read_u32(dev->of_node, "freq-num", &freq_num);
	if (err != 0) {
		pr_err("failed read freqnum\n");
		freq_num = 8;
	}
	scene_num = of_property_count_strings(dev->of_node, "sprd-scene");
	if (scene_num < 0)
		scene_num = 0;

	p = kzalloc(sizeof(struct dfs_data)+sizeof(struct devfreq_dev_profile)
			+sizeof(unsigned int)*freq_num*3
			+sizeof(struct scene_freq)*scene_num, GFP_KERNEL);
	if (p == NULL) {
		err = -ENOMEM;
		goto err;
	}
	data = (struct dfs_data *)p;
	g_dfs_data = data;
	p += sizeof(struct dfs_data);
	data->profile = (struct devfreq_dev_profile *)p;
	p += sizeof(struct devfreq_dev_profile);
	data->freq_table = (unsigned int *)p;
	p += sizeof(unsigned int)*freq_num;
	data->overflow = (unsigned int *)p;
	p += sizeof(unsigned int)*freq_num;
	data->underflow = (unsigned int *)p;
	p += sizeof(unsigned int)*freq_num;
	data->scenes = (struct scene_freq *)p;

	data->dev = dev;
	data->freq_num = freq_num;
	data->scene_num = scene_num;
	spin_lock_init(&data->lock);
	mutex_init(&data->sync_mutex);

	err = of_property_read_u32(dev->of_node, "backdoor",
					&data->backdoor_freq);
	if (err != 0) {
		pr_err("failed read freqnum\n");
		data->backdoor_freq = 0;
	}

	err = of_property_read_u32_array(dev->of_node, "overflow",
					data->overflow, freq_num);
	if (err != 0)
		memset(data->overflow, 0, sizeof(unsigned int)*freq_num);

	err = of_property_read_u32_array(dev->of_node, "underflow",
					data->underflow, freq_num);
	if (err != 0)
		memset(data->underflow, 0, sizeof(unsigned int)*freq_num);

	for (i = 0; i < scene_num; i++) {
		err = of_property_read_string_index(dev->of_node, "sprd-scene",
				i, (const char **)&data->scenes[i].scene_name);
		if (err < 0 || strlen(data->scenes[i].scene_name) == 0)
			goto err_data;
		err = of_property_read_u32_index(dev->of_node,
				"sprd-freq", i, &data->scenes[i].scene_freq);
		if (err)
			goto err_data;
		err = of_property_read_u32_index(dev->of_node,
				"scene-magic", i, &data->scenes[i].vote_magic);
		if (err)
			data->scenes[i].vote_magic = 0;
		data->scenes[i].scene_count = 0;
	}
	platform_set_drvdata(pdev, data);
	data->profile->target = dfs_freq_target;
	data->profile->get_dev_status = dfs_get_dev_status;
	data->profile->exit = dfs_exit;

	data->devfreq = devfreq_add_device(data->dev, data->profile,
						"sprd_governor", NULL);
	if (IS_ERR(data->devfreq)) {
		err = PTR_ERR(data->devfreq);
		goto err_data;
	}

	data->dfs_smsg_ch_open = kthread_run(dfs_smsg_thread, data,
			"dfs-%d-%d", SIPC_ID_PM_SYS, SMSG_CH_PM_CTRL);
	if (IS_ERR(data->dfs_smsg_ch_open)) {
		err = -EINVAL;
		goto err_device;
	}

	return 0;
err_device:
	devfreq_remove_device(data->devfreq);
err_data:
	kfree(p);
err:
	return err;

}

static int dfs_auto_freq_remove(struct platform_device *pdev)
{
	struct dfs_data *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);
	kfree(data);
	return 0;
}

#ifdef CONFIG_DEVFREQ_GOV_SPRD_EXT_VOTE
static int dfs_auto_freq_remove(struct platform_device *pdev)
{
	dfs_ext_vote_resume();
	return 0;
}
#endif

static const struct of_device_id dfs_auto_freq_match[] = {
	{ .compatible = "sprd,dfs" },
	{},
};

MODULE_DEVICE_TABLE(of, dfs_freq_match);

static struct platform_driver dfs_auto_freq_drvier = {
	.probe = dfs_auto_freq_probe,
	.remove = dfs_auto_freq_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_dfs_auto",
#ifdef CONFIG_OF
		.of_match_table = dfs_auto_freq_match,
#endif
		},
#ifdef CONFIG_DEVFREQ_GOV_SPRD_EXT_VOTE
	.resume = dfs_auto_resume,
#endif
};

module_platform_driver(dfs_auto_freq_drvier);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("dfs driver for sprd ddrc v1 and later");
