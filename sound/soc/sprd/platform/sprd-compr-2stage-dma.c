/*
 *  sprd-compr.c - ASoC Spreadtrum Compress Platform driver
 *
 *  Copyright (C) 2010-2020 Spreadtrum Communications Inc.
 *  Author: yintang.ren
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("COMPR")""fmt

#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dma/sprd-dma.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <sound/core.h>
#include <sound/compress_driver.h>
#include <sound/compress_offload.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <uapi/linux/sched/types.h>

#define DMA_SG  0x1


#include "audio_mem.h"
#include "mcdt_hw.h"
#include "sprd-asoc-common.h"
#include "sprd-compr.h"
#include "sprd-compr-util.c"
#include <linux/sipc.h>
#include "sprd_audio_dma.h"


/* #define CONFIG_SPRD_COMPR_CM4_WAKE_UP */
#define COMPR_DUMP_DEBUG	0
#define COMPR_DUMP_MEM_DEBUG	0

#define ADEBUG() sp_asoc_pr_info("%s, line: %d\n", __func__, __LINE__)

#define DMA_LINKLIST_CFG_NODE_SIZE (sizeof(struct sprd_dma_linklist))

/* Default values used if user space does not set */
#define COMPR_PLAYBACK_MIN_FRAGMENT_SIZE	(8 * 1024)
#define COMPR_PLAYBACK_MAX_FRAGMENT_SIZE	(128 * 1024)
#define COMPR_PLAYBACK_MIN_NUM_FRAGMENTS	(4)
#define COMPR_PLAYBACK_MAX_NUM_FRAGMENTS	(16 * 4)

#define CMD_TIMEOUT			msecs_to_jiffies(5000)
#define DATA_TIMEOUT			msecs_to_jiffies(50)
#define CMD_MODEM_RESET_TIMEOUT	msecs_to_jiffies(10000)

/*
 *******************************************************************************
 *	|<------------------------------54K----------------------------------->|
 *	|<---- For VBC---->|<------------SPRD_COMPR_DMA_TOTAL_SIZE-- ----------|
 *	|<-------SPRD_COMPR_DMA_DATA_SIZE------->|<--------1K--------->|
 *	|<---DESC--->|<-info->|
 *******************************************************************************
 */
#define SPRD_COMPR_INFO_SIZE	(sizeof(struct sprd_compr_playinfo))

#define SPRD_COMPR_DMA_DESC_SIZE		(1024 - SPRD_COMPR_INFO_SIZE)
#define SPRD_COMPR_DMA_DATA_SIZE	(iram_size_dma)

#define SPRD_COMPR_TOTAL_SIZE			(SPRD_COMPR_DMA_DATA_SIZE + \
				SPRD_COMPR_DMA_DESC_SIZE + \
				SPRD_COMPR_INFO_SIZE)

#define MCDT_CHN_COMPR	(0)
#define MCDT_EMPTY_WMK	(0)

/* only 9bits in register, could not set 512 */
#define MCDT_FULL_WMK		(512 - 1)
#define MCDT_FIFO_SIZE		(512)
#define MCDT_FIFO_SIZE_BYTE	2048

struct sprd_compr_dma_cb_data {
	struct snd_compr_stream *substream;
	struct dma_chan *dma_chn;
};

struct sprd_compr_dma_params {
	char *name;		/* stream identifier */
	int irq_type;		/* dma interrupt type */
	u32 frag_len;
	u32 block_len;
	u32 tran_len;
	/* dma description struct, 2 stages dma */
	struct sprd_dma_cfg desc;
};

struct sprd_compr_dma_rtd {
	struct dma_chan		*dma_chn;
	dma_addr_t			dma_cfg_phy;
	void				*dma_cfg_virt;
	struct dma_async_tx_descriptor *dma_tx_des;
	struct sprd_compr_dma_cb_data *dma_cb_data;
	dma_cookie_t		cookie;
	u32			buffer_paddr;
	char				*buffer;
	u32			buffer_size;
	u32			received;
	u32			copied;
};

struct sprd_compr_drain_info {
	u32 stream_id;
	u32 padding_cnt;
	u32 received_total[2];
	wait_queue_head_t stream_avail_wait;
};

struct sprd_compr_rtd {
	struct sprd_compr_dma_params params;
	struct dma_chan *dma_chn[2];
	struct sprd_compr_dma_cb_data *dma_cb_data;

	u32 iram_paddr;
	int8_t *iram_buff;
#if COMPR_DUMP_DEBUG
	struct file *log_file0;
	struct file *log_file1;
#endif
	dma_addr_t dma_cfg_phy[2];
	void *dma_cfg_virt[2];
	struct dma_async_tx_descriptor *dma_tx_des[2];
	dma_cookie_t cookie[2];
	int hw_chan;
	int cb_called;
	int dma_stage;

	struct snd_compr_stream *cstream;
	struct snd_compr_caps compr_cap;
	struct snd_compr_codec_caps codec_caps;
	struct snd_compr_params codec_param;

	u32 info_paddr;
	unsigned long info_vaddr;
	u32 info_size;

	int stream_id;
	u32 stream_state;

	u32 codec;
	u32 buffer_paddr; /* physical address */
	char *buffer;
	u32 app_pointer;
	u32 buffer_size;
	u32 total_size;

	u32 copied_total;
	u32 received_total;
	u32 iram_write_pos;
	u32 received_stage0;
	u32 received_stage1;
	u32 avail_total;

	int sample_rate;
	u32 num_channels;

	u32 drain_ready;
	u32 next_track;

	bool dma_paused;

	atomic_t start;
	atomic_t eos;
	atomic_t drain;
	atomic_t xrun;
	atomic_t pause;

	wait_queue_head_t eos_wait;
	wait_queue_head_t drain_wait;
	wait_queue_head_t flush_wait;

	struct work_struct drain_work;
#ifdef CONFIG_SPRD_COMPR_CM4_WAKE_UP
	struct task_struct *thread;
#endif

	int wake_locked;
	struct wakeup_source wake_lock;
	spinlock_t lock;
	struct mutex mutex;

#ifdef CONFIG_SND_VERBOSE_PROCFS
	struct snd_info_entry *proc_info_entry;
#endif
	bool is_access_enabled;
	struct sprd_compr_drain_info drain_info;
};

struct sprd_compr_pdata {
	bool suspend;
	wait_queue_head_t point_wait;
	struct task_struct *thread;
	struct workqueue_struct *drain_work_q;
	struct sprd_compr_dev_ctrl dev_ctrl;
	struct device	*dev;
};

static struct sprd_compr_dma_params sprd_compr_dma_param_out = {
	.name = "COMPR out",
	.irq_type = SPRD_DMA_BLK_INT,
	.frag_len = 512 * 4,
	.block_len = 512 * 4,
	.desc = {
		 .datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES,
		 .fragmens_len = 240,
		 .src_step = 4,
		 .des_step = 4,
	},
};

static int sprd_compress_new(struct snd_soc_pcm_runtime *rtd);
static int sprd_platform_compr_trigger(
		struct snd_compr_stream *cstream,
		int cmd);
static void sprd_compr_dma_buf_done(void *data);


static DEFINE_MUTEX(sprd_compr_lock);

#define SPRD_IRAM_INFO_ALL_PHYS  0xffffff

#if COMPR_DUMP_MEM_DEBUG
static int  timer_init(void);
static void  timer_exit(void);
static struct timer_list stimer;
static int g_frag_size = 0x4000;
static uint8_t *g_iram;
static uint8_t *g_buff;
#endif
#ifdef CONFIG_SPRD_COMPR_CM4_WAKE_UP
static void *compr_cb_data;
static struct mutex g_lock;
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
int agdsp_access_enable(void)
	__attribute__ ((weak, alias("__agdsp_access_enable")));
static int __agdsp_access_enable(void)
{
	pr_debug("%s\n", __func__);
	return 0;
}

int agdsp_access_disable(void)
	__attribute__ ((weak, alias("__agdsp_access_disable")));
static int __agdsp_access_disable(void)
{
	pr_debug("%s\n", __func__);
	return 0;
}
#pragma GCC diagnostic pop

static void sprd_compr_drain_work(struct work_struct *work)
{
	struct sprd_compr_rtd *srtd =
			container_of(work, struct sprd_compr_rtd, drain_work);
	int ret = 0, value = 0;

	struct snd_compr_stream *cstream = srtd->cstream;

	sp_asoc_pr_info(
		"%s: waited out, drain_ready=%d,stream_state=%d,ret=%d\n",
		__func__, srtd->drain_ready, srtd->stream_state, ret);

	ret = compr_receive_cmd(AMSG_CH_MP3_OFFLOAD_DRAIN,
			COMPR_CMD_DRAIN, &value);
	sp_asoc_pr_info("%s: drain finished, ret=%d\n",
		__func__, ret);

	if (srtd->next_track) {
		sp_asoc_pr_info("%s,before,app_pointer=%d,copied_total=%d\n",
				__func__, srtd->app_pointer,
				srtd->copied_total);

		spin_lock_irq(&srtd->lock);

		/*
		 * adjust copied_total,
		 * otherwise next track data will be out of range.
		 * If copied_total less than avail_total, that mean's some data
		 * is still left in second buffer(DDR), we could not fill next
		 * track data into this period; But copied_total maybe less
		 * than 0 sometimes, so we change total_bytes_available, they
		 * are same.
		 */
		cstream->runtime->total_bytes_available +=
			srtd->drain_info.padding_cnt;

		/*
		 * if next_track is true, that mean's we get next track data,
		 * we should count it from 0.
		 */
		srtd->received_total = 0;
		spin_unlock_irq(&srtd->lock);

		sp_asoc_pr_info("%s,after,app_pointer=%d,copied_total=%d,avail_total=%d\n",
				__func__, srtd->app_pointer,
				srtd->copied_total, srtd->avail_total);
	} else {
		dmaengine_pause(srtd->dma_chn[0]);
		memset_io((void *)srtd->info_vaddr, 0,
			  sizeof(struct sprd_compr_playinfo));
	}

	mutex_lock(&cstream->device->lock);
	snd_compr_drain_notify(cstream);
	atomic_set(&srtd->drain, 0);
	mutex_unlock(&cstream->device->lock);

}

