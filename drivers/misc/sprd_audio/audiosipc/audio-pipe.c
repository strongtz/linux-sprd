/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) "[Audio:PIPE] "fmt

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "audio-sipc.h"
#include "sprd-string.h"

#define sp_asoc_pr_dbg pr_debug
#define sp_asoc_pr_info pr_info

#define  AGDSP_COMMUNICATION_TIMEOUT	0x27

#define	AUDIO_PIPE_MARGIC		'A'

#define	AUDIO_PIPE_WAKEUP		_IOW(AUDIO_PIPE_MARGIC, 0, int)
#define	AUDIO_PIPE_BTHAL_STATE_SET		_IOW(AUDIO_PIPE_MARGIC, 1, int)
#define	AUDIO_PIPE_BTHAL_STATE_GET		_IOR(AUDIO_PIPE_MARGIC, 0, int)

#define BTHAL_STATE_RUNNING		0
#define BTHAL_STATE_IDLE		1

enum {
	SPRD_PIPE_VOICE,
	SPRD_PIPE_EFFECT,
	SPRD_PIPE_RECORD_PROCESS,
	SPRD_PIPE_BTHAL,
	SPRD_PIPE_TYPE_MAX,
};

#define SPRD_PIPE_NAME_MAX 64

struct aud_pipe_device {
	struct miscdevice misc_dev;
	char device_name[SPRD_PIPE_NAME_MAX];
	u32 openned;
	u32 channel;
	void *tmp_buffer;
	u32 writesync;
	u32 maxwritebufsize;
	struct mutex mutex;
};

static u32 g_bthal_state = BTHAL_STATE_IDLE;

static int aud_pipe_recv_cmd(uint16_t channel, struct aud_smsg *o_msg,
	int32_t timeout)
{
	int ret = 0;
	struct aud_smsg mrecv = { 0 };

	aud_smsg_set(&mrecv, channel, 0, 0, 0, 0, 0);

	ret = aud_smsg_recv(AUD_IPC_AGDSP, &mrecv, timeout);
	if (ret < 0) {
		if ((channel == AMSG_CH_DSP_ASSERT_CTL)
			&& (ret == -EPIPE)) {
			pr_err("%s, Failed to recv,ret(%d) and notify\n",
				__func__, ret);
			mrecv.command = AGDSP_COMMUNICATION_TIMEOUT;
			mrecv.parameter0 = AGDSP_COMMUNICATION_TIMEOUT;
		} else {
			if (ret == -ENODATA) {
				pr_warn("%s  ENODATA\n", __func__);
				return ret;
			}
			pr_err("%s, Failed to recv,ret(%d)\n",
				__func__, ret);
			return ret;
		}
	}

	sp_asoc_pr_dbg("%s, chan: 0x%x, cmd: 0x%x\n",
		__func__, mrecv.channel, mrecv.command);
	sp_asoc_pr_dbg("value0: 0x%x, value1: 0x%x\n",
		mrecv.parameter0, mrecv.parameter1);
	sp_asoc_pr_dbg(" value2: 0x%x, value3: 0x%x, timeout: %d\n",
		mrecv.parameter2, mrecv.parameter3, timeout);

	if (mrecv.channel == channel) {
		unalign_memcpy(o_msg, &mrecv, sizeof(struct aud_smsg));
		return 0;
	}
	pr_err("%s,wrong chan(0x%x) from agdsp\n",
		__func__, mrecv.channel);

	return ret;
}

static int aud_pipe_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	/*
	 * The miscdevice layer puts our struct miscdevice into the
	 * filp->private_data field. We use this to find our private
	 * data and then overwrite it with our own private structure.
	 */
	struct aud_pipe_device *aud_pipe_dev = container_of(filp->private_data,
			struct aud_pipe_device, misc_dev);

	filp->private_data = aud_pipe_dev;
	mutex_lock(&aud_pipe_dev->mutex);
	if (!aud_pipe_dev->openned) {
		ret = aud_ipc_ch_open(aud_pipe_dev->channel);
		if (ret) {
			pr_err("%s channel(%u)_open failed\n",
				__func__, aud_pipe_dev->channel);
			return ret;
		}
		aud_pipe_dev->openned++;
		pr_info("%s channel opened %u\n", __func__,
			aud_pipe_dev->channel);
		if (aud_pipe_dev->maxwritebufsize) {
			aud_pipe_dev->tmp_buffer =
				vmalloc(aud_pipe_dev->maxwritebufsize);
			if (aud_pipe_dev->tmp_buffer == NULL) {
				ret = aud_ipc_ch_close(aud_pipe_dev->channel);
				pr_err("%s memory alloc  error\n", __func__);
				return -ENOMEM;
			}
		}
	} else
		aud_pipe_dev->openned++;

	mutex_unlock(&aud_pipe_dev->mutex);
	return 0;
}

