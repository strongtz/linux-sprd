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
#define pr_fmt(fmt) "[saudio] "fmt

#include <linux/err.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/memory.h>
#include <linux/mutex.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#endif
#include <linux/platform_device.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <sound/control.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/tlv.h>

#include "saudio.h"

#define ETRACE(x...) pr_err("Error: " x)
#define WTRACE(x...) pr_warn(x)

#define ADEBUG() pr_debug("saudio.c: function: %s,line %d\n", \
	__func__, __LINE__)

#define ITRACE(fmt, ...) pr_info("%s " fmt, __func__, ##__VA_ARGS__)

#define CMD_BLOCK_SIZE			80
#define TX_DATA_BLOCK_SIZE		80
#define RX_DATA_BLOCK_SIZE		0
#define MAX_BUFFER_SIZE			(10 * 1024)

#define MAX_PERIOD_SIZE			(MAX_BUFFER_SIZE)

#define USE_FORMATS		(SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE)

#define USE_RATE (SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000)
#define USE_RATE_MIN			5500
#define USE_RATE_MAX			48000

#define USE_CHANNELS_MIN		1

#define USE_CHANNELS_MAX		2

#define USE_PERIODS_MIN			1

#define USE_PERIODS_MAX			1024

#define CMD_TIMEOUT			msecs_to_jiffies(5000)
#define CMD_MODEM_RESET_TIMEOUT		msecs_to_jiffies(10000)

#define SAUDIO_CMD_NONE			0x00000000
#define SAUDIO_CMD_OPEN			0x00000001
#define SAUDIO_CMD_CLOSE		0x00000002
#define SAUDIO_CMD_START		0x00000004
#define SAUDIO_CMD_STOP			0x00000008
#define SAUDIO_CMD_PREPARE		0x00000010
#define SAUDIO_CMD_TRIGGER              0x00000020
#define SAUDIO_CMD_HANDSHAKE            0x00000040
#define SAUDIO_CMD_RESET		0x00000080
#define SAUDIO_CMD_SET_PLAYBACK_ROUTE	0x00000100
#define SAUDIO_CMD_SET_CAPTURE_ROUTE	0x00000200
#define SAUDIO_CMD_ENABLE_LOOP		0x00000400
#define SAUDIO_CMD_TYPE_LOOP		0x00000800
#define SAUDIO_CMD_SET_MIXER_ROUTE	0x00001000
#define SAUDIO_CMD_SET_APPTYPE		0x00002000
#define SAUDIO_CMD_SET_UL_GAIN	0x00004000
#define SAUDIO_CMD_SET_DL_GAIN	0x00008000

#define SAUDIO_CMD_OPEN_RET		0x00010000
#define SAUDIO_CMD_CLOSE_RET		0x00020000
#define SAUDIO_CMD_START_RET		0x00040000
#define SAUDIO_CMD_STOP_RET             0x00080000
#define SAUDIO_CMD_PREPARE_RET		0x00100000
#define SAUDIO_CMD_TRIGGER_RET		0x00200000
#define SAUDIO_CMD_HANDSHAKE_RET	0x00400000
#define SAUDIO_CMD_RESET_RET		0x00800000
#define SAUDIO_CMD_SET_PLAYBACK_ROUTE_RET	0x01000000
#define SAUDIO_CMD_SET_CAPTURE_ROUTE_RET	0x02000000
#define SAUDIO_CMD_ENABLE_LOOP_RET		0x04000000
#define SAUDIO_CMD_TYPE_LOOP_RET		0x08000000
#define SAUDIO_CMD_SET_MIXER_ROUTE_RET		0x10000000
#define SAUDIO_CMD_SET_APPTYPE_RET		0x20000000

#define SAUDIO_DATA_PCM			0x00000040
#define SAUDIO_DATA_SILENCE		0x00000080

#define PLAYBACK_DATA_ABORT		0x00000001
#define PLAYBACK_DATA_BREAK		0x00000002

#define  SAUDIO_DEV_CTRL_ABORT		0x00000001
#define SAUDIO_DEV_CTRL_BREAK		0x00000002

#define SAUDIO_SUBCMD_CAPTURE		0x0
#define SAUDIO_SUBCMD_PLAYBACK		0x1

#define SAUDIO_DEV_MAX			2
#define SAUDIO_STREAM_MAX		2
#define SAUDIO_CARD_NAME_LEN_MAX	16

#define  SAUDIO_STREAM_BLOCK_COUNT	4
#define  SAUDIO_CMD_BLOCK_COUNT		4

#define  SAUDIO_MONITOR_BLOCK_COUNT	2
#define  SAUDIO_MONITOR_BLOCK_COUNT	2

struct cmd_common {
	unsigned int command;
	unsigned int sub_cmd;
	unsigned int reserved1;
	unsigned int reserved2;
};

struct cmd_prepare {
	struct cmd_common common;
	unsigned int rate;	/* rate in Hz */
	unsigned char channels;	/* channels */
	unsigned char format;
	unsigned char reserved1;
	unsigned char reserved2;
	unsigned int period;	/* period size */
	unsigned int periods;	/* periods */
};

struct cmd_open {
	struct cmd_common common;
	u32 stream_type;
};

struct saudio_msg {
	u32 command;
	u32 stream_id;
	u32 reserved1;
	u32 reserved2;
	void *param;
};

enum snd_status {
	SAUDIO_IDLE,
	SAUDIO_OPENNED,
	SAUDIO_CLOSED,
	SAUDIO_STOPPED,
	SAUDIO_PREPARED,
	SAUDIO_TRIGGERED,
	SAUDIO_PARAMIZED,
	SAUDIO_ABORT,
};

struct saudio_dev_ctrl;
struct snd_saudio;

struct saudio_stream {
	struct snd_saudio *saudio;
	struct snd_pcm_substream *substream;
	struct saudio_dev_ctrl *dev_ctrl;
	int stream_id;		/* numeric identification */

	u32 stream_state;

	u32 dst;
	u32 channel;

	s32 period;
	s32 periods_avail;
	s32 periods_tosend;

	u32 hwptr_done;

	u32 last_elapsed_count;
	u32 last_getblk_count;
	u32 blk_count;
	/* mutex for seriealize the playback command send to cp*/
	struct mutex mutex;
};

struct saudio_dev_ctrl {
	u32 dev_state;
	/* mutex for seriealize the ctrl command send to cp*/
	struct mutex mutex;
	u32 dst;
	u32 monitor_channel;
	u32 channel;
	u8 name[SAUDIO_CARD_NAME_LEN_MAX];
	struct saudio_stream stream[SAUDIO_STREAM_MAX];
};

struct snd_saudio {
	struct snd_card *card;
	struct snd_pcm *pcm[SAUDIO_DEV_MAX];
	struct saudio_dev_ctrl dev_ctrl[SAUDIO_DEV_MAX];
	u32 device_num;
	u32 ap_addr_to_cp;
	wait_queue_head_t wait;
	struct platform_device *pdev;
	struct workqueue_struct *queue;
	struct work_struct card_free_work;
	u32 dst;
	u32 channel;
	u32 in_init;
	struct task_struct *thread_id;
	s32 state;
	/* mutex for seriealize reference the  saudio card*/
	struct mutex mutex;
	u32 kcon_val_playback;
	u32 kcon_val_capture;
	u32 kcon_val_en;
	u32 kcon_val_type;
	u32 kcon_mixer_route;
	u32 kcon_apptype;
	u32 kcon_dl_gain;
	u32 kcon_ul_gain;
};

static DEFINE_MUTEX(snd_sound);

static int saudio_snd_card_free(const struct snd_saudio *saudio);

static int snd_pcm_playback_control_info(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_info *uinfo);