#ifdef CONFIG_SPRD_COMPR_CM4_WAKE_UP
static int sprd_compr_monitor_thread(void *data)
{
	struct smsg mrecv;
	int rval;
	struct sched_param param = {.sched_priority = 80};

	sp_asoc_pr_info("%s entry,id=%d,ch=%d!\n",
		__func__, SIPC_ID_PM_SYS, SMSG_CH_PLAYBACK);

	rval = smsg_ch_open(SIPC_ID_PM_SYS, SMSG_CH_PLAYBACK, -1);
	if (rval) {
		pr_err("Unable to request COMPR channel:");
		pr_err("dst %d chn %d ret %d !\n",
			SIPC_ID_PM_SYS, SMSG_CH_PLAYBACK, rval);
		return 0;
	}
	sp_asoc_pr_info("%s entry2!\n", __func__);

	smsg_set(&mrecv, SMSG_CH_PLAYBACK, 0, 0, 0);
	rval = smsg_recv(SIPC_ID_PM_SYS, &mrecv, -1);
	sp_asoc_pr_info("%s entry3!\n", __func__);

	if (rval) {
		pr_err("Unable to recv sipc message! vral=%d\n", rval);
		goto exit;
	}
	sp_asoc_pr_info("%s entry4!\n", __func__);

	sched_setscheduler(current, SCHED_RR, &param);
	while (!kthread_should_stop()) {
		smsg_set(&mrecv, SMSG_CH_PLAYBACK, 0, 0, 0);
		sp_asoc_pr_info("%s before receive!\n", __func__);
		rval = smsg_recv(SIPC_ID_PM_SYS, &mrecv, -1);
		sp_asoc_pr_info("%s after receive!\n", __func__);
		if (rval == -EIO) {
			msleep(500);
			continue;
		}
		if (mrecv.type == 4) {
			mutex_lock(&g_lock);
			if (compr_cb_data)
				sprd_compr_dma_buf_done(compr_cb_data);
			mutex_unlock(&g_lock);
		}

	}

exit:
	smsg_ch_close(SIPC_ID_PM_SYS, SMSG_CH_PLAYBACK, -1);

	return 0;
}
#endif
/* yintang: to be confirmed */
int sprd_compr_configure_dsp(struct sprd_compr_rtd *srtd)
{
	return 0;
}

static void sprd_compr_populate_codec_list(struct sprd_compr_rtd *srtd)
{
	ADEBUG();

	srtd->compr_cap.direction = srtd->cstream->direction;
	srtd->compr_cap.min_fragment_size = COMPR_PLAYBACK_MIN_FRAGMENT_SIZE;
	srtd->compr_cap.max_fragment_size = COMPR_PLAYBACK_MAX_FRAGMENT_SIZE;
	srtd->compr_cap.min_fragments = COMPR_PLAYBACK_MIN_NUM_FRAGMENTS;
	srtd->compr_cap.max_fragments = COMPR_PLAYBACK_MAX_NUM_FRAGMENTS;
	srtd->compr_cap.num_codecs = 2;
	srtd->compr_cap.codecs[0] = SND_AUDIOCODEC_MP3;
	srtd->compr_cap.codecs[1] = SND_AUDIOCODEC_AAC;

	ADEBUG();
}

/*
 * proc interface
 */
static inline void sprd_compr_proc_init(struct snd_compr_stream *substream)
{
}

static void sprd_compr_proc_done(struct snd_compr_stream *substream)
{
}

/* extern u32 audio_smem_alloc(u32 size);
 * extern void audio_smem_free(u32 addr, u32 size);
 */

static int sprd_compr_config_sprd_rtd(struct sprd_compr_rtd *srtd,
				struct snd_compr_stream *substream,
				struct snd_compr_params *params)
{
	int ret = 0;
	size_t total_size = 0;
	size_t buffer_size = 0;
	size_t period = 0;
	int period_cnt = 0;
	int cfg_size = 0;
	u32 iram_size = 0;
	u32 iram_addr = 0;

	ADEBUG();

	if (!srtd) {
		sp_asoc_pr_info("%s, rtd hasn't been allocated yet\n",
								__func__);
		ret = -1;
		goto err1;
	}