static int aud_pipe_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct aud_pipe_device *aud_pipe_dev = filp->private_data;

	mutex_lock(&aud_pipe_dev->mutex);
	if (aud_pipe_dev->openned) {
		aud_pipe_dev->openned--;
		if (aud_pipe_dev->openned == 0) {
			ret = aud_ipc_ch_close(aud_pipe_dev->channel);
			if (ret != 0) {
				pr_err("%s, Failed to close channel(%d)\n",
					__func__, aud_pipe_dev->channel);
			}
			if (aud_pipe_dev->tmp_buffer)
				vfree(aud_pipe_dev->tmp_buffer);
			filp->private_data = NULL;
		}
	}
	mutex_unlock(&aud_pipe_dev->mutex);
	return ret;
}

static ssize_t aud_pipe_read(struct file *filp,
	char __user *buf, size_t count, loff_t *ppos)
{
	int timeout = -1;
	struct aud_smsg  user_msg_out = {0};
	int ret = 0;
	struct aud_pipe_device *aud_pipe_dev = filp->private_data;

	if (sizeof(struct aud_smsg) != count) {
		pr_err("input not a struct aud_smsg type\n");
		ret = -EINVAL;
		goto error;
	}
	ret = aud_pipe_recv_cmd(aud_pipe_dev->channel,
		&user_msg_out, timeout);
	if (ret == 0)
		ret = sizeof(struct aud_smsg);
	else
		pr_err("%s aud_pipe_recv_cmd failed\n", __func__);

	if (unalign_copy_to_user((void __user *)buf,
		&user_msg_out, sizeof(struct aud_smsg))) {
		pr_err("%s: failed to copy to user!\n", __func__);
		ret = -EFAULT;
		goto error;
	}

	pr_info("user_msg_out.channel=%#hx\n", user_msg_out.channel);
	pr_info("user_msg_out.command=%#hx\n", user_msg_out.command);
	pr_info("user_msg_out.parameter0=%#x\n", user_msg_out.parameter0);
	pr_info("user_msg_out.parameter1=%#x\n", user_msg_out.parameter1);
	pr_info("user_msg_out.parameter2=%#x\n", user_msg_out.parameter2);
	pr_info("user_msg_out.parameter3=%#x\n", user_msg_out.parameter3);

	return ret;
error:
	pr_err("%s failed", __func__);

	return ret;
}