static int snd_pcm_playback_control_route_get(
			    struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_playback_control_route_put(
			    struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_capture_control_info(
			    struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo);

static int snd_pcm_capture_control_route_get(
			    struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_capture_control_route_put(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_loop_enable_control_info(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo);

static int snd_pcm_loop_enable_control_get(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_loop_enable_control_put(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_loop_type_control_info(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo);

static int snd_pcm_loop_type_control_get(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_loop_type_control_put(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_mixer_route_control_info(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo);

static int snd_pcm_mixer_route_control_get(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_mixer_route_control_put(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_apptype_control_info(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo);

static int snd_pcm_apptype_control_get(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_apptype_control_put(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_ul_gain_control_info(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo);

static int snd_pcm_ul_gain_control_get(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_ul_gain_control_put(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_dl_gain_control_info(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo);

static int snd_pcm_dl_gain_control_get(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static int snd_pcm_dl_gain_control_put(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);

static struct snd_kcontrol_new playbackcontroldata = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "PCM PLAYBACK Route",
	.count = 1,
	.info = snd_pcm_playback_control_info,
	.get = snd_pcm_playback_control_route_get,
	.put = snd_pcm_playback_control_route_put
};

static struct snd_kcontrol_new capturecontroldata = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "PCM CAPTURE Route",
	.count = 1,
	.info = snd_pcm_capture_control_info,
	.get = snd_pcm_capture_control_route_get,
	.put = snd_pcm_capture_control_route_put
};

static struct snd_kcontrol_new loopenabledata = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "PCM LOOP Enable",
	.count = 1,
	.info = snd_pcm_loop_enable_control_info,
	.get = snd_pcm_loop_enable_control_get,
	.put = snd_pcm_loop_enable_control_put
};

static struct snd_kcontrol_new looptypedata = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "PCM LOOP Type",
	.count = 1,
	.info = snd_pcm_loop_type_control_info,
	.get = snd_pcm_loop_type_control_get,
	.put = snd_pcm_loop_type_control_put
};

static struct snd_kcontrol_new mixerroutedata = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "PCM MIXER Route",
	.count = 1,
	.info = snd_pcm_mixer_route_control_info,
	.get = snd_pcm_mixer_route_control_get,
	.put = snd_pcm_mixer_route_control_put
};

static struct snd_kcontrol_new apptypedata = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "PCM APPTYPE",
	.count = 1,
	.info = snd_pcm_apptype_control_info,
	.get = snd_pcm_apptype_control_get,
	.put = snd_pcm_apptype_control_put
};

static struct snd_kcontrol_new ul_gain = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "UL MIXERGAIN",
	.count = 1,
	.info = snd_pcm_ul_gain_control_info,
	.get = snd_pcm_ul_gain_control_get,
	.put = snd_pcm_ul_gain_control_put
};

static struct snd_kcontrol_new dl_gain = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "DL MIXERGAIN",
	.count = 1,
	.info = snd_pcm_dl_gain_control_info,
	.get = snd_pcm_dl_gain_control_get,
	.put = snd_pcm_dl_gain_control_put
};

static struct snd_kcontrol_new *kctrl_array[] = {
	&playbackcontroldata,
	&capturecontroldata,
	&loopenabledata,
	&looptypedata,
	&mixerroutedata,
	&apptypedata,
	&ul_gain,
	&dl_gain,
};

static int saudio_clear_cmd(u32 dst, uint32_t channel)
{
	int result = 0;
	struct sblock blk = { 0 };

	do {
		result = sblock_receive(dst, channel, (struct sblock *)&blk, 0);
		if (!result)
			sblock_release(dst, channel, &blk);
	} while (!result);

	return result;
}

static int saudio_send_common_cmd_ex(u32 dst,
				     u32 channel,
	u32 cmd,
	u32 subcmd,
	u32 reserve1,
	u32 reserve2,
	int32_t timeout)
{
	int result = 0;
	struct sblock blk = {NULL};

	pr_debug(" dst is %d, channel %d, cmd %x, subcmd %x, reserve1 %d, reserve2 %d\n",
		 dst, channel, cmd, subcmd, reserve1, reserve2);
	result = sblock_get(dst, channel, (struct sblock *)&blk, timeout);
	ITRACE("cmd %x, subcmd %x, reserve1 %d, reserve2 %d!\n",
	       cmd, subcmd, reserve1, reserve2);

	if (result >= 0) {
		struct cmd_common *common = (struct cmd_common *)blk.addr;

		common->command = cmd;
		common->sub_cmd = subcmd;
		common->reserved1 = reserve1;
		common->reserved2 = reserve2;
		blk.length = sizeof(struct cmd_common);
		pr_debug(" dst is %d, channel %d, cmd %x, subcmd %x, reserve1 %d, reserve2 %d send ok\n",
			 dst, channel, cmd, subcmd, reserve1, reserve2);
		result = sblock_send(dst, channel, (struct sblock *)&blk);
	}

	return result;
}

static int saudio_send_common_cmd(u32 dst, uint32_t channel,
				  u32 cmd, uint32_t subcmd, uint32_t device,
		int32_t timeout)
{
	int result = 0;
	struct sblock blk = { 0 };

	ADEBUG();
	pr_debug(" dst is %d, channel %d, cmd %x, subcmd %x\n", dst, channel,
		 cmd, subcmd);
	saudio_clear_cmd(dst, channel);
	result = sblock_get(dst, channel, (struct sblock *)&blk, timeout);
	if (result >= 0) {
		struct cmd_common *common = (struct cmd_common *)blk.addr;

		common->command = cmd;
		common->sub_cmd = subcmd;
		/* add pcm device for playback deepbuffer */
		common->reserved2 = device;
		blk.length = sizeof(struct cmd_common);
		pr_debug(" dst is %d, channel %d, cmd %x, subcmd %x send ok\n",
			 dst, channel, cmd, subcmd);
		result = sblock_send(dst, channel, (struct sblock *)&blk);
	}

	return result;
}

static int saudio_wait_common_cmd(u32 dst, uint32_t channel,
				  u32 cmd, uint32_t subcmd,
				  int32_t timeout)
{
	int result = 0;
	struct sblock blk = { 0 };
	struct cmd_common *common = NULL;

	ADEBUG();
	result = sblock_receive(dst, channel, (struct sblock *)&blk, timeout);
	if (result < 0) {
		ETRACE("sblock_receive dst %d, channel %d result is %d\n", dst,
		       channel, result);
		return result;
	}

	common = (struct cmd_common *)blk.addr;
	pr_debug("dst is %d, channel %d, common->command is %x ,sub cmd %x,\n",
		 dst, channel, common->command, common->sub_cmd);
	if (subcmd) {
		if (common->command == cmd && common->sub_cmd == subcmd)
			result = 0;
		else
			result = -1;
	} else {
		if (common->command == cmd)
			result = 0;
		else
			result = -1;
	}
	sblock_release(dst, channel, &blk);

	return result;
}

static int saudio_clear_ctrl_cmd(struct snd_saudio *saudio)
{
	int result = 0;
	int i = 0;
	struct sblock blk = { 0 };
	struct saudio_dev_ctrl *dev_ctrl = NULL;

	for (i = 0; i < saudio->device_num; i++) {
		dev_ctrl = &saudio->dev_ctrl[i];
		do {
			result =
			    sblock_receive(dev_ctrl->dst, dev_ctrl->channel,
					   (struct sblock *)&blk, 0);
			if (!result) {
				sblock_release(dev_ctrl->dst, dev_ctrl->channel,
					       &blk);
			}
		} while (!result);
	}

	return result;
}

static int saudio_pcm_lib_malloc_pages(struct snd_pcm_substream *substream,
				       size_t size)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dmab = &substream->dma_buffer;

	/* Use the pre-allocated buffer */
	snd_pcm_set_runtime_buffer(substream, dmab);
	runtime->dma_bytes = size;

	return 1;
}

static int saudio_pcm_lib_free_pages(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);

	return 0;
}

static int saudio_snd_mmap(struct snd_pcm_substream *substream,
			   struct vm_area_struct *vma)
{
	ADEBUG();
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	return remap_pfn_range(vma, vma->vm_start,
			       substream->dma_buffer.addr >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static struct snd_pcm_hardware snd_card_saudio_playback = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = USE_FORMATS,
	.rates = USE_RATE,
	.rate_min = USE_RATE_MIN,
	.rate_max = USE_RATE_MAX,
	.channels_min = USE_CHANNELS_MIN,
	.channels_max = USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_min = 64,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = USE_PERIODS_MIN,
	.periods_max = USE_PERIODS_MAX,
	.fifo_size = 0,
};

static struct snd_pcm_hardware snd_card_saudio_capture = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = USE_FORMATS,
	.rates = USE_RATE,
	.rate_min = USE_RATE_MIN,
	.rate_max = USE_RATE_MAX,
	.channels_min = USE_CHANNELS_MIN,
	.channels_max = USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_min = 64,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = USE_PERIODS_MIN,
	.periods_max = USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int snd_card_saudio_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct saudio_stream *stream = NULL;
	int result = 0;
	struct saudio_dev_ctrl *dev_ctrl = NULL;

	ADEBUG();

	mutex_lock(&saudio->mutex);
	if (!saudio->state) {
		mutex_unlock(&saudio->mutex);
		pr_err("saudio.c: snd_pcm_open error saudio state %d\n",
		       saudio->state);
		return -EIO;
	}
	mutex_unlock(&saudio->mutex);

	pr_info("%s IN, stream_id=%d\n", __func__, stream_id);
	dev_ctrl = (struct saudio_dev_ctrl *)&saudio->dev_ctrl[dev];
	stream = (struct saudio_stream *)&dev_ctrl->stream[stream_id];
	stream->substream = substream;
	stream->stream_id = stream_id;

	stream->period = 0;
	stream->periods_tosend = 0;
	stream->periods_avail = 0;
	stream->hwptr_done = 0;
	stream->last_getblk_count = 0;
	stream->last_elapsed_count = 0;
	stream->blk_count = SAUDIO_STREAM_BLOCK_COUNT;

	if (stream_id == SNDRV_PCM_STREAM_PLAYBACK)
		runtime->hw = snd_card_saudio_playback;
	else
		runtime->hw = snd_card_saudio_capture;
	mutex_lock(&dev_ctrl->mutex);
	result = saudio_send_common_cmd(dev_ctrl->dst, dev_ctrl->channel,
					SAUDIO_CMD_OPEN, stream_id, dev,
					CMD_TIMEOUT);
	if (result) {
		ETRACE
		    ("saudio.c: %s: saudio_send_common_cmd result is %d",
		    __func__,
		     result);
		if (result != (-ERESTARTSYS))
			saudio_snd_card_free(saudio);
		mutex_unlock(&dev_ctrl->mutex);
		return result;
	}
	pr_info("%s send cmd done\n", __func__);
	result = saudio_wait_common_cmd(dev_ctrl->dst,
					dev_ctrl->channel,
					SAUDIO_CMD_OPEN_RET, 0, CMD_TIMEOUT);
	if (result && (result != (-ERESTARTSYS)))
		saudio_snd_card_free(saudio);
	mutex_unlock(&dev_ctrl->mutex);
	pr_info("%s OUT, result=%d\n", __func__, result);

	return result;
}

static int snd_card_saudio_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	int result = 0;

	ADEBUG();
	mutex_lock(&saudio->mutex);
	if (!saudio->state) {
		mutex_unlock(&saudio->mutex);
		pr_err("saudio.c: snd_pcm_close error saudio state %d\n",
		       saudio->state);
		return -EIO;
	}
	mutex_unlock(&saudio->mutex);
	dev_ctrl = (struct saudio_dev_ctrl *)&saudio->dev_ctrl[dev];
	pr_info("%s IN, stream_id=%d,dst %d, channel %d\n", __func__, stream_id,
		dev_ctrl->dst, dev_ctrl->channel);
	mutex_lock(&dev_ctrl->mutex);
	result = saudio_send_common_cmd(dev_ctrl->dst, dev_ctrl->channel,
					SAUDIO_CMD_CLOSE, stream_id, dev,
					CMD_TIMEOUT);
	if (result) {
		ETRACE
		    ("saudio.c: %s: saudio_send_common_cmd result is %d",
		    __func__,
		     result);
		if (result != (-ERESTARTSYS))
			saudio_snd_card_free(saudio);
		mutex_unlock(&dev_ctrl->mutex);
		return result;
	}
	pr_info("%s send cmd done\n", __func__);
	result =
	    saudio_wait_common_cmd(dev_ctrl->dst,
				   dev_ctrl->channel,
				   SAUDIO_CMD_CLOSE_RET, 0, CMD_TIMEOUT);
	if (result && (result != (-ERESTARTSYS)))
		saudio_snd_card_free(saudio);
	mutex_unlock(&dev_ctrl->mutex);
	pr_info("%s OUT, result=%d,dst %d, channel %d\n", __func__, result,
		dev_ctrl->dst, dev_ctrl->channel);

	return result;
}

static int saudio_data_trigger_process(struct saudio_stream *stream,
				       struct saudio_msg *msg);

static int snd_card_saudio_pcm_trigger(struct snd_pcm_substream *substream,
				       int cmd)
{
	const struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	struct saudio_stream *stream = NULL;
	struct saudio_msg msg = { 0 };
	int err = 0;
	int result = 0;

	ADEBUG();
	dev_ctrl = (struct saudio_dev_ctrl *)&saudio->dev_ctrl[dev];
	stream = (struct saudio_stream *)&dev_ctrl->stream[stream_id];

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		pr_info("%s IN, TRIGGER_START, stream_id=%d\n", __func__,
			stream_id);
		msg.stream_id = stream_id;
		stream->stream_state = SAUDIO_TRIGGERED;
		result = saudio_data_trigger_process(stream, &msg);
		result =
		    saudio_send_common_cmd(dev_ctrl->dst, dev_ctrl->channel,
					   SAUDIO_CMD_START, stream_id, dev,
					   0);
		if (result) {
			ETRACE
			    ("saudio.c: %s: RESUME, send_common_cmd result is %d",
			    __func__,
			     result);
			if (result != (-ERESTARTSYS))
				saudio_snd_card_free(saudio);
			return result;
		}
		pr_info("%s OUT, TRIGGER_START, result=%d\n", __func__, result);

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		pr_info("%s IN, TRIGGER_STOP, stream_id=%d\n", __func__,
			stream_id);
		stream->stream_state = SAUDIO_STOPPED;
		result =
		    saudio_send_common_cmd(dev_ctrl->dst, dev_ctrl->channel,
					   SAUDIO_CMD_STOP, stream_id, dev,
					   0);
		if (result) {
			ETRACE
			    ("saudio.c: %s: SUSPEND, send_common_cmd result is %d",
			    __func__,
			     result);
			if (result != (-ERESTARTSYS))
				saudio_snd_card_free(saudio);
			return result;
		}
		pr_info("%s OUT, TRIGGER_STOP, result=%d\n", __func__, result);

		break;
	default:
		err = -EINVAL;
		break;
	}

	return 0;
}

static int saudio_cmd_prepare_process(struct saudio_dev_ctrl *dev_ctrl,
				      struct saudio_msg *msg);
static int snd_card_saudio_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	struct saudio_msg msg = { 0 };
	int result = 0;

	ADEBUG();
	mutex_lock(&saudio->mutex);
	if (!saudio->state) {
		mutex_unlock(&saudio->mutex);
		pr_err("saudio.c: snd_pcm_prepare error saudio state %d\n",
		       saudio->state);
		return -EIO;
	}
	mutex_unlock(&saudio->mutex);
	pr_info("%s IN, stream_id=%d\n", __func__, stream_id);
	dev_ctrl = (struct saudio_dev_ctrl *)&saudio->dev_ctrl[dev];
	msg.command = SAUDIO_CMD_PREPARE;
	msg.stream_id = stream_id;

	result = saudio_cmd_prepare_process(dev_ctrl, &msg);
	pr_info("%s OUT, result=%d\n", __func__, result);

	return 0;
}

static int snd_card_saudio_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	s32 result = 0;

	ADEBUG();
	result =
	    saudio_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
	pr_debug("saudio.c: saudio.c: hw_params result is %d", result);

	return result;
}

static int snd_card_saudio_hw_free(struct snd_pcm_substream *substream)
{
	int ret = 0;
	const struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct saudio_stream *stream =
	    (struct saudio_stream *)&saudio->dev_ctrl[dev].stream[stream_id];
	ADEBUG();
	mutex_lock(&stream->mutex);
	ret = saudio_pcm_lib_free_pages(substream);
	mutex_unlock(&stream->mutex);

	return ret;
}

static snd_pcm_uframes_t snd_card_saudio_pcm_pointer(struct snd_pcm_substream
						     *substream)
{
	const struct snd_saudio *saudio = snd_pcm_substream_chip(substream);
	const int stream_id = substream->pstr->stream;
	const int dev = substream->pcm->device;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct saudio_stream *stream =
	    (struct saudio_stream *)&saudio->dev_ctrl[dev].stream[stream_id];
	unsigned int offset;

	offset =
	    stream->hwptr_done * frames_to_bytes(runtime, runtime->period_size);

	return bytes_to_frames(runtime, offset);
}

static struct snd_pcm_ops snd_card_saudio_playback_ops = {
	.open = snd_card_saudio_pcm_open,
	.close = snd_card_saudio_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_card_saudio_hw_params,
	.hw_free = snd_card_saudio_hw_free,
	.prepare = snd_card_saudio_pcm_prepare,
	.trigger = snd_card_saudio_pcm_trigger,
	.pointer = snd_card_saudio_pcm_pointer,
	.mmap = saudio_snd_mmap,
};

static struct snd_pcm_ops snd_card_saudio_capture_ops = {
	.open = snd_card_saudio_pcm_open,
	.close = snd_card_saudio_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_card_saudio_hw_params,
	.hw_free = snd_card_saudio_hw_free,
	.prepare = snd_card_saudio_pcm_prepare,
	.trigger = snd_card_saudio_pcm_trigger,
	.pointer = snd_card_saudio_pcm_pointer,
	.mmap = saudio_snd_mmap,
};

void saudio_pcm_lib_preallocate_free_for_all(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	int stream;

	pr_debug("saudio.c:%s", __func__);
	for (stream = 0; stream < 2; stream++)
		for (substream = pcm->streams[stream].substream; substream;
		     substream = substream->next) {
			if (substream->dma_buffer.addr) {
				iounmap(substream->dma_buffer.area);
				smem_free(substream->dma_buffer.addr,
					  substream->dma_buffer.bytes);
				substream->dma_buffer.addr = (dma_addr_t)NULL;
				substream->dma_buffer.area = (int8_t *)NULL;
				substream->dma_buffer.bytes = 0;
			}
		}
}

int saudio_pcm_lib_preallocate_pages_for_all(struct snd_pcm *pcm,
					     int type, void *data,
					     size_t size, size_t max)
{
	struct snd_pcm_substream *substream;
	int stream;

	pr_debug("saudio.c:%s in", __func__);
	(void)size;
	for (stream = 0; stream < SAUDIO_STREAM_MAX; stream++) {
		for (substream = pcm->streams[stream].substream; substream;
		     substream = substream->next) {
			struct snd_dma_buffer *dmab = &substream->dma_buffer;
			u32 addr = smem_alloc(size);

			if (addr) {
				dmab->dev.type = type;
				dmab->dev.dev = data;
				dmab->area = shmem_ram_vmap_nocache(addr, size);
				dmab->addr = addr;
				dmab->bytes = size;
				memset(dmab->area, 0x5a, size);
				pr_debug("%s:saudio.c: dmab addr is %x, area is %lx,size is %d",
					 __func__,
					 (uint32_t)dmab->addr,
					(unsigned long)dmab->area,
					(int)size);
				if (substream->dma_buffer.bytes > 0)
					substream->buffer_bytes_max =
						substream->dma_buffer.bytes;
				substream->dma_max = max;
			} else {
				pr_err("saudio: prealloc smem error\n");
				memset(dmab, 0, sizeof(struct snd_dma_buffer));
				substream->dma_max = 0;
				substream->buffer_bytes_max = 0;
				goto ERR;
			}
		}
	}

	return 0;
ERR:
	saudio_pcm_lib_preallocate_free_for_all(pcm);

	return -1;
}

static int  snd_card_saudio_pcm(struct snd_saudio *saudio, int device,
				int substreams)
{
	struct snd_pcm *pcm;
	int err;
	int capture_count = 1;

	ADEBUG();
	/* Note: pcm1 only need support playback */
	if (device == 1)
		capture_count = 0;
	err = snd_pcm_new(saudio->card, "SAUDIO PCM", device,
			  substreams, capture_count, &pcm);
	if (err < 0)
		return err;
	pcm->private_data = saudio;
	saudio->pcm[device] = pcm;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_card_saudio_playback_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE,
			&snd_card_saudio_capture_ops);
	pcm->private_data = saudio;
	pcm->info_flags = 0;
	strcpy(pcm->name, "SAUDIO PCM");
	pcm->private_free = saudio_pcm_lib_preallocate_free_for_all;
	err = saudio_pcm_lib_preallocate_pages_for_all(
		pcm, SNDRV_DMA_TYPE_CONTINUOUS,
		snd_dma_continuous_data(GFP_KERNEL),
		MAX_BUFFER_SIZE, MAX_BUFFER_SIZE);
	if (err) {
		pr_err("saudio:%s: prealloc pages error\n", __func__);
		saudio->pcm[device] = NULL;
	}

	return err;
}