	period_cnt = params->buffer.fragments;
	period = params->buffer.fragment_size;
	buffer_size = period * period_cnt;
	cfg_size = sizeof(struct sprd_dma_cfg) * (period_cnt + 1);
	total_size = buffer_size + cfg_size;
	iram_addr = audio_mem_alloc(IRAM_OFFLOAD, &iram_size);
	if (!iram_addr && (iram_size <= (4*1024)))
		goto err1;
	else
		srtd->iram_paddr = iram_addr;
	srtd->iram_buff = (int8_t *)audio_mem_vmap(iram_addr, iram_size, 1);

#if COMPR_DUMP_MEM_DEBUG
	g_iram = (uint8_t *)srtd->iram_buff;
	g_frag_size = period;
#endif
	memset_io(srtd->iram_buff, 0, iram_size);
	/* for 2stage DMA debug */
	/*__iounmap(iram_buff);*/
#if COMPR_DUMP_DEBUG
	srtd->log_file0 =
		filp_open("/data/dma0.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
	srtd->log_file1 =
		filp_open("/data/dma1.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
#endif
	/* stage 0 */
	srtd->dma_cfg_phy[0] =
		(dma_addr_t)(iram_addr + iram_size -
			SPRD_COMPR_INFO_SIZE -
			SPRD_COMPR_DMA_DESC_SIZE);
	srtd->dma_cfg_virt[0] =
		(void *)((unsigned long)srtd->iram_buff +
			(srtd->dma_cfg_phy[0] - iram_addr));

	/* stage 1 */
	srtd->buffer_paddr =
		audio_mem_alloc(DDR32, (unsigned int *)&total_size);
	if (!srtd->buffer_paddr) {
		ret = -2;
		goto err2;
	}
	srtd->total_size = total_size;
	srtd->buffer_size = buffer_size;
	srtd->buffer = audio_mem_vmap(srtd->buffer_paddr, total_size, 1);
	memset_io(srtd->buffer, 0, total_size);
#if COMPR_DUMP_MEM_DEBUG
	g_buff = (uint8_t *)srtd->buffer;
#endif
	srtd->dma_cfg_phy[1] = audio_addr_ap2dsp(DDR32, srtd->buffer_paddr +
						 buffer_size, 0);
	srtd->dma_cfg_virt[1] =
		(void *)((unsigned long)srtd->buffer + buffer_size);

	srtd->info_size = SPRD_COMPR_INFO_SIZE;
	/*srtd->info_paddr = srtd->dma_cfg_phy[0] + SPRD_COMPR_DMA_DESC_SIZE;*/
	/*srtd->info_paddr = 0xd800 - SPRD_COMPR_INFO_SIZE; */
	/*0xd800 - SPRD_COMPR_INFO_SIZE;*/
	srtd->info_paddr = srtd->dma_cfg_phy[0] + SPRD_COMPR_DMA_DESC_SIZE;

	srtd->info_vaddr =
		(unsigned long)srtd->dma_cfg_virt[0] +
		SPRD_COMPR_DMA_DESC_SIZE;

	srtd->dma_cb_data = kzalloc(
			sizeof(struct sprd_compr_dma_cb_data),
			GFP_KERNEL);
	if (!srtd->dma_cb_data) {
		ret = -3;
		goto err3;
	}
#ifdef CONFIG_SPRD_COMPR_CM4_WAKE_UP
	mutex_lock(&g_lock);
	compr_cb_data = srtd->dma_cb_data;
	mutex_unlock(&g_lock);
#endif
	srtd->dma_stage = 2;
	srtd->hw_chan = 1;
	srtd->dma_chn[0] = srtd->dma_chn[1] = NULL;
	srtd->dma_tx_des[0] = srtd->dma_tx_des[1] = NULL;
	srtd->cookie[0] = srtd->cookie[1] = 0;
	srtd->params = sprd_compr_dma_param_out;
	goto out;
err3:
	audio_mem_free(DDR32, srtd->buffer_paddr, total_size);
	audio_mem_unmap(srtd->buffer);
	srtd->buffer = 0;
	srtd->dma_cfg_virt[1] = 0;
	srtd->dma_cfg_phy[1] = 0;
err2:
	srtd->dma_cfg_virt[0] = 0;
	srtd->dma_cfg_phy[0] = 0;
	audio_mem_unmap(srtd->iram_buff);
	srtd->iram_buff = 0;
err1:
	pr_err("%s, error!!! ret=%d\n", __func__, ret);

	return ret;
out:
	sp_asoc_pr_info("%s OK!\n", __func__);

	return ret;
}
static int s_buf_done_count0;

#if COMPR_DUMP_MEM_DEBUG
static void sprd_compr_dma_buf_done0(void *data)
{
	s_buf_done_count0++;
	pr_info("%s, entry! cnt=%d\n", __func__, s_buf_done_count0);
}
#endif

int s_buf_done_count;
static void sprd_compr_dma_buf_done(void *data)
{
	struct sprd_compr_dma_cb_data *dma_cb_data =
		(struct sprd_compr_dma_cb_data *)data;
	struct snd_compr_runtime *runtime = dma_cb_data->substream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;

	pr_debug("%s in s_buf_done_count0:%d ,s_buf_done_count:%d\n",
		__func__, s_buf_done_count0, s_buf_done_count);
	if (srtd->stream_state != COMPR_TRIGGERED) {
		sp_asoc_pr_info("buff done ignored,stream_state=%d\n",
			srtd->stream_state);
		return;
	}

	if (!srtd->wake_locked) {
		__pm_stay_awake(&srtd->wake_lock);
		sp_asoc_pr_info("buff done wake_lock\n");
		srtd->wake_locked = 1;
	}
#if COMPR_DUMP_DEBUG
	mm_segment_t *old_fs = 0;

	old_fs = get_fs();
	set_fs(get_ds());
	vfs_write(srtd->log_file0, srtd->iram_buff,
				SPRD_COMPR_DMA_DATA_SIZE,
				&srtd->log_file0->f_pos);
	set_fs(old_fs);
#endif
	s_buf_done_count++;
	srtd->copied_total += srtd->params.tran_len;
	pr_info("%s, DMA copied totol=%d, avail_total=%d, buf_done_count=%d\n",
		__func__, srtd->copied_total, srtd->avail_total,
		s_buf_done_count);
	snd_compr_fragment_elapsed(dma_cb_data->substream);

}

struct dma_async_tx_descriptor *sprd_compr_config_dma_channel(
	struct dma_chan *dma_chn,
	struct sprd_dma_cfg *dma_config,
	struct sprd_compr_dma_cb_data *dma_cb_data,
	unsigned int flag)
{
	int ret = 0;
	struct dma_async_tx_descriptor *dma_tx_des = NULL;
	/* config dma channel */
	ret = dmaengine_slave_config(dma_chn, &(dma_config->config));
	if (ret < 0) {
		pr_err("%s, DMA chan ID %d config is failed!\n",
			__func__,
			dma_chn->chan_id);
		return NULL;
	}
	/* get dma desc from dma config */
	dma_tx_des =
			dma_chn->device->device_prep_slave_sg(
				dma_chn,
				dma_config->sg,
				dma_config->sg_num,
				dma_config->config.direction,
				dma_config->dma_config_flag,
				&(dma_config->ll_cfg));
	if (!dma_tx_des) {
		pr_err("%s, DMA chan ID %d memcpy is failed!\n",
						__func__,
					dma_chn->chan_id);
		return NULL;
	}

#ifndef CONFIG_SPRD_COMPR_CM4_WAKE_UP
	if (!(flag & SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP)) {
		sp_asoc_pr_info(
				"%s, Register Callback func for DMA chan ID %d\n",
				__func__, dma_chn->chan_id);
		dma_tx_des->callback = sprd_compr_dma_buf_done;
		dma_tx_des->callback_param = (void *)(dma_cb_data);
	}
#endif

	return dma_tx_des;
}

int compr_stream_config_dma1(struct snd_compr_stream *substream,
			      u32 frag_size, u32 frag_cnt)
{
	struct snd_compr_runtime *runtime = substream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sprd_compr_pdata *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);

	struct sprd_compr_dma_cb_data *dma_cb_data_ptr;
	struct scatterlist *sg = NULL;

	struct dma_chan *dma_chn_request;
	dma_cap_mask_t mask;

	size_t totsize = 0;
	size_t period = 0;
	int period_cnt = 0;

	struct sprd_dma_cfg *cfg_ptr = NULL;
	dma_addr_t dma_des_phys;
	dma_addr_t dma_src_phys;

	int ret = 0;
	int j = 0, s = 1;

	sp_asoc_pr_info("%s,frag_size=%d,frag_cnt=%d\n",
			__func__, frag_size, frag_cnt);

	period_cnt = frag_cnt;
	period = frag_size;
	totsize = period * period_cnt;

	dma_src_phys = audio_addr_ap2dsp(DDR32, srtd->buffer_paddr, 0);
	dma_des_phys = (dma_addr_t)srtd->iram_paddr;

	/* request dma channel */
	dma_cap_zero(mask);
	dma_cap_set(DMA_MEMCPY|DMA_SG, mask);

	dma_chn_request = dma_request_slave_channel(
					pdata->dev,
					"compr_dma_stage1");
	if (IS_ERR_OR_NULL(dma_chn_request)) {
		pr_err("ERR:PCM Request DMA Error %p\n", dma_chn_request);
		ret = -3;
		goto hw_param_err;
	}
	srtd->dma_chn[s] = dma_chn_request;
	sp_asoc_pr_dbg("dma_chn_request=%p\n", dma_chn_request);

	cfg_ptr = kzalloc(
		sizeof(struct sprd_dma_cfg)
		+ sizeof(struct scatterlist) * period_cnt,
		GFP_KERNEL);

	if (!cfg_ptr) {
		ret = -4;
		pr_err("cfg_ptr:kzalloc error\n");
		goto hw_param_err;
	}
	memset(cfg_ptr, 0,
		sizeof(struct sprd_dma_cfg)
		+ sizeof(struct scatterlist) * period_cnt);

	cfg_ptr->sg = (struct scatterlist *)((u8 *) &
		(cfg_ptr->sg) + sizeof(void *));

	dma_cb_data_ptr = srtd->dma_cb_data;
	memset(dma_cb_data_ptr, 0, sizeof(struct sprd_compr_dma_cb_data));
	dma_cb_data_ptr->dma_chn = srtd->dma_chn[(s)];
	dma_cb_data_ptr->substream = substream;

	srtd->params.frag_len = period;
	srtd->params.block_len = period;
	/* none-wrap */
	/* srtd->params.tran_len = period *
	 * ((period_cnt > 1) ? (period_cnt - 1) : period_cnt);
	 */
	srtd->params.tran_len = period * period_cnt;

	/*config dma */
	cfg_ptr->config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	cfg_ptr->config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	cfg_ptr->config.src_maxburst = period;
	cfg_ptr->config.slave_id = 0;
	cfg_ptr->config.step = DMA_SLAVE_BUSWIDTH_2_BYTES;
	cfg_ptr->config.direction = DMA_MEM_TO_DEV;
	if (substream->direction == SND_COMPRESS_PLAYBACK) {
		cfg_ptr->config.src_addr = dma_src_phys;
		cfg_ptr->config.dst_addr = audio_addr_ap2dsp(IRAM_OFFLOAD,
			dma_des_phys, 0);
		cfg_ptr->config.direction = DMA_MEM_TO_DEV;
	} else {
		cfg_ptr->config.dst_addr = dma_src_phys;
		cfg_ptr->config.src_addr = audio_addr_ap2dsp(IRAM_OFFLOAD,
			dma_des_phys, 0);
		cfg_ptr->config.direction = DMA_DEV_TO_MEM;
	}
#if defined(CONFIG_SPRD_COMPR_CM4_WAKE_UP)
	cfg_ptr->dma_config_flag = SPRD_DMA_FLAGS(SPRD_DMA_DST_CHN1,
		SPRD_DMA_TRANS_DONE_TRG, SPRD_DMA_FRAG_REQ, SPRD_DMA_NO_INT);
#else
	cfg_ptr->dma_config_flag = SPRD_DMA_FLAGS(SPRD_DMA_DST_CHN1,
		SPRD_DMA_TRANS_DONE_TRG, SPRD_DMA_FRAG_REQ,
		SPRD_DMA_DST_CHN1_INT);
#endif
	cfg_ptr->ll_cfg.virt_addr = (unsigned long)(srtd->dma_cfg_virt[s]);
	cfg_ptr->ll_cfg.phy_addr = (unsigned long)(srtd->dma_cfg_phy[s]);
	cfg_ptr->ll_cfg.wrap_ptr = audio_addr_ap2dsp(IRAM_OFFLOAD,
		dma_des_phys, 0) + srtd->params.frag_len;
	/* wrap, only two node is OK */
	for (j = 0; j < 2; j++) {
		sg = (struct scatterlist *)((u8 *)cfg_ptr->sg +
			j * sizeof(struct scatterlist));
		if (j == 0) {
			sg_init_table(sg, 2);
			cfg_ptr->sg_num = 2;
		}
		sg_dma_len(sg) = srtd->params.tran_len;
		if (substream->direction == SND_COMPRESS_PLAYBACK)
			sg_dma_address(sg) = audio_addr_ap2dsp(DDR32,
				dma_src_phys, 0);
	}

	srtd->dma_tx_des[s] = sprd_compr_config_dma_channel(srtd->dma_chn[s],
		cfg_ptr, dma_cb_data_ptr, 0);

	if (!srtd->dma_tx_des[s]) {
		ret = -5;
		pr_err("sprd_compr_config_dma_channel error\n");
		goto hw_param_err;
	}
	pr_info("srtd frag_len=%d, block_len=%d, tran_len=%d\n",
		srtd->params.frag_len,
		srtd->params.block_len,
		srtd->params.tran_len);
	pr_info(
		"totsize=%zu, period=%zu, srtd->dma_tx_des[0]=0x%p\n",
		totsize, period, srtd->dma_tx_des[0]);
	pr_info("cfg_ptr->ll_cfg.phy_addr:%p,src:%p,dst:%p,wrap_ptr:%p,dma_chan:%d\n",
		(void *)(unsigned long)cfg_ptr->ll_cfg.phy_addr,
		(void *)(unsigned long)cfg_ptr->config.src_addr,
		(void *)(unsigned long)cfg_ptr->config.dst_addr,
		(void *)(unsigned long)cfg_ptr->ll_cfg.wrap_ptr,
		dma_chn_request->chan_id);
	kfree(cfg_ptr);
	goto ok_go_out;

hw_param_err:
	kfree(cfg_ptr);
	pr_err("%s, hw_param_err,ret=%d\n", __func__, ret);

	return ret;

ok_go_out:
	sp_asoc_pr_info("%s, OK\n", __func__);

	return ret;
}

int compr_stream_hw_params1(struct snd_compr_stream *substream,
			      struct snd_compr_params *params)
{
	return compr_stream_config_dma1(substream,
		params->buffer.fragment_size, params->buffer.fragments);
}

int compr_stream_config_dma0(struct snd_compr_stream *substream,
			      u32 frag_size)
{
	struct snd_compr_runtime *runtime = substream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sprd_compr_pdata *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);