static ssize_t aud_pipe_write(struct file *filp,
	const char __user *buf, size_t count, loff_t *ppos)
{
	struct aud_smsg  user_msg_from_user = {0};
	int ret = 0;
	int timeout = -1;
	struct aud_pipe_device *aud_pipe_dev = filp->private_data;

	pr_info("%s:aud_pipe_data->channel:%d,count:%zu\n",
		__func__, aud_pipe_dev->channel, count);
	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
		pr_info("%s noblock %u,timeout = %d\n", __func__,
			filp->f_flags & O_NONBLOCK, timeout);
	}
	if ((!aud_pipe_dev->writesync) &&
		(count == sizeof(struct aud_smsg))) {
		if (unalign_copy_from_user(&user_msg_from_user,
			(void __user *)buf, sizeof(struct aud_smsg))) {
			pr_err("%s failed unalign_copy_from_user\n", __func__);
			ret = -EFAULT;
			goto error;
		}
		pr_info("user_msg_in.channel=%#hx\n",
			user_msg_from_user.channel);
		pr_info("user_msg_in.command=%#hx\n",
			user_msg_from_user.command);
		pr_info("user_msg_in.parameter0=%#x\n",
			user_msg_from_user.parameter0);
		pr_info("user_msg_in.parameter1=%#x\n",
			user_msg_from_user.parameter1);
		pr_info("user_msg_in.parameter2=%#x\n",
			user_msg_from_user.parameter2);
		pr_info("user_msg_in.parameter3=%#x\n",
			user_msg_from_user.parameter3);
		ret = aud_send_cmd_no_wait(aud_pipe_dev->channel,
			user_msg_from_user.command,
			user_msg_from_user.parameter0,
			user_msg_from_user.parameter1,
			user_msg_from_user.parameter2,
			user_msg_from_user.parameter3);
		if (ret != 0) {
			pr_err("%s aud_send_msg failed\n", __func__);
			ret = -1;
			goto error;
		}

		ret = sizeof(struct aud_smsg);
	} else {
		if (count > aud_pipe_dev->maxwritebufsize) {
			pr_err("%s failed unalign_copy_from_user count:%zu,maxwritesize:%d\n",
				__func__, count, aud_pipe_dev->maxwritebufsize);
			ret = -EFAULT;
			goto error;
		}
		if (unalign_copy_from_user(aud_pipe_dev->tmp_buffer,
			buf, count)) {
			pr_err("%s failed unalign_copy_from_user\n", __func__);
			ret = -EFAULT;
			goto error;
		}
		ret = aud_send_cmd(aud_pipe_dev->channel, 0, -1, 0,
			(void *)aud_pipe_dev->tmp_buffer, count,
			AUDIO_SIPC_WAIT_FOREVER);
		if (ret != 0) {
			pr_err("%s aud_send_cmd failed\n", __func__);
			ret = -1;
			goto error;
		}
		ret = count;
		pr_info("%s aud_pipe_data->channel:%d,count:%zu\n",
			__func__, aud_pipe_dev->channel, count);
	}
	return ret;
error:
	pr_err("%s failed", __func__);

	return ret;
}