static struct snd_saudio *saudio_card_probe(struct saudio_init_data *init_data)
{
	struct snd_saudio *saudio = NULL;
	int i;

	saudio = kzalloc(sizeof(*saudio), GFP_KERNEL);
	if (!saudio)
		return NULL;
	saudio->device_num = init_data->device_num;
	saudio->ap_addr_to_cp = init_data->ap_addr_to_cp;
	for (i = 0; i < init_data->device_num; i++) {
		saudio->dev_ctrl[i].channel = init_data->ctrl_channel;
		saudio->dev_ctrl[i].monitor_channel =
			init_data->monitor_channel;
		saudio->dev_ctrl[i].dst = init_data->dst;
		saudio->dst = init_data->dst;
		saudio->channel = init_data->ctrl_channel;
		mutex_init(&saudio->mutex);
		memcpy(saudio->dev_ctrl[i].name, init_data->name,
		       SAUDIO_CARD_NAME_LEN_MAX);
		saudio->dev_ctrl[i].stream[SNDRV_PCM_STREAM_PLAYBACK].channel =
			init_data->playback_channel[i];
		saudio->dev_ctrl[i].stream[SNDRV_PCM_STREAM_PLAYBACK].dst =
			init_data->dst;
		saudio->dev_ctrl[i].stream[SNDRV_PCM_STREAM_CAPTURE].channel =
			init_data->capture_channel;
		saudio->dev_ctrl[i].stream[SNDRV_PCM_STREAM_CAPTURE].dst =
			init_data->dst;
	}
	return saudio;
}