	struct dma_chan *dma_chn_request;
	struct sprd_dma_cfg *dma_config_ptr = NULL;
	dma_cap_mask_t mask;
	struct scatterlist *sg = NULL;

	size_t totsize = 0;
	size_t period = 0;
	int period_cnt = 0;

	dma_addr_t dma_src_phys = 0;

	int hw_req_id = 0;

	int ret = 0;
	int j = 0, s = 0;

	sp_asoc_pr_info("%s,frag_size=%d\n", __func__, frag_size);

	/* request dma channel */
	dma_cap_zero(mask);
	dma_cap_set(DMA_MEMCPY|DMA_SG, mask);

	dma_chn_request = dma_request_slave_channel(
					pdata->dev,
					"compr_dma_stage0");

	if (IS_ERR_OR_NULL(dma_chn_request)) {
		pr_err("ERR:PCM Request DMA Error %p\n", dma_chn_request);
		ret = -1;
		goto hw_param_err;
	}
	srtd->dma_chn[s] = dma_chn_request;
	sp_asoc_pr_dbg("dma_chn_request=%p\n", dma_chn_request);

	dma_config_ptr = kzalloc(
		sizeof(struct sprd_dma_cfg)
		+ sizeof(struct scatterlist) *
		(SPRD_COMPR_DMA_DESC_SIZE /
		DMA_LINKLIST_CFG_NODE_SIZE),
		GFP_KERNEL);

	if (!dma_config_ptr) {
		ret = -2;
		pr_err("dma_config_ptr alloc mem error:\n");
		goto hw_param_err;
	}
	memset(dma_config_ptr, 0,
		sizeof(struct sprd_dma_cfg)
			+ sizeof(struct scatterlist) *
			(SPRD_COMPR_DMA_DESC_SIZE /
			DMA_LINKLIST_CFG_NODE_SIZE));
	dma_config_ptr->sg = (struct scatterlist *)((u8 *) &
		(dma_config_ptr->sg) + sizeof(void *));
	dma_src_phys = (dma_addr_t)srtd->iram_paddr;
	if (substream->direction == SND_COMPRESS_PLAYBACK)
		hw_req_id = mcdt_dac_dma_enable(MCDT_CHN_COMPR, MCDT_EMPTY_WMK);
	else
		hw_req_id = mcdt_adc_dma_enable(MCDT_CHN_COMPR, MCDT_FULL_WMK);

	period = (MCDT_FIFO_SIZE - MCDT_EMPTY_WMK) * 4;
	period_cnt = frag_size / period;
	totsize = period * period_cnt;

	/*config dma */
	dma_config_ptr->config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_config_ptr->config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_config_ptr->config.src_maxburst = period;
	dma_config_ptr->config.slave_id =  hw_req_id;
	dma_config_ptr->config.step = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_config_ptr->config.direction = DMA_MEM_TO_DEV;
	if (substream->direction == SND_COMPRESS_PLAYBACK) {
		dma_config_ptr->config.dst_addr =
			mcdt_dac_dma_phy_addr(MCDT_CHN_COMPR);
		dma_config_ptr->config.src_addr =
			audio_addr_ap2dsp(IRAM_OFFLOAD, dma_src_phys, 0);
		dma_config_ptr->config.direction = DMA_MEM_TO_DEV;
	} else {
		dma_config_ptr->config.src_addr =
			mcdt_dac_dma_phy_addr(MCDT_CHN_COMPR);
		dma_config_ptr->config.dst_addr =
			audio_addr_ap2dsp(IRAM_OFFLOAD, dma_src_phys, 0);
		dma_config_ptr->config.direction = DMA_DEV_TO_MEM;
	}
#if defined(CONFIG_SPRD_COMPR_CM4_WAKE_UP)
	dma_config_ptr->dma_config_flag = SPRD_DMA_FLAGS(SPRD_DMA_SRC_CHN1,
				SPRD_DMA_TRANS_DONE_TRG, SPRD_DMA_FRAG_REQ,
				SPRD_DMA_TRANS_INT);
#else
	dma_config_ptr->dma_config_flag = SPRD_DMA_FLAGS(
		SPRD_DMA_SRC_CHN1, SPRD_DMA_TRANS_DONE_TRG,
		SPRD_DMA_FRAG_REQ, SPRD_DMA_NO_INT);
#endif
	dma_config_ptr->ll_cfg.virt_addr =
		(unsigned long)(srtd->dma_cfg_virt[s]);
	dma_config_ptr->ll_cfg.phy_addr =
			audio_addr_ap2dsp(IRAM_OFFLOAD,
				(unsigned long)(srtd->dma_cfg_phy[s]), 0);

	/*config dma linklist node */
	for (j = 0; j < 2; j++) {
		sg = (struct scatterlist *)((u8 *)dma_config_ptr->sg +
			j * sizeof(struct scatterlist));
		if (j == 0) {
			sg_init_table(sg, 2);
			dma_config_ptr->sg_num = 2;
		}
		sg_dma_len(sg) = totsize;
		if (substream->direction == SND_COMPRESS_PLAYBACK) {
			sg_dma_address(sg) = audio_addr_ap2dsp(IRAM_OFFLOAD,
				      dma_src_phys, 0);
		} else {
			sg_dma_address(sg) =
				mcdt_dac_dma_phy_addr(MCDT_CHN_COMPR);
		}
	}

	/* config dma channel */
	ret = dmaengine_slave_config(dma_chn_request,
				     &(dma_config_ptr->config));
	if (ret < 0) {
		ret = -4;
		goto hw_param_err;
	}

	/* get dma desc from dma config */
	srtd->dma_tx_des[s] =
			dma_chn_request->device->device_prep_slave_sg(
				dma_chn_request,
				dma_config_ptr->sg,
				dma_config_ptr->sg_num,
				dma_config_ptr->config.direction,
				dma_config_ptr->dma_config_flag,
				&(dma_config_ptr->ll_cfg));
	if (!srtd->dma_tx_des[s]) {
		ret = -5;
		pr_err("device_prep_slave_sg error\n");
		goto hw_param_err;
	}

#if COMPR_DUMP_MEM_DEBUG
	srtd->dma_tx_des[s]->callback = sprd_compr_dma_buf_done0;
#endif
	pr_info(
			"totsize=%zu, period=%zu, hw_req_id=%d, srtd->dma_tx_des[0]=0x%p,srtd->iram_paddr:%x,dst_addr:%p,src_addr:%p,dma_chn_request:%d\n",
			totsize, period,
			hw_req_id,
			srtd->dma_tx_des[0],
			srtd->iram_paddr,
			(void *)(unsigned long)dma_config_ptr->config.dst_addr,
			(void *)(unsigned long)dma_config_ptr->config.src_addr,
			dma_chn_request->chan_id);
	pr_info("dma_config_ptr->ll_cfg.phy_addr:%p\n",
		(void *)(unsigned long)dma_config_ptr->ll_cfg.phy_addr);
	kfree(dma_config_ptr);
	goto ok_go_out;

hw_param_err:
	kfree(dma_config_ptr);
	pr_err("%s, hw_param_err, ret=%d\n", __func__, ret);

	return ret;

ok_go_out:
	sp_asoc_pr_info("%s, OK\n", __func__);

	return ret;
}

int compr_stream_hw_params0(struct snd_compr_stream *substream,
					struct snd_compr_params *params)
{
	sp_asoc_pr_info("srtd buffer.fragments=%d, fragment_size=%d\n",
			params->buffer.fragments, params->buffer.fragment_size);

	return compr_stream_config_dma0(substream,
		params->buffer.fragment_size);
}

static int compr_stream_hw_free(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	int s = 0;

	ADEBUG();

	if (cstream->direction == SND_COMPRESS_PLAYBACK)
		mcdt_dac_dma_disable(MCDT_CHN_COMPR);
	else
		mcdt_adc_dma_disable(MCDT_CHN_COMPR);

	sprd_compr_proc_done(cstream);

	for (s = 0; s < srtd->dma_stage; s++)
		if (srtd->dma_chn[s]) {
			dma_release_channel(srtd->dma_chn[s]);
			srtd->dma_chn[s] = 0;
		}

	if (srtd->buffer_paddr) {
		audio_mem_free(DDR32, srtd->buffer_paddr, srtd->total_size);
		srtd->buffer_paddr = 0;
	}
	if (srtd->buffer) {
		audio_mem_unmap(srtd->buffer);
		srtd->buffer = 0;
	}
	if (srtd->is_access_enabled) {
		agdsp_access_disable();
		srtd->is_access_enabled = false;
	}
	srtd->dma_cfg_virt[1] = 0;
	srtd->dma_cfg_phy[1] = 0;

	/* __iounmap(srtd->dma_cfg_virt[0]); */
	srtd->dma_cfg_virt[0] = 0;
	srtd->dma_cfg_phy[0] = 0;

	audio_mem_unmap(srtd->iram_buff);
	srtd->iram_buff = 0;
	srtd->dma_paused = false;
#ifdef CONFIG_SPRD_COMPR_CM4_WAKE_UP
	mutex_lock(&g_lock);
	compr_cb_data = NULL;
	mutex_unlock(&g_lock);
#endif

	kfree(srtd->dma_cb_data);

	kfree(srtd);

	return 0;
}