static long aud_pipe_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct aud_pipe_device *aud_pipe_dev = filp->private_data;
	void __user *argp = (void __user *)(arg);

	if (!aud_pipe_dev)
		return -EINVAL;

	switch (cmd) {
	case AUDIO_PIPE_WAKEUP:
		aud_smsg_wakeup_ch(AUD_IPC_AGDSP, aud_pipe_dev->channel);
		break;
	case AUDIO_PIPE_BTHAL_STATE_SET:
		if (get_user(g_bthal_state, (u32 __user *)argp))
			return -EFAULT;
		pr_info("BTHAL_STATE_SET:%d\n", g_bthal_state);
		break;
	case AUDIO_PIPE_BTHAL_STATE_GET:
		pr_info("BTHAL_STATE_GET:%d\n", g_bthal_state);
		if (put_user(g_bthal_state, (u32 __user *)argp))
			return -EFAULT;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct file_operations audio_pipe_fops = {
	.open = aud_pipe_open,
	.release = aud_pipe_release,
	.read = aud_pipe_read,
	.write = aud_pipe_write,
	.unlocked_ioctl = aud_pipe_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = aud_pipe_ioctl,
#endif
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct of_device_id audio_pipe_match_table[] = {
	{.compatible = "sprd,audio_pipe_voice",
		.data = (void *)SPRD_PIPE_VOICE},
	{.compatible = "sprd,audio_pipe_effect",
		.data = (void *)SPRD_PIPE_EFFECT},
	{.compatible = "sprd,audio_pipe_recordproc",
		.data = (void *)SPRD_PIPE_RECORD_PROCESS},
	{.compatible = "sprd,audio_pipe_bthal",
		.data = (void *)SPRD_PIPE_BTHAL},
	{ },
};

static int aud_pipe_parse_dt(struct device *dev,
	struct aud_pipe_device *aud_pipe_dev)
{
	struct device_node *np = dev->of_node;
	int ret;
	const struct of_device_id *of_id;
	long type;

	if (!np) {
		ret = -EPROBE_DEFER;
		pr_err("%s defer\n", __func__);
		goto error;
	}

	of_id = of_match_node(audio_pipe_match_table, np);
	if (!of_id) {
		pr_err("%s Get device id failed deffer!\n", __func__);
		ret = -EPROBE_DEFER;
		goto error;
	}

	type = (long)of_id->data;
	switch (type) {
	case SPRD_PIPE_VOICE:
		strncpy(aud_pipe_dev->device_name,
			"audio_pipe_voice", SPRD_PIPE_NAME_MAX);
		break;
	case SPRD_PIPE_EFFECT:
		strncpy(aud_pipe_dev->device_name,
			"audio_pipe_effect", SPRD_PIPE_NAME_MAX);
		break;
	case SPRD_PIPE_RECORD_PROCESS:
		strncpy(aud_pipe_dev->device_name,
			"audio_pipe_recordproc", SPRD_PIPE_NAME_MAX);
		break;
	case SPRD_PIPE_BTHAL:
		strncpy(aud_pipe_dev->device_name,
			"audio_pipe_bthal", SPRD_PIPE_NAME_MAX);
		break;
	default:
		ret = -EINVAL;
		pr_err("%s invalid pipe type %ld\n", __func__, type);
		goto error;
	}
	ret = of_property_read_u32(np,
		"sprd,writesync", &(aud_pipe_dev->writesync));
	if (ret) {
		pr_err("%s  sprd,writesync, failed\n", __func__);
		goto error;
	}
	ret = of_property_read_u32(np,
		"sprd,maxuserwritebufsize", &(aud_pipe_dev->maxwritebufsize));
	if (ret) {
		pr_err("%s  sprd,maxwritebufsize, failed\n", __func__);
		goto error;
	}
	ret = of_property_read_u32(np,
		"sprd,channel", &(aud_pipe_dev->channel));
	if (ret) {
		pr_err("%s  sprd,channel,core failed\n", __func__);
		goto error;
	}
	ret = 0;
	pr_info("aud_pipe_data->channel:%d\n", aud_pipe_dev->channel);

	return ret;
error:
	pr_err("%s failed", __func__);

	return ret;
}

static int audio_pipe_probe(struct platform_device *pdev)
{
	int rval = 0;
	struct aud_pipe_device *aud_pipe_dev;

	aud_pipe_dev = devm_kzalloc(&pdev->dev, sizeof(struct aud_pipe_device),
		GFP_KERNEL);
	if (!aud_pipe_dev)
		return -ENOMEM;

	rval = aud_pipe_parse_dt(&pdev->dev, aud_pipe_dev);
	if (rval != 0) {
		pr_err("%s parse dt failed\n", __func__);
		return rval;
	}
	pr_info("%s:name=%s\n", __func__,
		aud_pipe_dev->device_name);
	aud_pipe_dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	aud_pipe_dev->misc_dev.name = aud_pipe_dev->device_name;
	aud_pipe_dev->misc_dev.fops = &audio_pipe_fops;
	aud_pipe_dev->misc_dev.parent = NULL;
	rval = misc_register(&aud_pipe_dev->misc_dev);
	if (rval != 0) {
		pr_err("Failed to alloc audio_pipe chrdev\n");
		return rval;
	}
	mutex_init(&aud_pipe_dev->mutex);
	dev_set_drvdata(&pdev->dev, aud_pipe_dev);

	return 0;
}

static int audio_pipe_remove(struct platform_device *pdev)
{
	struct aud_pipe_device *aud_pipe_dev = platform_get_drvdata(pdev);

	if (aud_pipe_dev) {
		misc_deregister((struct miscdevice *)&aud_pipe_dev->misc_dev);
		devm_kfree(&pdev->dev, aud_pipe_dev);
	}
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver audio_pipe_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_audio_pipe",
		.of_match_table = audio_pipe_match_table,
	},
	.probe = audio_pipe_probe,
	.remove = audio_pipe_remove,
};

static int __init audio_pipe_init(void)
{
	return platform_driver_register(&audio_pipe_driver);
}

static void __exit audio_pipe_exit(void)
{
	platform_driver_unregister(&audio_pipe_driver);
}

module_init(audio_pipe_init);
module_exit(audio_pipe_exit);

MODULE_AUTHOR("SPRD");
MODULE_DESCRIPTION("SIPC/AUDIO_PIPE driver");
MODULE_LICENSE("GPL");