static int saudio_data_trigger_process(struct saudio_stream *stream,
				       struct saudio_msg *msg)
{
	s32 result = 0;
	struct sblock blk = { 0 };
	struct cmd_common *common = NULL;
	struct snd_pcm_runtime *runtime = stream->substream->runtime;

	ADEBUG();

	if (stream->stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
		stream->periods_avail = snd_pcm_playback_avail(runtime) /
		    runtime->period_size;
	} else {
		stream->periods_avail = snd_pcm_capture_avail(runtime) /
		    runtime->period_size;
	}

	pr_debug("saudio.c:stream->periods_avail is %d,block count is %d",
		 stream->periods_avail, sblock_get_free_count(stream->dst,
							      stream->channel));

	stream->periods_tosend = runtime->periods - stream->periods_avail;

	ADEBUG();

	while (stream->periods_tosend) {
		result = sblock_get(stream->dst, stream->channel, &blk, 0);
		if (result)
			break;
		stream->last_getblk_count++;
		common = (struct cmd_common *)blk.addr;
		blk.length = frames_to_bytes(runtime, runtime->period_size);
		common->command = SAUDIO_DATA_PCM;
		common->sub_cmd = stream->stream_id;
		common->reserved1 =
		    stream->substream->dma_buffer.addr +
		    stream->period * blk.length +
		    stream->saudio->ap_addr_to_cp;

		sblock_send(stream->dst, stream->channel, &blk);

		stream->period++;
		stream->period = stream->period % runtime->periods;
		stream->periods_tosend--;
	}

	pr_debug(":sblock_getblock_count trigger is %d\n",
		 stream->last_getblk_count);

	return result;
}

static int saudio_data_transfer_process(struct saudio_stream *stream,
					struct saudio_msg *msg)
{
	struct snd_pcm_runtime *runtime = stream->substream->runtime;
	struct sblock blk = { 0 };
	s32 result = 0;
	struct cmd_common *common = NULL;

	s32 elapsed_blks = 0;
	s32 periods_avail;
	s32 periods_tosend;
	s32 cur_blk_count = 0;

	cur_blk_count = sblock_get_free_count(stream->dst, stream->channel);