static int compr_stream_trigger(struct snd_compr_stream *substream, int cmd)
{
	struct sprd_compr_rtd *srtd = substream->runtime->private_data;
	struct sprd_compr_dma_params *dma = &srtd->params;
	int ret = 0;
	int s = 0, i = 0;

	if (!dma) {
		sp_asoc_pr_dbg("no trigger");
		return 0;
	}

	s = srtd->dma_stage;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	/*
	 * case SNDRV_PCM_TRIGGER_RESUME:
	 * case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	 */
		if (srtd->next_track) {
			srtd->next_track--;
			break;
		}
		if (srtd->dma_paused == true) {
			struct sprd_compr_dma_params *params;
			for (i = 0; i < srtd->dma_stage; i++)
				if (srtd->dma_chn[i]) {
					dma_release_channel(srtd->dma_chn[i]);
					srtd->dma_chn[i] = 0;
				}
			params = &srtd->params;
			compr_stream_config_dma1(substream,
				params->frag_len,
				params->tran_len / params->frag_len);
			compr_stream_config_dma0(substream,
				params->frag_len);
			srtd->next_track = 0;
			srtd->dma_paused = false;
		}

		for (i = s - 1; i >= 0; i--) {
			if (srtd->dma_tx_des[i]) {
				srtd->cookie[i] =
					dmaengine_submit(srtd->dma_tx_des[i]);
				ret = dma_submit_error(srtd->cookie[i]);
				if (ret) {
					pr_err("dmaengine_submit error:%d,i:%d\n",
						ret, i);
					return ret;
				}
			}
		}
		for (i = s - 1; i >= 0; i--) {
			if (srtd->dma_chn[i])
				dma_async_issue_pending(srtd->dma_chn[i]);
		}

#if COMPR_DUMP_MEM_DEBUG
		timer_init();
#endif
		pr_info("%s:start out\n", __func__);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	/*
	 * case SNDRV_PCM_TRIGGER_SUSPEND:
	 * case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	 */
		for (i = s - 1; i >= 0; i--) {
			if (srtd->dma_chn[i])
				dmaengine_pause(srtd->dma_chn[i]);
		}
		srtd->dma_paused = true;
#if COMPR_DUMP_MEM_DEBUG
		timer_exit();
#endif
		srtd->cb_called = 0;
		sp_asoc_pr_info("E\n");
		pr_info("%s:stop out\n", __func__);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int sprd_platform_send_cmd(
								int cmd,
								int stream_id,
								void *buff,
								int buff_size)
{
	struct cmd_common command = {0};
	int ret = 0;

	command.command = cmd;
	command.sub_cmd = stream_id;
	sp_asoc_pr_dbg("%s:cmd=%d,stream_id=%d\n",
		__func__, cmd, stream_id);

	ret = compr_send_cmd(cmd, (void *)&command, sizeof(command));
	if (ret < 0) {
		sp_asoc_pr_dbg("%s: failed to send command(%d), ret=%d\n",
			__func__, cmd, ret);
		return -EIO;
	}

	return 0;
}

static int sprd_platform_send_param(
								int cmd,
								int stream_id,
								void *buff,
								int buff_size)
{
	int ret = 0;

	sp_asoc_pr_dbg("%s:cmd=%d,stream_id=%d,buff=0x%p,buff_size=%d\n",
		__func__, cmd, stream_id, buff, buff_size);

	ret = compr_send_cmd(cmd, buff, buff_size);
	if (ret < 0) {
		sp_asoc_pr_dbg(
				"%s: failed to send command(%d), ret=%d\n",
				__func__, cmd, ret);
		return -EIO;
	}

	return 0;
}

static int sprd_platform_compr_open(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);

	int result = 0;

	const int stream_id = cstream->direction;
	struct sprd_compr_rtd *srtd = NULL;
	struct sprd_compr_dev_ctrl *dev_ctrl = NULL;

	ADEBUG();

	sprd_compress_new(rtd);

	sp_asoc_pr_info(
		"%s E,pdata=0x%p, platform name=%s\n", __func__,
		pdata, rtd->platform->component.name);

	srtd = kzalloc(
			sizeof(struct sprd_compr_rtd),
			GFP_KERNEL);
	memset(srtd, 0, sizeof(struct sprd_compr_rtd));
	if (!srtd) {
		sp_asoc_pr_dbg("srtd is NULL!\n");
		result = -1;
		goto err;
	}

	sp_asoc_pr_info(
		"%s ,srtd=0x%p,cstream->direction=%d\n", __func__, srtd,
		cstream->direction);

	srtd->cstream = cstream;

	srtd->stream_id = stream_id;
	srtd->stream_state = COMPR_IDLE;
	srtd->codec = FORMAT_MP3;
	srtd->sample_rate = 44100;
	srtd->num_channels = 2;
	srtd->hw_chan = 1;
	srtd->dma_paused = false;

	runtime->private_data = srtd;

	sprd_compr_populate_codec_list(srtd);

	spin_lock_init(&srtd->lock);

	atomic_set(&srtd->eos, 0);
	atomic_set(&srtd->start, 0);
	atomic_set(&srtd->drain, 0);
	atomic_set(&srtd->xrun, 0);
	atomic_set(&srtd->pause, 0);
	wakeup_source_init(&srtd->wake_lock, "compr write");

	init_waitqueue_head(&srtd->eos_wait);
	init_waitqueue_head(&srtd->drain_wait);
	init_waitqueue_head(&srtd->flush_wait);

	dev_ctrl = &pdata->dev_ctrl;
	mutex_lock(&dev_ctrl->mutex);
	result = agdsp_access_enable();
	if (result) {
		pr_err("%s:agdsp_access_enable:error:%d",
			__func__, result);
		goto out_ops;
	}
	srtd->is_access_enabled = true;
	sp_asoc_pr_dbg("%s: before send open cmd\n", __func__);

	result = sprd_platform_send_cmd(COMPR_CMD_OPEN, stream_id, 0, 0);

	if (result < 0)
		goto out_ops;

	INIT_WORK(&srtd->drain_work, sprd_compr_drain_work);

	mutex_unlock(&dev_ctrl->mutex);
	ADEBUG();

	return 0;

out_ops:
	sp_asoc_pr_dbg("%s failed\n", __func__);
	if (srtd && srtd->is_access_enabled) {
		agdsp_access_disable();
		srtd->is_access_enabled = false;
	}
	mutex_unlock(&dev_ctrl->mutex);
err:
	return -1;
}

static int sprd_platform_compr_free(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);

	int ret = 0;
	const int stream_id = cstream->direction;
	struct sprd_compr_dev_ctrl *dev_ctrl = &pdata->dev_ctrl;

	ADEBUG();

	if (srtd->stream_state == COMPR_TRIGGERED)
		sprd_platform_compr_trigger(cstream, SNDRV_PCM_TRIGGER_STOP);

#ifdef CONFIG_SPRD_COMPR_CM4_WAKE_UP
	if (srtd->thread) {
		srtd->stream_state = COMPR_CLOSED;
		wake_up(&srtd->drain_wait);
		kthread_stop(srtd->thread);
	}
#endif

	flush_workqueue(pdata->drain_work_q);

	if (srtd->wake_locked) {
		sp_asoc_pr_info("wake_lock released\n");
		__pm_relax(&srtd->wake_lock);
		srtd->wake_locked = 0;
	}
	wakeup_source_trash(&srtd->wake_lock);

	compr_stream_hw_free(cstream);
	mutex_lock(&dev_ctrl->mutex);
	ret = sprd_platform_send_cmd(COMPR_CMD_CLOSE, stream_id, 0, 0);
	if (ret < 0)
		sp_asoc_pr_info("send COMPR_CMD_CLOSE ret %d\n", ret);

	mutex_unlock(&dev_ctrl->mutex);

	return 0;
}

static int sprd_platform_compr_update_params(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct sprd_compr_dev_ctrl *dev_ctrl = &pdata->dev_ctrl;
	struct cmd_prepare prepare = {};
	int result = 0;

	ADEBUG();

	prepare.common.command = COMPR_CMD_PARAMS;
	prepare.common.sub_cmd = cstream->direction;
	prepare.samplerate = srtd->sample_rate;
	prepare.channels = srtd->num_channels;
	prepare.format = srtd->codec;
	prepare.info_paddr =
		audio_addr_ap2dsp(IRAM_OFFLOAD, srtd->info_paddr, 0);
	prepare.info_size = srtd->info_size;
	prepare.mcdt_chn = MCDT_CHN_COMPR;
	prepare.rate = srtd->codec_param.codec.bit_rate;

	mutex_lock(&dev_ctrl->mutex);
	result = sprd_platform_send_param(COMPR_CMD_PARAMS, 0, &prepare,
					  sizeof(prepare));
	mutex_unlock(&dev_ctrl->mutex);

	return result;
}

static int sprd_platform_compr_set_params(struct snd_compr_stream *cstream,
					struct snd_compr_params *params)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct sprd_compr_dev_ctrl *dev_ctrl = NULL;
	const int stream_id = cstream->direction;
	int result = 0;
	unsigned int rate;
	struct cmd_prepare prepare;

	ADEBUG();
	/*prtd->cstream = cstream;*/

	memcpy(&srtd->codec_param, params, sizeof(struct snd_compr_params));

	/* ToDo: remove duplicates */
	srtd->num_channels = srtd->codec_param.codec.ch_in;
	sp_asoc_pr_info("%s: channels: %d, sample_rate=%d\n",
			__func__,
			srtd->num_channels,
			srtd->codec_param.codec.sample_rate);

	/*
	 * User space will give the sample rate in two ways:
	 *   1) SNDRV_PCM_RATE_xxx(old way)
	 *   2) the direct sample rate, like 48000(new way)
	 * Following codes are written to cover these 2 cases.
	 */
	rate = snd_pcm_rate_bit_to_rate(srtd->codec_param.codec.sample_rate);
	if (rate)
		srtd->sample_rate = rate;
	else
		srtd->sample_rate = srtd->codec_param.codec.sample_rate;
	sp_asoc_pr_info("%s: sample_rate %d, code.id=%d\n",
			__func__, srtd->sample_rate, params->codec.id);

	switch (params->codec.id) {
	case SND_AUDIOCODEC_MP3:
		srtd->codec = FORMAT_MP3;
		break;
	case SND_AUDIOCODEC_AAC:
		srtd->codec = FORMAT_AAC;
		break;
	default:
		return -EINVAL;
	}

	result = sprd_compr_config_sprd_rtd(srtd, cstream, params);
	if (result) {
		sp_asoc_pr_info("sprd_compr_config_sprd_rtd error %d\n",
				result);
		return result;
	}

	result = compr_stream_hw_params1(cstream, params);
	if (result) {
		sp_asoc_pr_info("compr_stream_hw_params1 error %d\n", result);
		return result;
	}

	result = compr_stream_hw_params0(cstream, params);
	if (result) {
		sp_asoc_pr_info("compr_stream_hw_params0 error %d\n", result);
		return result;
	}

	result = sprd_compr_configure_dsp(srtd);
	if (result) {
		sp_asoc_pr_info("sprd_compr_configure_dsp error %d\n", result);
		return result;
	}

	dev_ctrl = &pdata->dev_ctrl;

	memset(&prepare, 0, sizeof(prepare));
	prepare.common.command = COMPR_CMD_PARAMS;
	prepare.common.sub_cmd = stream_id;
	prepare.samplerate = srtd->sample_rate;
	prepare.channels = srtd->num_channels;
	prepare.format = srtd->codec;
	prepare.info_paddr = audio_addr_ap2dsp(IRAM_OFFLOAD,
		srtd->info_paddr, 0);
	prepare.info_size = srtd->info_size;
	prepare.mcdt_chn = MCDT_CHN_COMPR;
	prepare.rate = srtd->codec_param.codec.bit_rate;

	mutex_lock(&dev_ctrl->mutex);

	result =
		sprd_platform_send_param(COMPR_CMD_PARAMS,
					stream_id, &prepare, sizeof(prepare));

	if (result < 0)
		goto out_ops;
	mutex_unlock(&dev_ctrl->mutex);

	srtd->stream_state = COMPR_PARAMSED;

	ADEBUG();

	return 0;

out_ops:
	sp_asoc_pr_info("%s failed\n", __func__);
	mutex_unlock(&dev_ctrl->mutex);

	return result;
}


static int sprd_platform_compr_trigger(
			struct snd_compr_stream *cstream,
								int cmd)
{
	int drain_cmd = COMPR_CMD_DRAIN;
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);

	int result = 0;
	const int stream_id = cstream->direction;

	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct sprd_compr_dev_ctrl *dev_ctrl = &pdata->dev_ctrl;
	u32 padding_cnt;

	ADEBUG();

	if (cstream->direction != SND_COMPRESS_PLAYBACK) {
		sp_asoc_pr_dbg("%s: Unsupported stream type\n", __func__);
		return -EINVAL;
	}
	compr_stream_trigger(cstream, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		sp_asoc_pr_info("%s: SNDRV_PCM_TRIGGER_START\n", __func__);
		spin_lock_irq(&srtd->lock);
		atomic_set(&srtd->start, 1);
		srtd->stream_state = COMPR_TRIGGERED;
		spin_unlock_irq(&srtd->lock);

		if (srtd->received_total < runtime->fragment_size &&
		    srtd->received_stage0 >= runtime->fragment_size)
			srtd->iram_write_pos = srtd->received_total;
		else
			srtd->iram_write_pos = 0;

		mutex_lock(&dev_ctrl->mutex);
		result =
			sprd_platform_send_cmd(COMPR_CMD_START,
							stream_id, 0, 0);
		if (result < 0)
			goto out_ops;
		mutex_unlock(&dev_ctrl->mutex);

		break;
	case SNDRV_PCM_TRIGGER_STOP:
		sp_asoc_pr_info("%s: SNDRV_PCM_TRIGGER_STOP\n", __func__);

		if (atomic_read(&srtd->drain)) {
			sp_asoc_pr_dbg("wake up on drain\n");
			srtd->drain_ready = 1;
			/*wake_up(&srtd->drain_wait);*/
			atomic_set(&srtd->drain, 0);
		}
		spin_lock_irq(&srtd->lock);
		atomic_set(&srtd->start, 0);
		atomic_set(&srtd->pause, 0);
		srtd->stream_state = COMPR_STOPPED;
		srtd->copied_total = 0;
		srtd->app_pointer  = 0;
		srtd->received_total = 0;
		srtd->received_stage0 = 0;
		srtd->received_stage1 = 0;
		srtd->avail_total = 0;
		srtd->iram_write_pos = 0;
		memset_io((void *)srtd->info_vaddr,
					0,
					sizeof(struct sprd_compr_playinfo));
		if (srtd->next_track)
			srtd->next_track--;
		spin_unlock_irq(&srtd->lock);

		mutex_lock(&dev_ctrl->mutex);
		result =
			sprd_platform_send_cmd(
					COMPR_CMD_STOP,
					stream_id, 0, 0);
		if (result < 0)
			goto out_ops;

		if (cstream->direction == SND_COMPRESS_PLAYBACK) {
			mcdt_da_fifo_clr(MCDT_CHN_COMPR);
			mcdt_dac_dma_disable(MCDT_CHN_COMPR);
		} else {
			mcdt_ad_fifo_clr(MCDT_CHN_COMPR);
			mcdt_adc_dma_disable(MCDT_CHN_COMPR);
		}
		mutex_unlock(&dev_ctrl->mutex);

		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		sp_asoc_pr_info("%s: SNDRV_PCM_TRIGGER_PAUSE_PUSH\n", __func__);

		mutex_lock(&dev_ctrl->mutex);
		atomic_set(&srtd->pause, 1);

		result =
			sprd_platform_send_cmd(COMPR_CMD_PAUSE_PUSH,
					stream_id, 0, 0);
		if (result < 0)
			goto out_ops;

		srtd->stream_state = COMPR_STOPPED;
		mutex_unlock(&dev_ctrl->mutex);

		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		sp_asoc_pr_dbg("%s: SNDRV_PCM_TRIGGER_PAUSE_RELEASE\n",
								__func__);

		mutex_lock(&dev_ctrl->mutex);
		if (atomic_read(&srtd->pause)) {
			result =
				sprd_platform_send_cmd(COMPR_CMD_PAUSE_RELEASE,
							stream_id, 0, 0);
			if (result < 0)
				goto out_ops;
		}
		srtd->stream_state = COMPR_TRIGGERED;

		atomic_set(&srtd->pause, 0);
		mutex_unlock(&dev_ctrl->mutex);

		break;
	case SND_COMPR_TRIGGER_PARTIAL_DRAIN:
		drain_cmd = COMPR_CMD_PARTIAL_DRAIN;
		srtd->iram_write_pos = 0;

		sp_asoc_pr_info("partial drain,app_pointer=%d, received_stage0=%d\n",
				srtd->app_pointer,
				srtd->received_stage0);

		if (srtd->received_stage0 < runtime->fragment_size) {
			padding_cnt = runtime->fragment_size -
				(srtd->received_stage0 %
				runtime->fragment_size);

			if ((padding_cnt <= MCDT_FIFO_SIZE_BYTE * 2)) {
				padding_cnt += runtime->fragment_size;
				srtd->app_pointer = runtime->fragment_size;
				srtd->app_pointer = srtd->app_pointer %
					srtd->buffer_size;
			}
			srtd->received_stage0 = runtime->fragment_size;
		} else {
			padding_cnt = runtime->fragment_size -
				(srtd->app_pointer % runtime->fragment_size);
			memset(srtd->buffer + srtd->app_pointer,
			       0x5a, padding_cnt);

			if (padding_cnt <= MCDT_FIFO_SIZE_BYTE * 2)
				padding_cnt += runtime->fragment_size;
			srtd->app_pointer += padding_cnt;
			srtd->app_pointer = srtd->app_pointer %
				srtd->buffer_size;
		}
		srtd->drain_info.padding_cnt = padding_cnt;
		srtd->received_total += padding_cnt;

		sp_asoc_pr_info("partial drain,padding_cnt=%d,app_pointer=%d, received_stage0=%d\n",
				srtd->drain_info.padding_cnt,
				srtd->app_pointer,
				srtd->received_stage0);
	case SND_COMPR_TRIGGER_DRAIN:
		sp_asoc_pr_info("%s: SNDRV_COMPRESS_DRAIN, cmd=%d, total=%d\n",
			__func__, cmd, srtd->received_total);
		/* Make sure all the data is sent to DSP before sending EOS */
		atomic_set(&srtd->drain, 1);

		mutex_lock(&dev_ctrl->mutex);

		result = compr_send_cmd_no_wait(AMSG_CH_MP3_OFFLOAD_DRAIN,
						drain_cmd,
						srtd->received_total,
						srtd->drain_info.padding_cnt);
		if (result < 0) {
			sp_asoc_pr_info("drain out err!");
			goto out_ops;
		}
		mutex_unlock(&dev_ctrl->mutex);

		srtd->drain_ready = 1;
		/*
		 * wake_up(&srtd->drain_wait);
		 */
		queue_work(pdata->drain_work_q, &srtd->drain_work);
		sp_asoc_pr_info("%s: out of drain\n", __func__);
		break;
	case SND_COMPR_TRIGGER_NEXT_TRACK:
		sp_asoc_pr_info("%s: SND_COMPR_TRIGGER_NEXT_TRACK\n", __func__);
		srtd->next_track++;
		srtd->drain_info.stream_id =
			(srtd->drain_info.stream_id + 1) % 2;
		srtd->drain_info.received_total[srtd->drain_info.stream_id] =
			srtd->received_total;
		sp_asoc_pr_info("%s: next_track=%d,receive_total[%d]=%d\n",
				__func__,
				srtd->next_track,
				srtd->drain_info.stream_id,
				srtd->drain_info.received_total[
				srtd->drain_info.stream_id]);
		break;
	default:
		sp_asoc_pr_info("%s: no responsed cmd=%d\n", __func__, cmd);
		break;
	}

	ADEBUG();

	return 0;