	elapsed_blks =
	    (cur_blk_count + stream->last_getblk_count - stream->blk_count) -
	    stream->last_elapsed_count;

	if (stream->stream_id == SNDRV_PCM_STREAM_PLAYBACK) {
		periods_avail = snd_pcm_playback_avail(runtime) /
		    runtime->period_size;
	} else {
		periods_avail = snd_pcm_capture_avail(runtime) /
		    runtime->period_size;
	}

	periods_tosend = stream->periods_avail - periods_avail;
	if (periods_tosend > 0)
		stream->periods_tosend += periods_tosend;

	if (stream->periods_tosend) {
		while (stream->periods_tosend) {
			result =
			    sblock_get(stream->dst, stream->channel, &blk, 0);
			if (result)
				break;
			stream->last_getblk_count++;
			common = (struct cmd_common *)blk.addr;
			blk.length =
			    frames_to_bytes(runtime, runtime->period_size);
			common->command = SAUDIO_DATA_PCM;
			common->sub_cmd = stream->stream_id;
			common->reserved1 =
			    stream->substream->dma_buffer.addr +
			    stream->period * blk.length +
			    stream->saudio->ap_addr_to_cp;

			sblock_send(stream->dst, stream->channel, &blk);

			stream->periods_tosend--;
			stream->period++;
			stream->period = stream->period % runtime->periods;
		}

	} else {
		pr_debug("saudio.c: saudio no data to send ");
		if (sblock_get_free_count(stream->dst, stream->channel) ==
		    SAUDIO_STREAM_BLOCK_COUNT) {
			pr_debug
			    ("saudio.c: saudio no data to send and  is empty ");
			result =
			    sblock_get(stream->dst, stream->channel, &blk, 0);
			if (result) {
				ETRACE("saudio.c: no data and no blk\n");
			} else {
				stream->last_getblk_count++;
				common = (struct cmd_common *)blk.addr;
				common->command = SAUDIO_DATA_SILENCE;
				common->sub_cmd = stream->stream_id;

				sblock_send(stream->dst, stream->channel, &blk);
				stream->last_elapsed_count++;
				schedule_timeout_interruptible(
					msecs_to_jiffies(5));
			}
		}
	}

	while (elapsed_blks > 0) {
		elapsed_blks--;
		stream->hwptr_done++;
		stream->hwptr_done %= runtime->periods;
		snd_pcm_period_elapsed(stream->substream);
		stream->periods_avail++;
		stream->last_elapsed_count++;
	}

	return 0;
}

static int saudio_cmd_prepare_process(struct saudio_dev_ctrl *dev_ctrl,
				      struct saudio_msg *msg)
{
	struct sblock blk;
	struct snd_saudio *saudio = NULL;
	s32 result = 0;
	int dev = 0;
	struct snd_pcm_runtime *runtime =
	    dev_ctrl->stream[msg->stream_id].substream->runtime;
	ADEBUG();
	dev = dev_ctrl->stream[msg->stream_id].substream->pcm->device;
	saudio = dev_ctrl->stream[msg->stream_id].saudio;
	result =
	    sblock_get(dev_ctrl->dst, dev_ctrl->channel, (struct sblock *)&blk,
		       CMD_TIMEOUT);
	if (!result) {
		struct cmd_prepare *prepare = (struct cmd_prepare *)blk.addr;

		prepare->common.command = SAUDIO_CMD_PREPARE;
		prepare->common.sub_cmd = msg->stream_id;
		prepare->common.reserved2 = dev;
		prepare->rate = runtime->rate;
		prepare->channels = runtime->channels;
		prepare->format = runtime->format;
		prepare->period =
		    frames_to_bytes(runtime, runtime->period_size);
		prepare->periods = runtime->periods;
		blk.length = sizeof(struct cmd_prepare);

		mutex_lock(&dev_ctrl->mutex);

		sblock_send(dev_ctrl->dst, dev_ctrl->channel,
			    (struct sblock *)&blk);
		result =
		    saudio_wait_common_cmd(dev_ctrl->dst, dev_ctrl->channel,
					   SAUDIO_CMD_PREPARE_RET,
					   0, CMD_TIMEOUT);
		if (result && (result != (-ERESTARTSYS)))
			saudio_snd_card_free(saudio);

		mutex_unlock(&dev_ctrl->mutex);
	}
	pr_debug("%s result is %d", __func__, result);

	return result;
}

static void sblock_notifier(int event, void *data)
{
	struct saudio_stream *stream = data;
	struct saudio_msg msg = { 0 };
	int result = 0;

	if (event == SBLOCK_NOTIFY_GET) {
		if (stream->stream_state == SAUDIO_TRIGGERED) {
			mutex_lock(&stream->mutex);
			result = saudio_data_transfer_process(stream, &msg);
			mutex_unlock(&stream->mutex);
		} else {
			pr_debug("\n: saudio is stopped\n");
		}
	}
}

static int saudio_snd_card_free(const struct snd_saudio *saudio)
{
	int result = 0;

	pr_info("saudio:saudio_snd_card free in dst %d,channel %d\n",
		saudio->dst, saudio->channel);
	queue_work(saudio->queue,
		   (struct work_struct *)&saudio->card_free_work);
	pr_info("saudio:saudio_snd_card free out %d ,dst %d, channel %d\n",
		result, saudio->dst, saudio->channel);

	return 0;
}

static void saudio_snd_wait_modem_restart(struct snd_saudio *saudio)
{
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	int result = 0;

	dev_ctrl = &saudio->dev_ctrl[0];
	while (1) {
		result =
		    saudio_wait_common_cmd(dev_ctrl->dst,
					   dev_ctrl->monitor_channel,
					   SAUDIO_CMD_HANDSHAKE, 0, -1);
		if (result) {
			schedule_timeout_interruptible(msecs_to_jiffies(1000));
			pr_err("saudio_wait_monitor_cmd error %d\n", result);
			continue;
		} else {
			while (1) {
				result =
					saudio_send_common_cmd(dev_ctrl->dst,
							       dev_ctrl->monitor_channel,
						SAUDIO_CMD_HANDSHAKE_RET,
						0, 0, -1);
				if (!result)
					break;
				pr_err("saudio_send_monitor_cmd error %d\n",
				       result);
				schedule_timeout_interruptible(
					msecs_to_jiffies(1000));
			}
		}
		break;
	}
}

static int saudio_snd_notify_modem_clear(struct snd_saudio *saudio)
{
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	int result = 0;

	dev_ctrl = &saudio->dev_ctrl[0];
	pr_info("saudio.c:saudio_snd_notify_mdem_clear in");
	result =
		saudio_send_common_cmd(dev_ctrl->dst,
				       dev_ctrl->monitor_channel,
				   SAUDIO_CMD_RESET, 0, 0, -1);
	if (result) {
		pr_err("saudio_send_modem_reset_cmd error %d\n", result);
	} else {
		result =
			saudio_wait_common_cmd(dev_ctrl->dst,
					       dev_ctrl->monitor_channel,
			SAUDIO_CMD_RESET_RET, 0, CMD_MODEM_RESET_TIMEOUT);
		if (result)
			pr_err("saudio_wait_monitor_cmd error %d\n", result);
	}
	pr_info("saudio.c:saudio_snd_notify_mdem_clear out");

	return result;
}