out_ops:
	sp_asoc_pr_info("%s failed\n", __func__);
	mutex_unlock(&dev_ctrl->mutex);

	return 0;
}

static int sprd_platform_compr_pointer(struct snd_compr_stream *cstream,
					struct snd_compr_tstamp *arg)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct sprd_compr_pdata *pdata
		= snd_soc_platform_get_drvdata(rtd->platform);
	struct sprd_compr_rtd *srtd = runtime->private_data;
	struct sprd_compr_playinfo *info
		= (struct sprd_compr_playinfo *)srtd->info_vaddr;
	struct snd_compr_tstamp tstamp;

	ADEBUG();

	sp_asoc_pr_dbg("%s E, info=0x%p\n", __func__, info);
	if (pdata->suspend == true) {
		sp_asoc_pr_dbg("%s, wait_event\n", __func__);
		wait_event(pdata->point_wait, !pdata->suspend);
	}
	memset(&tstamp, 0x0, sizeof(struct snd_compr_tstamp));

	tstamp.sampling_rate = srtd->sample_rate;
	tstamp.copied_total = srtd->copied_total;

	tstamp.pcm_io_frames = (uint64_t)info->uiCurrentDataOffset;
	/*
	 * timestamp = (uint64_t)info->uiCurrentDataOffset * 1000;
	 * timestamp = div64_u64(timestamp, srtd->sample_rate);
	 * tstamp.timestamp = timestamp;
	 */
	memcpy(arg, &tstamp, sizeof(struct snd_compr_tstamp));
	sp_asoc_pr_info(
		"%s: copied_total: %d bit_rate: %d sample_rate: %d  pcm_io_frames: %d\n",
		__func__,
		tstamp.copied_total,
		srtd->codec_param.codec.bit_rate,
		srtd->sample_rate,
		tstamp.pcm_io_frames);

	return 0;
}

static int sprd_platform_compr_copy(struct snd_compr_stream *cstream,
						char __user *buf,
								size_t count)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;

	void *dstn;
	u32 copy;
	u32 bytes_available = 0;
	u32 count_bak = count;
	char __user *buf_bak = buf;

	if (srtd->wake_locked) {
		sp_asoc_pr_info("wake_lock released\n");
		__pm_relax(&srtd->wake_lock);
		srtd->wake_locked = 0;
	}

	sp_asoc_pr_dbg(
		"%s: count = %d,app_pointer=%d,buff_size=%d,received_total=%d\n",
		__func__,
		(u32)count,
		srtd->app_pointer,
		srtd->buffer_size,
		srtd->received_total);

#if COMPR_DUMP_DEBUG
	mm_segment_t *old_fs = 0;

	old_fs = get_fs();
	set_fs(get_ds());
#endif

	if (srtd->received_stage0 < runtime->fragment_size) {
		bytes_available
			= runtime->fragment_size - srtd->received_stage0;
		dstn = srtd->iram_buff + srtd->received_stage0;
		if (bytes_available >= count) {
			/* copy to stage0 buffer directly */
			if (unalign_copy_from_user(dstn, buf, count))
				return -EFAULT;
			srtd->received_stage0 += count;
			srtd->copied_total += count;
			goto copy_done;
		} else {
			/* copy partial data to stage0 buffer,
			 * and copy left to stage1 buffer
			 */
			copy = bytes_available;
			if (unalign_copy_from_user(dstn, buf, copy))
				return -EFAULT;
			count -= copy;
			srtd->received_stage0 += copy;
			srtd->copied_total += copy;
			buf += copy;
			/* not return, continue to
			 * copy left data to stage1 buffer
			 */
		}
	}

	/* go on copy to stage1 buffer */
	dstn = srtd->buffer + srtd->app_pointer;
	if (count < srtd->buffer_size - srtd->app_pointer) {
		if (unalign_copy_from_user(dstn, buf, count))
			return -EFAULT;
		srtd->app_pointer += count;
#if COMPR_DUMP_DEBUG
		vfs_write(srtd->log_file1,
						dstn,
						count,
						&srtd->log_file1->f_pos);
#endif
	} else {
		copy = srtd->buffer_size - srtd->app_pointer;
		if (unalign_copy_from_user(dstn, buf, copy))
			return -EFAULT;
#if COMPR_DUMP_DEBUG
		vfs_write(srtd->log_file1, dstn, copy, &srtd->log_file1->f_pos);
#endif

		if (unalign_copy_from_user(srtd->buffer,
				buf + copy,
				count - copy))
			return -EFAULT;
		srtd->app_pointer = count - copy;
#if COMPR_DUMP_DEBUG
		vfs_write(srtd->log_file1,
						srtd->buffer,
						count - copy,
						&srtd->log_file1->f_pos);
#endif
	}

	if (srtd->iram_write_pos) {
		void *dst_buf = NULL;
		u32 iram_avail_buf_size = 0;
		u32 copy_size = 0;

		sp_asoc_pr_info("%s: iram_write_pos = %d, before\n",
				__func__,
				srtd->iram_write_pos);

		if (srtd->iram_write_pos < runtime->fragment_size)
			iram_avail_buf_size = runtime->fragment_size
				- srtd->iram_write_pos;
		else
			srtd->iram_write_pos = 0;
		copy_size = iram_avail_buf_size > count_bak ?
			count_bak : iram_avail_buf_size;
		dst_buf = (char *)srtd->iram_buff + srtd->iram_write_pos;
		if (unalign_copy_from_user(dst_buf, buf_bak, copy_size))
			return -EFAULT;
		srtd->iram_write_pos += copy_size;
		if (srtd->iram_write_pos == runtime->fragment_size)
			srtd->iram_write_pos = 0;

		sp_asoc_pr_info("%s: iram_write_pos = %d,copy_size=%d\n",
				__func__,
				srtd->iram_write_pos,
				copy_size);
	}

#if COMPR_DUMP_DEBUG
	set_fs(old_fs);
#endif
	srtd->received_stage1 += count;

copy_done:
	wmb();/*copy done*/
	srtd->received_total += count_bak;
	srtd->avail_total += count_bak;

	sp_asoc_pr_info(
		"%s: count=%zu,stage0=%u,stage1=%u,total=%u,avail_total=%u,app_pointer=%u\n",
		__func__,
		count,
		srtd->received_stage0,
		srtd->received_stage1,
		srtd->received_total,
		srtd->avail_total,
		srtd->app_pointer);

	/* return all the count we copied , including stage 0 */
	return count_bak;
}

static int sprd_platform_compr_ack(struct snd_compr_stream *cstream,
					size_t bytes)
{
	ADEBUG();
	return 0;
}

static int sprd_platform_compr_get_caps(struct snd_compr_stream *cstream,
					struct snd_compr_caps *caps)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;

	ADEBUG();
	memcpy(caps, &srtd->compr_cap, sizeof(struct snd_compr_caps));

	ADEBUG();

	return 0;
}

static int sprd_platform_compr_get_codec_caps(struct snd_compr_stream *cstream,
					struct snd_compr_codec_caps *codec)
{
	ADEBUG();

	/* yintang:how to get codec caps, to be confirmed */

	switch (codec->codec) {
	case SND_AUDIOCODEC_MP3:
		codec->num_descriptors = 2;
		codec->descriptor[0].max_ch = 2;
		/*codec->descriptor[0].sample_rates = 0;*/
		codec->descriptor[0].bit_rate[0] = 320; /* 320kbps */
		codec->descriptor[0].bit_rate[1] = 128;
		codec->descriptor[0].num_bitrates = 2;
		codec->descriptor[0].profiles = 0;
		codec->descriptor[0].modes = SND_AUDIOCHANMODE_MP3_STEREO;
		codec->descriptor[0].formats = 0;
		break;
	case SND_AUDIOCODEC_AAC:
		codec->num_descriptors = 2;
		codec->descriptor[1].max_ch = 2;
		/*codec->descriptor[1].sample_rates = 1;*/
		codec->descriptor[1].bit_rate[0] = 320; /* 320kbps */
		codec->descriptor[1].bit_rate[1] = 128;
		codec->descriptor[1].num_bitrates = 2;
		codec->descriptor[1].profiles = 0;
		codec->descriptor[1].modes = 0;
		codec->descriptor[1].formats = 0;
		break;
	default:
		sp_asoc_pr_dbg("%s: Unsupported audio codec %d\n",
						__func__, codec->codec);
		return -EINVAL;
	}