static int saudio_snd_init_ipc(struct snd_saudio *saudio)
{
	int result = 0;
	s32 i = 0, j = 0;
	struct saudio_stream *stream = NULL;
	struct saudio_dev_ctrl *dev_ctrl = NULL;

	ADEBUG();

	dev_ctrl = &saudio->dev_ctrl[0];
	result =
	    sblock_create(dev_ctrl->dst, dev_ctrl->monitor_channel,
			  SAUDIO_MONITOR_BLOCK_COUNT, CMD_BLOCK_SIZE,
			  SAUDIO_MONITOR_BLOCK_COUNT, CMD_BLOCK_SIZE);
	if (result) {
		ETRACE
		    ("saudio:monitor channel create failed result is %d\n",
		     result);
		goto nodev;
	}
	result =
	    sblock_create(dev_ctrl->dst, dev_ctrl->channel,
			  SAUDIO_CMD_BLOCK_COUNT, CMD_BLOCK_SIZE,
			  SAUDIO_CMD_BLOCK_COUNT, CMD_BLOCK_SIZE);
	if (result) {
		ETRACE
		    ("saudio_thread sblock create failed result is %d\n",
		     result);
		goto nodev;
	}
	pr_debug("saudio_thread sblock create result is %d\n", result);

	for (i = 0; i < saudio->device_num; i++) {
		dev_ctrl = &saudio->dev_ctrl[i];
		for (j = 0; j < SAUDIO_STREAM_MAX; j++) {
			stream = &dev_ctrl->stream[j];
			/* Note: pcm 1 don't need register channel */
			if (i == 1 && j == 1) {
				result = 0;
				pr_err("saudio:pcm 1 don't need register channel");
				goto nodev;
			}
			result =
			    sblock_create(stream->dst, stream->channel,
					  SAUDIO_STREAM_BLOCK_COUNT,
					  TX_DATA_BLOCK_SIZE,
					  SAUDIO_STREAM_BLOCK_COUNT,
					  RX_DATA_BLOCK_SIZE);
			if (result) {
				ETRACE
				    ("saudio_thread sblock create failed result is %d\n",
				    result);
				goto nodev;
			}
			sblock_register_notifier(stream->dst, stream->channel,
						 sblock_notifier, stream);
			pr_debug("saudio_thread sblock create  result is %d\n",
				 result);
		}
	}
	ADEBUG();

	return result;
nodev:
	ETRACE("initialization failed\n");

	return result;
}

static int saudio_snd_init_card(struct snd_saudio *saudio)
{
	int result = 0;
	s32 i = 0, j = 0, err = 0;
	struct saudio_stream *stream = NULL;
	struct saudio_dev_ctrl *dev_ctrl = NULL;
	struct snd_card *saudio_card = NULL;

	ADEBUG();

	if (!saudio)
		return -1;

	mutex_lock(&snd_sound);

	result = snd_card_new(&saudio->pdev->dev, SNDRV_DEFAULT_IDX1,
			      saudio->dev_ctrl[0].name, THIS_MODULE,
		sizeof(struct snd_saudio *), &saudio_card);
	if (!saudio_card) {
		pr_err("saudio:snd_card_create failed result is %d\n", result);
		mutex_unlock(&snd_sound);
		return -1;
	}
	saudio->card = saudio_card;
	saudio_card->private_data = saudio;

	for (i = 0; i < saudio->device_num; i++) {
		dev_ctrl = &saudio->dev_ctrl[i];
		mutex_init(&dev_ctrl->mutex);
		err = snd_card_saudio_pcm(saudio, i, 1);
		if (err < 0) {
			mutex_unlock(&snd_sound);
			goto nodev;
		}
		for (j = 0; j < SAUDIO_STREAM_MAX; j++) {
			stream = &dev_ctrl->stream[j];
			stream->dev_ctrl = dev_ctrl;

			stream->stream_state = SAUDIO_IDLE;
			stream->stream_id = j;
			stream->saudio = saudio;
			mutex_init(&stream->mutex);
		}
	}
	ADEBUG();

	memcpy(saudio->card->driver, dev_ctrl->name, SAUDIO_CARD_NAME_LEN_MAX);
	memcpy(saudio->card->shortname, dev_ctrl->name,
	       SAUDIO_CARD_NAME_LEN_MAX);
	memcpy(saudio->card->longname, dev_ctrl->name,
	       SAUDIO_CARD_NAME_LEN_MAX);

	for (i = 0; i < ARRAY_SIZE(kctrl_array); i++) {
		struct snd_kcontrol *kctl;

		kctl = snd_ctl_new1(kctrl_array[i], saudio);
		err = snd_ctl_add(saudio->card, kctl);
		pr_info("saudio.c:kcontrol create entry!saudio:%p, kctl:%p, card:%p, add_err:%d\n",
			saudio, kctl, saudio->card, err);
	}

	err = snd_card_register(saudio->card);

	mutex_unlock(&snd_sound);

	if (err == 0) {
		pr_info("saudio.c:snd_card create ok\n");
		return 0;
	}
nodev:
	if (saudio) {
		if (saudio->card) {
			mutex_lock(&snd_sound);
			snd_card_free(saudio->card);
			saudio->card = NULL;
			mutex_unlock(&snd_sound);
		}
	}
	pr_err("saudio.c:initialization failed\n");

	return err;
}

static int saudio_ctrl_thread(void *data)
{
	int result = 0;
	struct snd_saudio *saudio = (struct snd_saudio *)data;

	ADEBUG();

	result = saudio_snd_init_ipc(saudio);
	if (result) {
		pr_err("saudio:saudio_snd_init_ipc error %d\n", result);
		return -1;
	}
	while (!kthread_should_stop()) {
		pr_info(
			"%s,saudio: waiting for modem boot handshake,dst %d,channel %d\n",
			__func__, saudio->dst, saudio->channel);
		saudio_snd_wait_modem_restart(saudio);

		saudio->in_init = 1;

		pr_info(
			"%s,saudio: modem boot and handshake ok,dst %d, channel %d\n",
			__func__, saudio->dst, saudio->channel);
		saudio_snd_card_free(saudio);
		pr_info(
			"%s flush work queue in dst %d, channel %d\n",
			__func__,
			saudio->dst, saudio->channel);

		flush_workqueue(saudio->queue);

		saudio->in_init = 0;

		pr_info(
			"%s flush work queue out,dst %d, channel %d\n",
			__func__,
			saudio->dst, saudio->channel);

		if (saudio_snd_notify_modem_clear(saudio)) {
			pr_err(" %s modem error again when notify modem clear\n", __func__);
			continue;
		}

		saudio_clear_ctrl_cmd(saudio);

		if (!saudio->card) {
			result = saudio_snd_init_card(saudio);
			pr_info(
				"saudio: snd card init reulst %d, dst %d, channel %d\n",
				result, saudio->dst, saudio->channel);
		}
		mutex_lock(&saudio->mutex);
		saudio->state = 1;
		mutex_unlock(&saudio->mutex);
	}
	ETRACE("%s  create  ok\n", __func__);

	return 0;
}

static void saudio_work_card_free_handler(struct work_struct *data)
{
	struct snd_saudio *saudio =
	    container_of(data, struct snd_saudio, card_free_work);
	pr_info("saudio: card free handler in\n");

	if (saudio->card) {
		int res = 0;

		pr_info(
			"saudio: work_handler:snd card free in,dst %d, channel %d\n",
			saudio->dst, saudio->channel);
		mutex_lock(&saudio->mutex);
		saudio->state = 0;
		mutex_unlock(&saudio->mutex);
		if (!saudio->in_init)
			res = saudio_send_common_cmd(
				saudio->dst, saudio->channel, 0,
				SAUDIO_CMD_HANDSHAKE, 0, -1);
		pr_info(
			"saudio: work_handler:snd card free reulst %d,dst %d, channel %d\n",
			res, saudio->dst, saudio->channel);
	}
	pr_info("saudio: card free handler out\n");
}