	ADEBUG();

	return 0;
}

static int sprd_platform_compr_set_metadata(struct snd_compr_stream *cstream,
					struct snd_compr_metadata *metadata)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct sprd_compr_rtd *srtd = runtime->private_data;

	ADEBUG();

	if (!metadata || !cstream)
		return -EINVAL;

	ADEBUG();

	if (metadata->key == SNDRV_COMPRESS_ENCODER_PADDING) {
		sp_asoc_pr_dbg("%s, got encoder padding %u\n",
							__func__,
							metadata->value[0]);
	} else if (metadata->key == SNDRV_COMPRESS_ENCODER_DELAY) {
		sp_asoc_pr_dbg("%s, got encoder delay %u\n",
							__func__,
							metadata->value[0]);
	} else if (metadata->key == SNDRV_COMPRESS_BITRATE) {
		sp_asoc_pr_info("set_metadata,bitrate=%d\n",
				metadata->value[0]);
		srtd->codec_param.codec.bit_rate = metadata->value[0];
		sprd_platform_compr_update_params(cstream);
	} else if (metadata->key == SNDRV_COMPRESS_SAMPLERATE) {
		sp_asoc_pr_info("set_metadata,samplerate=%d\n",
				metadata->value[0]);
		srtd->codec_param.codec.sample_rate = metadata->value[0];
		srtd->sample_rate = metadata->value[0];
		sprd_platform_compr_update_params(cstream);
	} else if (metadata->key == SNDRV_COMPRESS_CHANNEL) {
		sp_asoc_pr_info("set_metadata,channel=%d\n",
				metadata->value[0]);
		srtd->codec_param.codec.ch_in = metadata->value[0];
		srtd->num_channels = metadata->value[0];
		sprd_platform_compr_update_params(cstream);
	}

	return 0;
}

#ifdef CONFIG_PROC_FS
static void sprd_compress_proc_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	struct snd_soc_platform *platform = entry->private_data;

	snd_iprintf(buffer, "%s\n", platform->component.name);
}

static void sprd_compress_proc_init(struct snd_soc_platform *platform)
{
	struct snd_info_entry *entry;

	sp_asoc_pr_dbg("%s E", __func__);
	if (!snd_card_proc_new(platform->component.card->snd_card,
		"sprd-compress", &entry))
		snd_info_set_text_ops(entry, platform, sprd_compress_proc_read);
}
#else /* !CONFIG_PROC_FS */
static inline void sprd_codec_proc_init(struct sprd_codec_priv *sprd_codec)
{
}
#endif

#if COMPR_DUMP_MEM_DEBUG
static void time_handler(unsigned long data)
{
	mod_timer(&stimer, jiffies + 100);
	pr_info("yintang:done_0=%d, done_1=%d\n",
					s_buf_done_count0,
					s_buf_done_count);
	pr_info("yintang:0x%x,0x%x\n", g_iram[0], g_iram[1]);

	pr_info("yintang:0x%x,0x%x,0x%x,0x%x----0x%x,0x%x,0x%x,0x%x\n",
					g_buff[0], g_buff[1],
					g_buff[g_frag_size],
					g_buff[g_frag_size + 1],
					g_buff[g_frag_size * 2],
					g_buff[g_frag_size * 2 + 1],
					g_buff[g_frag_size * 3],
					g_buff[g_frag_size * 3 + 1]);
}
static int timer_init(void)
{
	init_timer(&stimer);
	stimer.data = 0;
	stimer.expires = jiffies + 100;
	stimer.function = time_handler;
	add_timer(&stimer);

	return 0;
}
static void timer_exit(void)
{
	del_timer(&stimer);
}
#endif

static int sprd_compr_probe(struct snd_soc_platform *platform)
{
	struct sprd_compr_pdata *pdata = NULL;
	int result = 0;

	ADEBUG();
	sprd_compress_proc_init(platform);

	pdata = kzalloc(
					sizeof(struct sprd_compr_pdata),
					GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	init_waitqueue_head(&pdata->point_wait);
	mutex_init(&pdata->dev_ctrl.mutex);
#ifdef CONFIG_SPRD_COMPR_CM4_WAKE_UP
	mutex_init(&g_lock);
#endif

	pdata->suspend = false;
	pdata->dev = platform->dev;

	pdata->drain_work_q = create_singlethread_workqueue("compr drain");
	snd_soc_platform_set_drvdata(platform, pdata);

	result = compr_ch_open(AUD_IPC_AGDSP);
	if (result) {
		sp_asoc_pr_dbg(KERN_ERR
			"%s: failed to open sipc channel, ret=%d\n",
							__func__, result);
	}
#ifdef CONFIG_SPRD_COMPR_CM4_WAKE_UP
	pdata->thread = kthread_create(
		sprd_compr_monitor_thread,
		(void *)pdata,
		"compr monitor");
	if (IS_ERR(pdata->thread)) {
		result = PTR_ERR(pdata->thread);
		sp_asoc_pr_info(
			"Failed to create kthread: compr monitor,result=%d\n",
			result);
	}
	wake_up_process(pdata->thread);
#endif

	ADEBUG();

	return 0;
}

static u64 sprd_compr_dmamask = DMA_BIT_MASK(32);
static int sprd_compress_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &sprd_compr_dmamask;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	return 0;
}

static struct snd_compr_ops sprd_platform_compr_ops = {
	.open = sprd_platform_compr_open,
	.free = sprd_platform_compr_free,
	.set_params = sprd_platform_compr_set_params,
	.set_metadata = sprd_platform_compr_set_metadata,
	.trigger = sprd_platform_compr_trigger,
	.pointer = sprd_platform_compr_pointer,
	.copy = sprd_platform_compr_copy,
	.ack = sprd_platform_compr_ack,
	.get_caps = sprd_platform_compr_get_caps,
	.get_codec_caps = sprd_platform_compr_get_codec_caps,
};

static struct snd_soc_platform_driver sprd_platform_compr_drv = {
	.probe		= sprd_compr_probe,
	.compr_ops	= &sprd_platform_compr_ops,
	.pcm_new    = sprd_compress_new,
};

static int sprd_platform_compr_probe(struct platform_device *pdev)
{
	int ret = 0;

	ADEBUG();
	sp_asoc_pr_info("%s E,devname:%s, drivername:%s\n",
		__func__, dev_name(&pdev->dev), (pdev->dev).driver->name);

	ret = snd_soc_register_platform(&pdev->dev, &sprd_platform_compr_drv);
	if (ret) {
		sp_asoc_pr_info("registering soc platform failed\n");
		return ret;
	}
	ADEBUG();

	return ret;
}

static int sprd_platform_compr_remove(struct platform_device *pdev)
{
	struct sprd_compr_pdata *pdata = NULL;
	int ret = 0;

	ADEBUG();

	pdata = (struct sprd_compr_pdata *)dev_get_drvdata(&pdev->dev);
	kfree(pdata);

	snd_soc_unregister_platform(&pdev->dev);
	ADEBUG();
	ret = compr_ch_close(AUD_IPC_AGDSP);
	if (ret)
		sp_asoc_pr_info(KERN_ERR
				"%s: failed to open sipc channel, ret=%d\n",
				__func__, ret);

	return 0;
}

static int sprd_platform_compr_suspend(struct platform_device *pdev,
						pm_message_t state)
{
	struct sprd_compr_pdata *pdata = NULL;

	ADEBUG();

	/*dump_stack();*/

	pdata = (struct sprd_compr_pdata *)dev_get_drvdata(&pdev->dev);
	if (pdata)
		pdata->suspend = true;

	ADEBUG();

	return 0;
}

static int sprd_platform_compr_resume(struct platform_device *pdev)
{
	struct sprd_compr_pdata *pdata = NULL;

	ADEBUG();

	pdata = (struct sprd_compr_pdata *)dev_get_drvdata(&pdev->dev);
	if (pdata) {
		sp_asoc_pr_dbg("sprd compr platform wake_up\n");
		pdata->suspend = false;
		wake_up(&pdata->point_wait);
	}

	ADEBUG();

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sprd_compress_of_match[] = {
	{.compatible = "sprd,sharkl5-compress-platform",},
	{.compatible = "sprd,roc1-compress-platform",},
	{},
};

MODULE_DEVICE_TABLE(of, sprd_compress_of_match);
#endif

static struct platform_driver sprd_platform_compr_driver = {
	.driver = {
		.name = "sprd-compr-platform",
		.owner = THIS_MODULE,
		.of_match_table = sprd_compress_of_match,
	},
	.probe = sprd_platform_compr_probe,
	.remove = sprd_platform_compr_remove,
	.suspend = sprd_platform_compr_suspend,
	.resume = sprd_platform_compr_resume,
};

module_platform_driver(sprd_platform_compr_driver);

MODULE_DESCRIPTION("ASoC Spreadtrum Compress Platform driver");
MODULE_AUTHOR("Yintang Ren<yintang.ren@spreadtrum.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:compress-platform");