static int snd_saudio_probe(struct platform_device *devptr)
{
	struct snd_saudio *saudio = NULL;
#ifdef CONFIG_OF
	int ret, id, ctrl_ch;
	int playback_ch[SAUDIO_DEV_MAX], capture_ch, monitor_ch;
	const char *name = NULL;
	struct saudio_init_data snd_init_data = {0};
	const char *saudio_names = "sprd,saudio-names";
	const char *saudio_dst_id = "sprd,saudio-dst-id";
	const char *ctrl_channel = "sprd,ctrl_channel";
	const char *playback_channel = "sprd,playback_channel";
	const char *capture_channel = "sprd,capture_channel";
	const char *monitor_channel = "sprd,monitor_channel";
	const char *ap_addr_to_cp = "sprd,ap_addr_to_cp";
	int ap_addr_offset = 0;

	int device_num, i;
	const char *device_number = "sprd,device";

	struct saudio_init_data *init_data = &snd_init_data;
#else
	struct saudio_init_data *init_data = devptr->dev.platform_data;
#endif
	struct device_node *np = devptr->dev.of_node;

	ADEBUG();

#ifdef CONFIG_OF

	ret = of_property_read_u32(np, device_number, &device_num);
	if (ret) {
		pr_warn("saudio: missing %s in dt node\n", device_number);
		device_num = 1;
	}

	ret = of_property_read_u32(np, saudio_dst_id, &id);
	if (ret) {
		pr_err("saudio: missing %s in dt node\n", saudio_dst_id);
		return ret;
	}
	ret = of_property_read_u32(np, ctrl_channel, &ctrl_ch);
	if (ret) {
		pr_err("saudio: missing %s in dt node\n", ctrl_channel);
		return ret;
	}
	ret = of_property_read_u32(np, monitor_channel, &monitor_ch);
	if (ret) {
		pr_err("saudio: missing %s in dt node\n", monitor_channel);
		return ret;
	}
	ret = of_property_read_string(np, saudio_names, &name);
	if (ret) {
		pr_err("saudio: missing %s in dt node\n", saudio_names);
		return ret;
	}
	ret = of_property_read_u32(np, capture_channel, &capture_ch);
	if (ret) {
		pr_err("saudio: %s: missing %s in dt node\n",
		       __func__, monitor_channel);
		return ret;
	}
	ret = of_property_read_u32(np, ap_addr_to_cp, &ap_addr_offset);
	if (ret) {
		pr_err("saudio: %s: no %s in dt node\n",
		       __func__, ap_addr_to_cp);
		ap_addr_offset = 0;
	}
	pr_err("saudio: %s: %x in dt node\n",
	       __func__, ap_addr_offset);
	if (device_num == 2) {
		ret = of_property_read_u32_array(np, playback_channel,
						 &playback_ch[0], 2);
		if (ret) {
			pr_err("saudio: missing %s in dt node\n",
			       playback_channel);
			return ret;
		}
	} else if (device_num == 1) {
		ret = of_property_read_u32(np, playback_channel,
					   &playback_ch[0]);
		if (ret) {
			pr_err("saudio: missing %s in dt node\n",
			       playback_channel);
			return ret;
		}
	}
	init_data->name = (char *)name;
	init_data->dst = id;
	init_data->ctrl_channel = ctrl_ch;
	init_data->monitor_channel = monitor_ch;
	init_data->device_num = device_num;
	init_data->capture_channel = capture_ch;
	init_data->ap_addr_to_cp = ap_addr_offset;

	for (i = 0; i < device_num; i++)
		init_data->playback_channel[i] = playback_ch[i];
#endif

	saudio = saudio_card_probe(init_data);
	if (!saudio)
		return -1;
	saudio->pdev = devptr;

	saudio->queue = create_singlethread_workqueue("saudio");
	if (!saudio->queue) {
		pr_err("saudio:workqueue create error %p\n",
		       saudio->queue);
		kfree(saudio);
		saudio = NULL;
		return -1;
	}
	pr_info("saudio:workqueue create ok");
	INIT_WORK(&saudio->card_free_work, saudio_work_card_free_handler);

	platform_set_drvdata(devptr, saudio);

	saudio->thread_id = kthread_create(
			    saudio_ctrl_thread, saudio,
			    "saudio-%d-%d", saudio->dst, saudio->channel);
	if (IS_ERR(saudio->thread_id)) {
		ETRACE("virtual audio cmd kernel thread creation failure\n");
		destroy_workqueue(saudio->queue);
		saudio->queue = NULL;
		saudio->thread_id = NULL;
		kfree(saudio);
		platform_set_drvdata(devptr, NULL);
		return -1;
	}
	wake_up_process(saudio->thread_id);

	return 0;
}

static int snd_saudio_remove(struct platform_device *devptr)
{
	struct snd_saudio *saudio = platform_get_drvdata(devptr);

	if (saudio) {
		if (!IS_ERR_OR_NULL(saudio->thread_id)) {
			kthread_stop(saudio->thread_id);
			saudio->thread_id = NULL;
		}
		if (saudio->queue)
			destroy_workqueue(saudio->queue);
		if (saudio->pdev)
			platform_device_unregister(saudio->pdev);
		if (saudio->card) {
			mutex_lock(&snd_sound);
			snd_card_free(saudio->card);
			saudio->card = NULL;
			mutex_unlock(&snd_sound);
		}
		kfree(saudio);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id saudio_dt_match[] = {
	{.compatible = "sprd,saudio"},
	{},
};
#endif

static struct platform_driver snd_saudio_driver = {
	.probe = snd_saudio_probe,
	.remove = snd_saudio_remove,
	.driver = {
		.name = "saudio",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = saudio_dt_match,
#endif
	},
};

static int __init alsa_card_saudio_init(void)
{
	int err;

	err = platform_driver_register(&snd_saudio_driver);
	if (err < 0)
		pr_err("saudio: platform_driver_register err %d\n", err);

	return 0;
}

static void __exit alsa_card_saudio_exit(void)
{
	platform_driver_unregister(&snd_saudio_driver);
}

static int snd_pcm_playback_control_info(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = kcontrol->count;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int snd_pcm_playback_control_route_get(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_value *ucontrol)
{
	ITRACE(" entry!!!kcontrol:%p!\n", kcontrol);
	if (kcontrol) {
		if (kcontrol->private_data) {
			struct snd_saudio *saudio = kcontrol->private_data;

			ucontrol->value.integer.value[0] =
			saudio->kcon_val_playback;
			ITRACE(" get value %ld\n",
			       ucontrol->value.integer.value[0]);
		}
	}

	return 0;
}

static int snd_pcm_playback_control_route_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	int result = 0;
	struct snd_saudio *saudio = kcontrol->private_data;

	ITRACE(" entry set:%ld, org:%d!!!\n",
	       ucontrol->value.integer.value[0], saudio->kcon_val_type);
	if (ucontrol->value.integer.value[0] !=  saudio->kcon_val_playback) {
		saudio->kcon_val_playback = ucontrol->value.integer.value[0];
		changed = 1;

		result =
			saudio_send_common_cmd_ex(saudio->dst, saudio->channel,
						  SAUDIO_CMD_SET_PLAYBACK_ROUTE,
			SAUDIO_CMD_SET_PLAYBACK_ROUTE,
			ucontrol->value.integer.value[0], 0, 0);
		if (result) {
			ETRACE("saudio.c: %s: error!send cmd result is %d!\n",
			       __func__, result);
			if (result != (-ERESTARTSYS))
				saudio_snd_card_free(saudio);
			return result;
		}
	}

	return changed;
}

static int snd_pcm_capture_control_info(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = kcontrol->count;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 2;
	return 0;
}

static int snd_pcm_capture_control_route_get(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	ITRACE(" entry!!!kcontrol:%p!\n", kcontrol);
	if (kcontrol) {
		if (kcontrol->private_data) {
			struct snd_saudio *saudio = kcontrol->private_data;

			ucontrol->value.integer.value[0] =
			saudio->kcon_val_capture;
			ITRACE(" get value %ld\n",
			       ucontrol->value.integer.value[0]);
		}
	}
	return 0;
}

static int snd_pcm_capture_control_route_put(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	int result = 0;
	struct snd_saudio *saudio = kcontrol->private_data;

	ITRACE(" entry set:%ld, org:%d!!!\n",
	       ucontrol->value.integer.value[0], saudio->kcon_val_type);
	if (ucontrol->value.integer.value[0] !=  saudio->kcon_val_capture) {
		saudio->kcon_val_capture = ucontrol->value.integer.value[0];
		changed = 1;

		result =
			saudio_send_common_cmd_ex(saudio->dst, saudio->channel,
						  SAUDIO_CMD_SET_CAPTURE_ROUTE,
			SAUDIO_CMD_SET_CAPTURE_ROUTE,
			ucontrol->value.integer.value[0], 0, 0);
		if (result) {
			ETRACE("saudio.c: %s:error!send cmd result is %d!\n",
			       __func__, result);
			if (result != (-ERESTARTSYS))
				saudio_snd_card_free(saudio);
			return result;
		}
	}
	return changed;
}

static int snd_pcm_loop_enable_control_info(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = kcontrol->count;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffff;

	return 0;
}

static int snd_pcm_loop_enable_control_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ITRACE(" entry!!!kcontrol:%p!\n", kcontrol);
	if (kcontrol) {
		if (kcontrol->private_data) {
			struct snd_saudio *saudio = kcontrol->private_data;

			ucontrol->value.integer.value[0] = saudio->kcon_val_en;
			ITRACE(" get value %ld!\n",
			       ucontrol->value.integer.value[0]);
		}
	}

	return 0;
}

static int snd_pcm_loop_enable_control_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	int result = 0;
	struct snd_saudio *saudio = kcontrol->private_data;

	ITRACE("entry set:%ld, org:%d!!!\n",
	       ucontrol->value.integer.value[0], saudio->kcon_val_type);
	if (ucontrol->value.integer.value[0] !=  saudio->kcon_val_en) {
		u32 loop_enable = 0;

		saudio->kcon_val_en = ucontrol->value.integer.value[0];
		changed = 1;
		loop_enable = saudio->kcon_val_en;

		ITRACE(" loop_enable %d!!!!\n", loop_enable);
		result =
			saudio_send_common_cmd_ex(saudio->dst, saudio->channel,
						  SAUDIO_CMD_ENABLE_LOOP, SAUDIO_CMD_ENABLE_LOOP,
				loop_enable, 0, 0);
		if (result) {
			ETRACE("yaye %s: RESUME, send cmd result is %d!\n",
			       __func__, result);
			if (result != (-ERESTARTSYS))
				saudio_snd_card_free(saudio);
			return result;
		}
	}

	return changed;
}

static int snd_pcm_loop_type_control_info(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = kcontrol->count;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffff;

	return 0;
}

static int snd_pcm_loop_type_control_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	ITRACE(" entry!!!kcontrol:%p!\n", kcontrol);
	if (kcontrol) {
		if (kcontrol->private_data) {
			struct snd_saudio *saudio = kcontrol->private_data;

			ucontrol->value.integer.value[0] =
			saudio->kcon_val_type;
			ITRACE(" get value %ld!\n",
			       ucontrol->value.integer.value[0]);
		}
	}

	return 0;
}

static int snd_pcm_loop_type_control_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	int result = 0;
	struct snd_saudio *saudio = kcontrol->private_data;

	ITRACE(" entry set:%ld, org:%d!!!\n",
	       ucontrol->value.integer.value[0], saudio->kcon_val_type);
	if (ucontrol->value.integer.value[0] !=  saudio->kcon_val_type) {
		u32 loop_type = 0;

		saudio->kcon_val_type = ucontrol->value.integer.value[0];
		changed = 1;
		loop_type = saudio->kcon_val_type;

		ITRACE(" loop_type %d!!!!\n", loop_type);
		result =
			saudio_send_common_cmd_ex(saudio->dst, saudio->channel,
						  SAUDIO_CMD_TYPE_LOOP, SAUDIO_CMD_TYPE_LOOP,
			loop_type, 0, 0);
		if (result) {
			ETRACE("yaye %s: RESUME, send cmd result is %d!\n",
			       __func__, result);
			if (result != (-ERESTARTSYS))
				saudio_snd_card_free(saudio);
			return result;
		}
	}

	return changed;
}

static int snd_pcm_mixer_route_control_info(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = kcontrol->count;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0x2;

	return 0;
}

static int snd_pcm_mixer_route_control_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ITRACE(" entry!!!kcontrol:%p!\n", kcontrol);
	if (kcontrol) {
		if (kcontrol->private_data) {
			struct snd_saudio *saudio = kcontrol->private_data;

			ucontrol->value.integer.value[0] =
			saudio->kcon_mixer_route;
			ITRACE(" get value %ld!\n",
			       ucontrol->value.integer.value[0]);
		}
	}

	return 0;
}

static int snd_pcm_mixer_route_control_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	int result = 0;
	struct snd_saudio *saudio = kcontrol->private_data;

	ITRACE(" entry set:%ld, org:%d!!!\n",
	       ucontrol->value.integer.value[0], saudio->kcon_mixer_route);
	if (ucontrol->value.integer.value[0] !=  saudio->kcon_mixer_route) {
		u32 mixer_route = 0;

		saudio->kcon_mixer_route = ucontrol->value.integer.value[0];
		changed = 1;
		mixer_route = saudio->kcon_mixer_route;

		ITRACE(" mixer_route %d!\n", mixer_route);
		result =
			saudio_send_common_cmd_ex(saudio->dst, saudio->channel,
						  SAUDIO_CMD_SET_MIXER_ROUTE, SAUDIO_CMD_SET_MIXER_ROUTE,
			mixer_route, 0, 0);
		if (result) {
			ETRACE("yaye %s: RESUME, send cmd result is %d!\n",
			       __func__, result);
			if (result != (-ERESTARTSYS))
				saudio_snd_card_free(saudio);

			return result;
		}
	}

	return changed;
}

static int snd_pcm_apptype_control_info(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = kcontrol->count;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0x10;

	return 0;
}

static int snd_pcm_apptype_control_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ITRACE(" entry!!!kcontrol:%p!\n", kcontrol);
	if (kcontrol) {
		if (kcontrol->private_data) {
			struct snd_saudio *saudio = kcontrol->private_data;

			ucontrol->value.integer.value[0] = saudio->kcon_apptype;
			ITRACE(" get value %ld!\n",
			       ucontrol->value.integer.value[0]);
		}
	}

	return 0;
}

static int snd_pcm_apptype_control_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	int result = 0;
	struct snd_saudio *saudio = kcontrol->private_data;

	ITRACE(" entry set:%ld, org:%d!!!\n",
	       ucontrol->value.integer.value[0], saudio->kcon_val_type);
	if (ucontrol->value.integer.value[0] !=  saudio->kcon_apptype) {
		u32 app_type = 0;

		saudio->kcon_apptype = ucontrol->value.integer.value[0];
		changed = 1;
		app_type = saudio->kcon_apptype;

		ITRACE(" mixer_route %d!!!!\n", app_type);
		result =
			saudio_send_common_cmd_ex(saudio->dst,
						  saudio->channel,
			SAUDIO_CMD_SET_APPTYPE, SAUDIO_CMD_SET_APPTYPE,
				app_type, 0, 0);
		if (result) {
			ETRACE("%s: RESUME, send_common_cmd result is %d!\n",
			       __func__, result);
			if (result != (-ERESTARTSYS))
				saudio_snd_card_free(saudio);
			return result;
		}
	}

	return changed;
}

static int snd_pcm_ul_gain_control_info(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = kcontrol->count;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0x4096;

	return 0;
}

static int snd_pcm_ul_gain_control_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_saudio *saudio;

	if (kcontrol && kcontrol->private_data) {
		saudio = kcontrol->private_data;
		ucontrol->value.integer.value[0] = saudio->kcon_ul_gain;
	}

	return 0;
}

static int snd_pcm_ul_gain_control_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int result;
	struct snd_saudio *saudio = kcontrol->private_data;

	saudio->kcon_ul_gain = ucontrol->value.integer.value[0];

	result =
		saudio_send_common_cmd_ex(saudio->dst, saudio->channel,
					  SAUDIO_CMD_SET_UL_GAIN, SAUDIO_CMD_SET_UL_GAIN,
			saudio->kcon_ul_gain, 0, 0);
	if (result) {
		pr_err("%s: RESUME, send_common_cmd result is %d!\n",
		       __func__, result);
		if (result != -ERESTARTSYS)
			saudio_snd_card_free(saudio);
		return result;
	}
	pr_info("ul gain %#x\n", saudio->kcon_ul_gain);

	return 0;
}

static int snd_pcm_dl_gain_control_info(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = kcontrol->count;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0x4096;

	return 0;
}

static int snd_pcm_dl_gain_control_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_saudio *saudio;

	if (kcontrol && kcontrol->private_data) {
		saudio = kcontrol->private_data;
		ucontrol->value.integer.value[0] = saudio->kcon_dl_gain;
	}

	return 0;
}

static int snd_pcm_dl_gain_control_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int result;
	struct snd_saudio *saudio = kcontrol->private_data;

	saudio->kcon_dl_gain = ucontrol->value.integer.value[0];

	result =
		saudio_send_common_cmd_ex(saudio->dst, saudio->channel,
					  SAUDIO_CMD_SET_DL_GAIN, SAUDIO_CMD_SET_DL_GAIN,
			saudio->kcon_dl_gain, 0, 0);
	if (result) {
		pr_err("%s: RESUME, send_common_cmd result is %d!\n",
		       __func__, result);
		if (result != -ERESTARTSYS)
			saudio_snd_card_free(saudio);
		return result;
	}
	pr_info("dl gain %#x\n", saudio->kcon_dl_gain);

	return 0;
}

module_init(alsa_card_saudio_init)
module_exit(alsa_card_saudio_exit)
