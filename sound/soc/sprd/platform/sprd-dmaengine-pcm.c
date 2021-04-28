/*
 * sound/soc/sprd/platform
 *
 * Front end pcm (not include offload)
 *
 * Copyright (C) 2018 SpreadTrum Ltd.
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
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt(" PCM ")""fmt

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/suspend.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include "audio_mem.h"
#include "sprd-audio.h"
#include "sprd-asoc-common.h"
#include "sprd-dmaengine-pcm.h"
#include "sprd-platform-pcm-routing.h"
#include "sprd-i2s.h"

#define SPRD_PCM_CHANNEL_MAX 2
#define VBC_AUDRCD_FULL_WATERMARK 160

#undef sp_asoc_pr_info
#define sp_asoc_pr_info pr_info

struct audio_pm_dma *pm_dma;

static int test_normal_cap_use_iram = 1;

/* temply add it */
/*"todo pass params use dma_paras by fe dai\n"*/

static int is_normal_cap_use_iram(void)
{
	return test_normal_cap_use_iram;
}

#define sprd_is_normal_playback(cpu_dai_id, stream) \
((((cpu_dai_id) == BE_DAI_ID_NORMAL_AP01_CODEC) ||\
((cpu_dai_id) == BE_DAI_ID_NORMAL_AP01_USB)) && \
(stream) == SNDRV_PCM_STREAM_PLAYBACK)

static inline u32 sprd_pcm_dma_get_addr(struct dma_chan *dma_chn,
	dma_cookie_t cookie, struct snd_pcm_substream *substream)
{
	struct dma_tx_state dma_state;

	dmaengine_tx_status(dma_chn, cookie, &dma_state);

	return dma_state.residue;
}

static int sprd_pcm_preallocate_dma_ddr32_buffer(struct snd_pcm *pcm,
						int stream);

#ifndef DMA_LINKLIST_CFG_NODE_SIZE
#define DMA_LINKLIST_CFG_NODE_SIZE  (sizeof(struct sprd_dma_cfg))
#endif

struct sprd_dma_callback_data {
	struct snd_pcm_substream *substream;
	struct dma_chan *dma_chn;
};

struct sprd_runtime_data {
	int dma_addr_offset;
	struct sprd_pcm_dma_params *params;
	struct dma_chan *dma_chn[2];
	void *dma_cfg_array;
	struct sprd_dma_callback_data *dma_pdata;
	dma_addr_t dma_cfg_phy[2];
	void *dma_cfg_virt[2];
	struct dma_async_tx_descriptor *dma_tx_des[2];
	dma_cookie_t cookie[2];
	/*
	 * If dma address needs a transformation and the
	 * transformation type.
	 */
	enum AUDIO_MEM_TYPE_E transform_type;
	int int_pos_update[2];
	int burst_len;
	int hw_chan;
	int dma_pos_pre[2];
	int dma_pos_wrapped[2];
	int interleaved;
	int cb_called;
#ifdef CONFIG_SND_VERBOSE_PROCFS
	struct snd_info_entry *proc_info_entry;
#endif
	bool is_access_enabled;
};

struct dma_chan_index_name {
	int index;
	const char *name;
};

#define SPRD_SNDRV_PCM_INFO_COMMON ( \
	SNDRV_PCM_INFO_MMAP | \
	SNDRV_PCM_INFO_MMAP_VALID | \
	SNDRV_PCM_INFO_INTERLEAVED | \
	SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME)
#define SPRD_SNDRV_PCM_FMTBIT (SNDRV_PCM_FMTBIT_S16_LE | \
			       SNDRV_PCM_FMTBIT_S24_LE)

static const struct snd_pcm_hardware sprd_pcm_hardware_v1 = {
	.info = SPRD_SNDRV_PCM_INFO_COMMON |
		SNDRV_PCM_INFO_NONINTERLEAVED |
		SNDRV_PCM_INFO_NO_PERIOD_WAKEUP,
	.formats = SPRD_SNDRV_PCM_FMTBIT,
	/* 16bits, stereo-2-channels */
	.period_bytes_min = 1,
	/* non limit */
	/* test haps */
	.period_bytes_max = 1024*1024*8,
	.periods_min = 1,
	/* non limit */
	/* test haps */
	.periods_max = 100,
	/* test haps */
	.buffer_bytes_max = 1024 * 1024 * 8,
};

static const struct snd_pcm_hardware sprd_i2s_pcm_hardware = {
	.info = SPRD_SNDRV_PCM_INFO_COMMON,
	.formats = SPRD_SNDRV_PCM_FMTBIT,
	/* 16bits, stereo-2-channels */
	.period_bytes_min = 8 * 2,
	/* non limit */
	.period_bytes_max = 32 * 2 * 100,
	.periods_min = 1,
	/* non limit */
	.periods_max = PAGE_SIZE / DMA_LINKLIST_CFG_NODE_SIZE,
	.buffer_bytes_max = I2S_BUFFER_BYTES_MAX,
};

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

static inline int sprd_is_i2s(struct snd_soc_dai *cpu_dai)
{
	return (cpu_dai->driver->id == I2S_MAGIC_ID);
}

static inline const char *sprd_dai_pcm_name(struct snd_soc_dai *cpu_dai)
{
	if (sprd_is_i2s(cpu_dai))
		return "I2S";
	return cpu_dai->name ? : "VBC";
}

static struct audio_pm_dma *get_pm_dma(void);

static void normal_dma_protect_spin_lock(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();
	if (sprd_is_normal_playback(srtd->cpu_dai->id,
		substream->stream))
		spin_lock(&pm_dma->pm_splk_dma_prot);
}

static void normal_dma_protect_spin_unlock(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();
	if (sprd_is_normal_playback(srtd->cpu_dai->id,
		substream->stream))
		spin_unlock(&pm_dma->pm_splk_dma_prot);
}

static void normal_dma_protect_mutex_lock(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();
	if (sprd_is_normal_playback(srtd->cpu_dai->id,
		substream->stream))
		mutex_lock(&pm_dma->pm_mtx_dma_prot);
}

static void normal_dma_protect_mutex_unlock(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();
	if (sprd_is_normal_playback(srtd->cpu_dai->id,
		substream->stream))
		mutex_unlock(&pm_dma->pm_mtx_dma_prot);
}

static inline int sprd_pcm_is_interleaved(struct snd_pcm_runtime *runtime)
{
	return (runtime->access == SNDRV_PCM_ACCESS_RW_INTERLEAVED ||
		runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED);
}

#define PCM_DIR_NAME(stream) \
	(stream == SNDRV_PCM_STREAM_PLAYBACK ? "Playback" : "Captrue")

static int is_no_pcm_dai(int fe_dai_id)
{
	int ret;

	switch (fe_dai_id) {
	case FE_DAI_ID_VOICE:
	case FE_DAI_ID_FM:
	case FE_DAI_ID_FM_DSP:
	case FE_DAI_ID_CODEC_TEST:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int sprd_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct device *dev = srtd->dev;
	struct sprd_runtime_data *rtd;
	struct i2s_config *config;
	struct snd_pcm *pcm = srtd->pcm;
	int burst_len = 0;
	int hw_chan = 0;
	int ret;
	struct audio_pm_dma *pm_dma;
	u32 size_inout;

	pm_dma = get_pm_dma();

	pr_info("%s Open %s\n", sprd_dai_pcm_name(srtd->cpu_dai),
			PCM_DIR_NAME(substream->stream));

	if (sprd_is_i2s(srtd->cpu_dai)) {
		snd_soc_set_runtime_hwparams(substream, &sprd_i2s_pcm_hardware);
		config = sprd_i2s_dai_to_config(srtd->cpu_dai);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			burst_len = I2S_FIFO_DEPTH - config->tx_watermark;
		else
			burst_len = config->rx_watermark;
		burst_len <<= config->byte_per_chan;
		hw_chan = 1;
	} else {
		snd_soc_set_runtime_hwparams(substream,
			&sprd_pcm_hardware_v1);
		burst_len = VBC_AUDRCD_FULL_WATERMARK * 4;
		hw_chan = 2;
	}

	/*
	 * For mysterious reasons (and despite what the manual says)
	 * playback samples are lost if the DMA count is not a multiple
	 * of the DMA burst size.  Let's add a rule to enforce that.
	 */
	ret = snd_pcm_hw_constraint_step(runtime, 0,
					 SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
					 burst_len);
	if (ret)
		goto out;

	ret = snd_pcm_hw_constraint_step(runtime, 0,
					 SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
					 burst_len);
	if (ret)
		goto out;

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto out;

	ret = -ENOMEM;
	rtd = devm_kzalloc(dev, sizeof(*rtd), GFP_KERNEL);
	if (!rtd)
		goto out;
	runtime->private_data = rtd;
	mutex_lock(&pm_dma->pm_mtx_cnt);
	/* dma related need to access auio dsp sys */
	if (!is_no_pcm_dai(srtd->cpu_dai->id)) {
		ret = agdsp_access_enable();
		if (ret) {
			mutex_unlock(&pm_dma->pm_mtx_cnt);
			pr_err("%s:agdsp_access_enable:error:%d",
				__func__, ret);
			goto out;
		}
		rtd->is_access_enabled = true;
	}
	if (!sprd_is_normal_playback(srtd->cpu_dai->id,
		substream->stream))
		pm_dma->no_pm_cnt++;
	else
		pm_dma->normal_rtd = rtd;
	mutex_unlock(&pm_dma->pm_mtx_cnt);
	if (is_no_pcm_dai(srtd->cpu_dai->id)) {
		ret = 0;
		goto out;
	}
	size_inout = runtime->hw.periods_max * sizeof(struct sprd_dma_cfg);
	if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
		substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		is_normal_cap_use_iram()) {
		rtd->dma_cfg_phy[0] =
			audio_mem_alloc(IRAM_NORMAL_C_LINKLIST_NODE1,
				&size_inout);
		if (!rtd->dma_cfg_phy[0]) {
			pr_err("audio_smem_alloc failed for rtd->dma_cfg_phy[0]");
			ret = -ENOMEM;
			goto err;
		}
	} else {
		rtd->dma_cfg_phy[0] =
			audio_mem_alloc(DDR32, &size_inout);
		if (!rtd->dma_cfg_phy[0]) {
			pr_err("audio_smem_alloc failed for rtd->dma_cfg_phy[0]");
			ret = -ENOMEM;
			goto err;
		}
	}
	rtd->dma_cfg_virt[0] = audio_mem_vmap(
		rtd->dma_cfg_phy[0], size_inout, 1);
	if (!rtd->dma_cfg_virt[0]) {
		pr_err(" ioremap_nocache failed for rtd->dma_cfg_virt[0]");
		goto err;
	}
	memset_io(rtd->dma_cfg_virt[0], 0, size_inout);
	size_inout = runtime->hw.periods_max *
		sizeof(struct sprd_dma_cfg);
	if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
		substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		is_normal_cap_use_iram()) {
		rtd->dma_cfg_phy[1] =
			audio_mem_alloc(IRAM_NORMAL_C_LINKLIST_NODE2,
				&size_inout);
		if (!rtd->dma_cfg_phy[1]) {
			pr_err("audio_smem_alloc failed for rtd->dma_cfg_phy[1]");
			goto err;
		}
	} else {
		rtd->dma_cfg_phy[1] =
			audio_mem_alloc(DDR32, &size_inout);
		if (!rtd->dma_cfg_phy[1]) {
			pr_err("audio_smem_alloc failed for rtd->dma_cfg_phy[1]");
			goto err;
		}
	}
	rtd->dma_cfg_virt[1] = audio_mem_vmap(
		rtd->dma_cfg_phy[1], size_inout, 1);
	if (!rtd->dma_cfg_virt[1]) {
		pr_err("ioremap_nocache failed for rtd->dma_cfg_virt[1]");
		goto err;
	}
	memset_io(rtd->dma_cfg_virt[0], 0, size_inout);
	pr_info("rtd->dma_cfg_virt[0] =%#lx, rtd->dma_cfg_phy[0] =%#lx,",
		(unsigned long)rtd->dma_cfg_virt[0],
		(unsigned long)rtd->dma_cfg_phy[0]);
	pr_info("rtd->dma_cfg_virt[1]=%#lx,rtd->dma_cfg_phy[1]=%#lx,",
		(unsigned long)rtd->dma_cfg_virt[1],
		(unsigned long)rtd->dma_cfg_phy[1]);

	pr_info("runtime->hw.periods_max*(sizeof(struct sprd_dma_cfg)=%zd,",
		runtime->hw.periods_max*(sizeof(struct sprd_dma_cfg)));
	pr_info("sizeof(struct sprd_dma_cfg) = %zd, size_inout=%u\n",
		sizeof(struct sprd_dma_cfg), size_inout);
	/*pmc dma data*/
	ret = sprd_pcm_preallocate_dma_ddr32_buffer(pcm,
		substream->stream);
	rtd->dma_cfg_array = devm_kzalloc(dev, hw_chan * ((
		runtime->hw.periods_max * sizeof(struct scatterlist))
		+ sizeof(struct sprd_dma_cfg)), GFP_KERNEL);
	if (!rtd->dma_cfg_array)
		goto err;

	rtd->dma_pdata = devm_kzalloc(dev, hw_chan
		* sizeof(struct sprd_dma_callback_data), GFP_KERNEL);
	if (!rtd->dma_pdata)
		goto err;

	normal_dma_protect_mutex_lock(substream);
	normal_dma_protect_spin_lock(substream);
	rtd->dma_chn[0] = rtd->dma_chn[1] = NULL;
	rtd->dma_tx_des[0] = rtd->dma_tx_des[1] = NULL;
	rtd->cookie[0] = rtd->cookie[1] = 0;
	normal_dma_protect_spin_unlock(substream);
	normal_dma_protect_mutex_unlock(substream);

	rtd->burst_len = burst_len;
	rtd->hw_chan = hw_chan;
	ret = 0;
	return ret;

err:
	pr_err("ERR:dma_pdata alloc failed!\n");
	mutex_lock(&pm_dma->pm_mtx_cnt);
	/* dma related need to access auio dsp sys */
	if (rtd->is_access_enabled) {
		agdsp_access_disable();
		rtd->is_access_enabled = false;
	}
	mutex_unlock(&pm_dma->pm_mtx_cnt);
	if (rtd->dma_cfg_array) {
		devm_kfree(dev, rtd->dma_cfg_array);
		rtd->dma_cfg_array = NULL;
	}
	if (rtd->dma_cfg_virt[0]) {
		audio_mem_unmap(rtd->dma_cfg_virt[0]);
		rtd->dma_cfg_virt[0] = NULL;
	}
	if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
		substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		is_normal_cap_use_iram()) {
		if (rtd->dma_cfg_phy[0]) {
			audio_mem_free(IRAM_NORMAL_C_LINKLIST_NODE1,
			rtd->dma_cfg_phy[0], 0);
			rtd->dma_cfg_phy[0] = (dma_addr_t)NULL;
		}
	} else {
		if (rtd->dma_cfg_phy[0]) {
			audio_mem_free(DDR32, rtd->dma_cfg_phy[0],
				runtime->hw.periods_max *
				sizeof(struct sprd_dma_cfg));
			rtd->dma_cfg_phy[0] = (dma_addr_t)NULL;
		}
	}
	if (rtd->dma_cfg_virt[1]) {
		audio_mem_unmap(rtd->dma_cfg_virt[1]);
		rtd->dma_cfg_virt[1] = NULL;
	}
	if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
		substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		is_normal_cap_use_iram()) {
		if (rtd->dma_cfg_phy[1]) {
			audio_mem_free(IRAM_NORMAL_C_LINKLIST_NODE2,
				rtd->dma_cfg_phy[1], 0);
			rtd->dma_cfg_phy[1] = (dma_addr_t)NULL;
		}
	} else {
		if (rtd->dma_cfg_phy[1]) {
			audio_mem_free(DDR32, rtd->dma_cfg_phy[1],
				runtime->hw.periods_max *
				sizeof(struct sprd_dma_cfg));
			rtd->dma_cfg_phy[1] = (dma_addr_t)NULL;
		}
	}
	/*dma data buffer free*/
	if (substream->dma_buffer.area) {
		audio_mem_unmap(substream->dma_buffer.area);
		substream->dma_buffer.area = NULL;
	}
	if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
		substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		is_normal_cap_use_iram()) {
		if (substream->dma_buffer.addr) {
			audio_mem_free(IRAM_NORMAL_C_DATA,
				substream->dma_buffer.addr, 0);
			substream->dma_buffer.addr = (dma_addr_t)NULL;
		}
	} else {
		if (substream->dma_buffer.addr) {
			audio_mem_free(DDR32, substream->dma_buffer.addr,
			       substream->dma_buffer.bytes);
			substream->dma_buffer.addr = (dma_addr_t)NULL;
		}
	}
	devm_kfree(dev, rtd);
out:
	return ret;
}

static int sprd_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct device *dev = srtd->dev;
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();
	pr_info("%s Close %s\n", sprd_dai_pcm_name(srtd->cpu_dai),
			PCM_DIR_NAME(substream->stream));
	mutex_lock(&pm_dma->pm_mtx_cnt);
	if (!sprd_is_normal_playback(srtd->cpu_dai->id,
		substream->stream)) {
		if (--pm_dma->no_pm_cnt < 0) {
			pr_warn("%s no_pm_cnt=%d\n",
				__func__, pm_dma->no_pm_cnt);
			pm_dma->no_pm_cnt = 0;
		}
	} else
		pm_dma->normal_rtd = NULL;

	if (rtd && rtd->is_access_enabled) {
		agdsp_access_disable();
		rtd->is_access_enabled = false;
	}
	mutex_unlock(&pm_dma->pm_mtx_cnt);
	if (is_no_pcm_dai(srtd->cpu_dai->id)) {
		devm_kfree(dev, rtd);
		return 0;
	}
	devm_kfree(dev, rtd->dma_pdata);
	devm_kfree(dev, rtd->dma_cfg_array);
	if (rtd->dma_cfg_virt[0]) {
		audio_mem_unmap(rtd->dma_cfg_virt[0]);
		rtd->dma_cfg_virt[0] = NULL;
	}
	if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
		substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		is_normal_cap_use_iram()) {
		if (rtd->dma_cfg_phy[0]) {
			audio_mem_free(IRAM_NORMAL_C_LINKLIST_NODE1,
				rtd->dma_cfg_phy[0], 0);
			rtd->dma_cfg_phy[0] = (dma_addr_t)NULL;
		}
	} else {
		if (rtd->dma_cfg_phy[0]) {
			audio_mem_free(DDR32, rtd->dma_cfg_phy[0],
				runtime->hw.periods_max *
				sizeof(struct sprd_dma_cfg));
			rtd->dma_cfg_phy[0] = (dma_addr_t)NULL;
		}
	}
	if (rtd->dma_cfg_virt[1]) {
		audio_mem_unmap(rtd->dma_cfg_virt[1]);
		rtd->dma_cfg_virt[1] = NULL;
	}
	if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
		substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		is_normal_cap_use_iram()) {
		if (rtd->dma_cfg_phy[1]) {
			audio_mem_free(IRAM_NORMAL_C_LINKLIST_NODE2,
				rtd->dma_cfg_phy[1], 0);
			rtd->dma_cfg_phy[1] = (dma_addr_t)NULL;
		}
	} else {
		if (rtd->dma_cfg_phy[1]) {
			audio_mem_free(DDR32, rtd->dma_cfg_phy[1],
				runtime->hw.periods_max *
				sizeof(struct sprd_dma_cfg));
			rtd->dma_cfg_phy[1] = (dma_addr_t)NULL;
		}
	}
	/*dma data buffer free*/
	if (substream->dma_buffer.area) {
		audio_mem_unmap(substream->dma_buffer.area);
		substream->dma_buffer.area = NULL;
	}
	if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
		substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		is_normal_cap_use_iram()) {
		if (substream->dma_buffer.addr) {
			audio_mem_free(IRAM_NORMAL_C_DATA,
				substream->dma_buffer.addr, 0);
			substream->dma_buffer.addr = (dma_addr_t)NULL;
		}
	} else {
		if (substream->dma_buffer.addr) {
			audio_mem_free(DDR32,
				substream->dma_buffer.addr,
				substream->dma_buffer.bytes);
			substream->dma_buffer.addr = (dma_addr_t)NULL;
		}
	}
	devm_kfree(dev, rtd);

	return 0;
}

static void sprd_pcm_dma_buf_done(void *data)
{
	struct sprd_dma_callback_data *dma_cb_data =
		(struct sprd_dma_callback_data *)data;
	struct snd_pcm_runtime *runtime = dma_cb_data->substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = dma_cb_data->substream->private_data;
	int i = 0;
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();
	if (!rtd->cb_called) {
		rtd->cb_called = 1;
		sp_asoc_pr_info("DMA Callback CALL cpu_dai->id =%d\n",
				srtd->cpu_dai->id);
	}

	if (rtd->hw_chan == 1)
		goto irq_fast;
	normal_dma_protect_spin_lock(dma_cb_data->substream);
	for (i = 0; i < 2; i++) {
		if (dma_cb_data->dma_chn == rtd->dma_chn[i]) {
			rtd->int_pos_update[i] = 1;

			if (rtd->dma_chn[1 - i]) {
				if (rtd->int_pos_update[1 - i]) {
					normal_dma_protect_spin_unlock(
						dma_cb_data->substream);
					goto irq_ready;
				}
			} else {
				normal_dma_protect_spin_unlock(
					dma_cb_data->substream);
				goto irq_ready;
			}
		}
	}
	normal_dma_protect_spin_unlock(dma_cb_data->substream);
	return;
irq_ready:
	rtd->int_pos_update[0] = 0;
	rtd->int_pos_update[1] = 0;
irq_fast:
	snd_pcm_period_elapsed(dma_cb_data->substream);
}

static int sprd_pcm_get_dma_type(struct dma_chan *dma_chn)
{
	const char *cp;
	struct device_node *np = dma_chn->device->dev->of_node;

	if (!np)
		return -ENODEV;

	if (of_property_read_string(np, "compatible", &cp))
		return -EINVAL;

	pr_debug("%s, %s\n", __func__, cp);
	if (1)//strstr(cp, "agcp"))
		return DMA_TYPE_AGCP;
	else
		return DMA_TYPE_INVAL;
}

static enum AUDIO_MEM_TYPE_E sprd_pcm_get_transf_type(
	struct snd_pcm_substream *substream, struct dma_chan *dma_chn)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	int type = sprd_pcm_get_dma_type(dma_chn);

	/*
	 * There is a gap of ddr base address between AP and AGCP.
	 * So there is a need of transformation for the address gets
	 * from dma driver.
	 */
	if (type == DMA_TYPE_AGCP) {
		if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
			substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
			is_normal_cap_use_iram())
			return IRAM_NORMAL_C_DATA;
		return DDR32;
	} else
		return -1;
}

static int sprd_pcm_dma_step_config(struct sprd_pcm_dma_params *dma_data,
				    int playback, int interleaved)
{
	u32 step;
	struct sprd_dma_cfg *desc = &dma_data->desc;

	if (interleaved) {
		sp_asoc_pr_dbg("Interleaved Access\n");
		switch (desc->datawidth) {
		case DMA_SLAVE_BUSWIDTH_2_BYTES:
			step = DMA_SLAVE_BUSWIDTH_4_BYTES;
			break;
		case DMA_SLAVE_BUSWIDTH_4_BYTES:
			step = DMA_SLAVE_BUSWIDTH_8_BYTES;
			break;
		default:
			step = 0;
			pr_err("err: datawidth %u is not supported yet.\n",
			       desc->datawidth);
			break;
		}
		desc->src_step = playback ? step : 0;
		desc->des_step = playback ? 0 : step;
		pr_info("desc->src_step  =%u, des_step=%u, datawidth =%u\n",
		       desc->src_step,
		       desc->des_step,
		       desc->datawidth);
	} else {
		sp_asoc_pr_dbg("Non-interleaved Access\n");
		step = (dma_data->use_mcdt == 1) ?
			DMA_SLAVE_BUSWIDTH_4_BYTES : DMA_SLAVE_BUSWIDTH_2_BYTES;
		desc->src_step = playback ? step : 0;
		desc->des_step = playback ? 0 : step;
		sp_asoc_pr_info("desc->src_step=%d, dest_step=%d\n",
			       desc->src_step,
			       desc->des_step);
	}

	return 0;
}

static int sprd_pcm_request_dma_channel(
	struct snd_pcm_substream *substream, int ch_cnt)
{
	int i;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct dma_chan *dma_chn_request = NULL;
	struct sprd_pcm_dma_params *dma_data = rtd->params;
	struct audio_pm_dma *pm_dma;
	struct dma_chan *temp_dma_chan;

	pm_dma = get_pm_dma();
	if (!dma_data) {
		pr_err("ERR: %s, dma_data is NULL!\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ch_cnt; i++) {
		/*whale dma channel request*/
		if (dma_data->used_dma_channel_name[i] == NULL) {
			pr_err("ERR: dma_data->used_dma_channel_name[i] == NULL\n");
			dma_chn_request = NULL;
			break;
		}

		dma_chn_request = dma_request_slave_channel(srtd->platform->dev,
					 dma_data->used_dma_channel_name[i]);
		if (IS_ERR_OR_NULL(dma_chn_request)) {
			pr_err("ERR: request dma channel(%s) failed!(%ld)\n",
				dma_data->used_dma_channel_name[i],
				PTR_ERR(dma_chn_request));
			dma_chn_request = NULL;
			break;
		}
		normal_dma_protect_mutex_lock(substream);
		normal_dma_protect_spin_lock(substream);
		rtd->dma_chn[i] = dma_chn_request;
		rtd->transform_type = sprd_pcm_get_transf_type(substream,
			dma_chn_request);
		pr_info("Chan%d DMA ID=%d\n",
			       rtd->dma_chn[i]->chan_id,
			       rtd->params->channels[i]);
		normal_dma_protect_spin_unlock(substream);
		normal_dma_protect_mutex_unlock(substream);
	}

	if (!dma_chn_request) {
		pr_err("ERR:PCM Request DMA Error %d\n",
		       dma_data->channels[i]);
		normal_dma_protect_mutex_lock(substream);
		for (i = 0; i < ch_cnt; i++) {
			if (rtd->dma_chn[i]) {
				temp_dma_chan = rtd->dma_chn[i];
				normal_dma_protect_spin_lock(substream);
				rtd->dma_chn[i] = NULL;
				rtd->dma_tx_des[i] = NULL;
				rtd->cookie[i] = 0;
				rtd->params = NULL;
				normal_dma_protect_spin_unlock(substream);
				dma_release_channel(temp_dma_chan);
			}
		}
		normal_dma_protect_mutex_unlock(substream);
		return -ENODEV;
	}

	return 0;
}

static int sprd_pcm_config_dma(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct sprd_dma_cfg **dma_config,
			       dma_addr_t *dma_buff_phys)
{
	int i = 0;
	int j = 0;
	int ch_cnt = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_pcm_dma_params *dma_data;
	struct sprd_dma_cfg **dma_config_ptr = dma_config;
	struct sprd_dma_cfg *cfg;
	size_t totsize = params_buffer_bytes(params);
	size_t period = params_period_bytes(params);
	size_t periods = totsize/period;
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	u32 fragments_len;
	struct i2s_config *config = NULL;
	struct scatterlist *sg = NULL;
	int p_wakeup = !(params->flags & SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP);

	dma_data = snd_soc_dai_get_dma_data(srtd->cpu_dai, substream);
	if (!dma_data) {
		pr_err("ERR: %s dma_data is NULL!\n", __func__);
		return -EINVAL;
	}

	ch_cnt = params_channels(params);
	if (dma_data->use_mcdt == 1)
		ch_cnt = 1;

	if (sprd_is_i2s(srtd->cpu_dai)) {
		u16 wm;

		config = sprd_i2s_dai_to_config(srtd->cpu_dai);
		ch_cnt = rtd->hw_chan;
		wm = is_playback ? (I2S_FIFO_DEPTH - config->tx_watermark) :
			config->rx_watermark;
		fragments_len = wm * dma_data->desc.datawidth;
	} else {
		u32 fact = (dma_data->desc.datawidth ==
			DMA_SLAVE_BUSWIDTH_2_BYTES) ? 2:4;
		fragments_len = dma_data->desc.fragmens_len * fact;
	}

	if (ch_cnt > SPRD_PCM_CHANNEL_MAX) {
		pr_err("ERR: channel count(%d) is greater than %d\n",
		       ch_cnt, SPRD_PCM_CHANNEL_MAX);
		return -EINVAL;
	}

	for (i = 0; i < ch_cnt; i++) {
		cfg = dma_config_ptr[i];
		sg = dma_config_ptr[i]->sg;
		cfg->config.src_addr_width = dma_data->desc.datawidth;
		cfg->config.dst_addr_width = dma_data->desc.datawidth;
		cfg->config.src_maxburst = fragments_len;
		cfg->config.slave_id =  dma_data->channels[i];
		if (is_playback) {
			cfg->config.step = dma_data->desc.src_step;
			cfg->config.src_addr = audio_addr_ap2dsp(DDR32,
				dma_buff_phys[i], 0);
			cfg->config.dst_addr = dma_data->dev_paddr[i];
			cfg->config.direction = DMA_MEM_TO_DEV;
		} else {
			cfg->config.step = dma_data->desc.des_step;
			cfg->config.src_addr = dma_data->dev_paddr[i];
			if (srtd->cpu_dai->id ==
				FE_DAI_ID_NORMAL_AP01 &&
				substream->stream ==
				SNDRV_PCM_STREAM_CAPTURE &&
				is_normal_cap_use_iram())
				cfg->config.dst_addr =
					audio_addr_ap2dsp(
					IRAM_NORMAL_C_DATA,
					dma_buff_phys[i], 0);
			else
				cfg->config.dst_addr = audio_addr_ap2dsp(DDR32,
					dma_buff_phys[i], 0);
			cfg->config.direction = DMA_DEV_TO_MEM;
		}

		if (p_wakeup)
			cfg->dma_config_flag = SPRD_DMA_FLAGS(0,
				0, SPRD_DMA_FRAG_REQ, SPRD_DMA_TRANS_INT);
		else
			cfg->dma_config_flag = SPRD_DMA_FLAGS(0,
				0, SPRD_DMA_FRAG_REQ, SPRD_DMA_NO_INT);

		cfg->ll_cfg.virt_addr = (unsigned long)(rtd->dma_cfg_virt[i]);
		if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
			substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
			is_normal_cap_use_iram())
			cfg->ll_cfg.phy_addr =
				(unsigned long)audio_addr_ap2dsp(
				IRAM_NORMAL_C_LINKLIST_NODE1,
				(rtd->dma_cfg_phy[i]), 0);
		else
			cfg->ll_cfg.phy_addr =
				(unsigned long)audio_addr_ap2dsp(DDR32,
				(rtd->dma_cfg_phy[i]), 0);

		pr_info("src_dw=%d, dst_dw=%d frag=%lx, burst:%d, slave_id=%d, step=%d, ll_cfg_virt_addr=%p, ll_cfg_phy_addr=%ld ,dst_addr:%ld, src_addr:%ld,chan=%d\n",
			(int)cfg->config.src_addr_width,
			(int)cfg->config.dst_addr_width,
			cfg->dma_config_flag,
			(int)cfg->config.src_maxburst, cfg->config.slave_id,
			(int)cfg->config.step, (void *)cfg->ll_cfg.virt_addr,
			(unsigned long)cfg->ll_cfg.phy_addr,
			(unsigned long)cfg->config.dst_addr,
			(unsigned long)cfg->config.src_addr, i);
	}

	do {
		for (i = 0; i < ch_cnt; i++) {
			sg = (struct scatterlist *)((u8 *)
				dma_config_ptr[i]->sg +
				j * sizeof(struct scatterlist));
			if (j == 0) {
				cfg = dma_config_ptr[i];
				sg_init_table(sg, periods);
				cfg->sg_num = periods;
			}
			sg_dma_len(sg) = period/ch_cnt;
			if (is_playback)
				sg_dma_address(sg) = audio_addr_ap2dsp(DDR32,
					dma_buff_phys[i], 0);
			else {
				if (srtd->cpu_dai->id ==
						FE_DAI_ID_NORMAL_AP01 &&
						substream->stream ==
						SNDRV_PCM_STREAM_CAPTURE &&
						is_normal_cap_use_iram())
					sg_dma_address(sg) =
						audio_addr_ap2dsp(
						IRAM_NORMAL_C_DATA,
						dma_buff_phys[i], 0);
				else
					sg_dma_address(sg) = audio_addr_ap2dsp(
						DDR32, dma_buff_phys[i], 0);
			}
			if (rtd->interleaved)
				dma_buff_phys[i] += period;
			else
				dma_buff_phys[i] += (period/ch_cnt);
			pr_info("sg_dma_len=%zu, periods=%zu, sg_dma_address=%p, flag:%lx, chan=%d node=%d\n",
				period, periods, (void *)sg_dma_address(sg),
				(j == 0) ? cfg->dma_config_flag : 0, i, j);
		}
		if (period > totsize)
			period = totsize;
		j++;
	} while (totsize -= period);
	pr_info("Node Size:%d\n", j);

	return 0;
}

static int sprd_pcm_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_pcm_dma_params *dma_data;
	struct sprd_dma_cfg *dma_config_ptr[SPRD_PCM_CHANNEL_MAX];
	struct sprd_dma_callback_data *dma_pdata_ptr[SPRD_PCM_CHANNEL_MAX];
	size_t totsize = params_buffer_bytes(params);
	size_t period = params_period_bytes(params);
	dma_addr_t dma_buff_phys[SPRD_PCM_CHANNEL_MAX];
	struct i2s_config *config = NULL;
	int ret = 0;
	int i = 0;
	int ch_cnt;
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct dma_async_tx_descriptor *tmp_tx_des;
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();

	sp_asoc_pr_info("(pcm) %s, cpudai_id=%d\n", __func__,
			srtd->cpu_dai->id);
	dma_data = snd_soc_dai_get_dma_data(srtd->cpu_dai, substream);
	if (!dma_data)
		goto no_dma;

	ch_cnt = params_channels(params);
	if (dma_data->use_mcdt == 1)
		ch_cnt = 1;
	sp_asoc_pr_info("chan=%d totsize=%zu period=%zu\n", ch_cnt,
			totsize, period);
	if (sprd_is_i2s(srtd->cpu_dai)) {
		config = sprd_i2s_dai_to_config(srtd->cpu_dai);
		ch_cnt = rtd->hw_chan;
	} else {
		rtd->interleaved = (ch_cnt == 2)
		    && sprd_pcm_is_interleaved(runtime);
		sprd_pcm_dma_step_config(dma_data, is_playback,
					 rtd->interleaved);
	}

	if (ch_cnt > SPRD_PCM_CHANNEL_MAX) {
		pr_err("ERR: channel count(%d) is greater than %d\n",
		       ch_cnt, SPRD_PCM_CHANNEL_MAX);
		return -EINVAL;
	}

	/*
	 * this may get called several times by oss emulation
	 * with different params
	 */
	if (rtd->params == NULL) {
		rtd->params = dma_data;
		ret = sprd_pcm_request_dma_channel(substream, ch_cnt);
		if (ret)
			goto hw_param_err;
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	runtime->dma_bytes = totsize;

	rtd->dma_addr_offset = (totsize / ch_cnt);
	/* rtd->dma_addr_offset =  (rtd->dma_addr_offset + 7)&(~7); */
	if (sprd_pcm_is_interleaved(runtime)) {
		if (dma_data->desc.datawidth == DMA_SLAVE_BUSWIDTH_2_BYTES)
			rtd->dma_addr_offset = 2;
		else if (dma_data->desc.datawidth == DMA_SLAVE_BUSWIDTH_4_BYTES)
			rtd->dma_addr_offset = 4;
	}

	normal_dma_protect_mutex_lock(substream);
	normal_dma_protect_spin_lock(substream);
	for (i = 0; i < ch_cnt; i++) {
		dma_config_ptr[i] = (struct sprd_dma_cfg *)
			((u8 *)rtd->dma_cfg_array +
			i * (sizeof(struct sprd_dma_cfg)+
			sizeof(struct scatterlist)*runtime->hw.periods_max));
		memset(dma_config_ptr[i], 0,
			(sizeof(struct sprd_dma_cfg) +
			sizeof(struct scatterlist)*runtime->hw.periods_max));
		dma_pdata_ptr[i] = rtd->dma_pdata + i;
		memset(dma_pdata_ptr[i], 0,
			sizeof(struct sprd_dma_callback_data));
		/* used by dma irq done callback */
		dma_pdata_ptr[i]->dma_chn = rtd->dma_chn[i];
		dma_pdata_ptr[i]->substream = substream;
		dma_buff_phys[i] = runtime->dma_addr + i * rtd->dma_addr_offset;
		dma_config_ptr[i]->sg = (struct scatterlist *)((u8 *) &
			(dma_config_ptr[i]->sg) + sizeof(void *));
		pr_info("dma_buff_phys[%d] %u\n",
			i, (u32)dma_buff_phys[i]);
	}
	normal_dma_protect_spin_unlock(substream);
	normal_dma_protect_mutex_unlock(substream);
	pr_info("%s, block %u\n", __func__, (u32)period / ch_cnt);

	ret = sprd_pcm_config_dma(substream, params, dma_config_ptr,
				  dma_buff_phys);
	if (ret)
		goto hw_param_err;

	normal_dma_protect_mutex_lock(substream);
	/*
	 * if PM_POST_SUSPEND resumed the dma_chn has become null,
	 * so add protected code here.
	 */
	if (sprd_is_normal_playback(srtd->cpu_dai->id, substream->stream) &&
		rtd->dma_chn[0] == NULL) {
		pr_err("%s dam_chan is null for normalplayback\n", __func__);
		normal_dma_protect_mutex_unlock(substream);
		goto hw_param_err;
	}
	for (i = 0; i < ch_cnt; i++) {
		/* config dma channel */
		ret = dmaengine_slave_config(rtd->dma_chn[i],
					     &(dma_config_ptr[i]->config));
		if (ret < 0) {
			pr_err("%s, DMA chan ID %d config is failed!\n",
				__func__, rtd->dma_chn[i]->chan_id);
			normal_dma_protect_mutex_unlock(substream);
			goto hw_param_err;
		}
		/* get dma desc from dma config */
		tmp_tx_des = rtd->dma_chn[i]->device->device_prep_slave_sg(
				rtd->dma_chn[i],
				dma_config_ptr[i]->sg,
				dma_config_ptr[i]->sg_num,
				dma_config_ptr[i]->config.direction,
				dma_config_ptr[i]->dma_config_flag,
				&(dma_config_ptr[i]->ll_cfg));
		if (!tmp_tx_des) {
			pr_err("%s, DMA chan ID %d memcpy is failed!\n",
				__func__, rtd->dma_chn[i]->chan_id);
			normal_dma_protect_mutex_unlock(substream);
			goto hw_param_err;
		}
		normal_dma_protect_spin_lock(substream);
		rtd->dma_tx_des[i] = tmp_tx_des;
		normal_dma_protect_spin_unlock(substream);
		if (!(params->flags & SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP)) {
			pr_info("%s, Register Callback func for DMA chan ID %d\n",
				__func__, rtd->dma_chn[i]->chan_id);
			rtd->dma_tx_des[i]->callback = sprd_pcm_dma_buf_done;
			rtd->dma_tx_des[i]->callback_param =
				(void *)(dma_pdata_ptr[i]);
		}
	}
	normal_dma_protect_mutex_unlock(substream);

	goto ok_go_out;

no_dma:
	pr_info("no dma\n");
	rtd->params = NULL;
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totsize;

	return ret;
hw_param_err:
	pr_err("hw_param_err\n");
ok_go_out:
	pr_err("return %i\n", ret);

	return ret;
}

static int sprd_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct sprd_runtime_data *rtd = substream->runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_pcm_dma_params *dma = rtd->params;
	int i;
	struct dma_chan *temp_dma_chan;
	struct audio_pm_dma *pm_dma;

	sp_asoc_pr_info("%s, %s cpu_dai->id = %d %s\n", __func__,
		sprd_dai_pcm_name(srtd->cpu_dai), srtd->cpu_dai->id,
		PCM_DIR_NAME(substream->stream));
	pm_dma = get_pm_dma();
	snd_pcm_set_runtime_buffer(substream, NULL);

	if (dma) {
		normal_dma_protect_mutex_lock(substream);
		for (i = 0; i < rtd->hw_chan; i++) {
			if (rtd->dma_chn[i]) {
				temp_dma_chan = rtd->dma_chn[i];
				pr_info("%s release id=%d\n",
					__func__, rtd->dma_chn[i]->chan_id);
				normal_dma_protect_spin_lock(substream);
				rtd->dma_chn[i] = NULL;
				rtd->dma_tx_des[i] = NULL;
				rtd->cookie[i] = 0;
				normal_dma_protect_spin_unlock(substream);
				dma_release_channel(temp_dma_chan);
			}
		}
		rtd->params = NULL;
		normal_dma_protect_mutex_unlock(substream);
	}

	return 0;
}

static int sprd_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int sprd_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct sprd_runtime_data *rtd = substream->runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_pcm_dma_params *dma = rtd->params;
	int ret = 0;
	int i;
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();
	sp_asoc_pr_info("%s, %s cpu_dai->id = %d Trigger %s cmd:%d\n", __func__,
			sprd_dai_pcm_name(srtd->cpu_dai), srtd->cpu_dai->id,
			PCM_DIR_NAME(substream->stream), cmd);
	if (!dma) {
		sp_asoc_pr_info("no trigger");
		return 0;
	}
	/*
	 * if dma_tx_des[0] or rtd->dma_chn[0] is NULL, it must be mandatory
	 * released by sprd_pcm_pm_notifier, in this case I will return -ENODATA
	 * to the caller, and hal got a error code will restart the pcm.
	 */
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		normal_dma_protect_spin_lock(substream);
		if (rtd->dma_tx_des[0] == NULL) {
			normal_dma_protect_spin_unlock(substream);
			pr_info("%s dma_tx_des[0] is null ret=%#x, %d\n",
				__func__, -ENODATA, -ENODATA);
			return -ENODATA;
		}
		for (i = 0; i < rtd->hw_chan; i++) {
			if (rtd->dma_tx_des[i])
				rtd->cookie[i] = dmaengine_submit(
					rtd->dma_tx_des[i]);
		}

		for (i = 0; i < rtd->hw_chan; i++) {
			if (rtd->dma_chn[i])
				dma_async_issue_pending(rtd->dma_chn[i]);
		}

		pr_info("pcm Start\n");
		normal_dma_protect_spin_unlock(substream);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		normal_dma_protect_spin_lock(substream);
		pr_info("pcm Stop\n");
		if (rtd->dma_chn[0] == NULL) {
			normal_dma_protect_spin_unlock(substream);
			pr_info("%s dma_chn[0] is null ret=%#x, %d\n",
				__func__, -ENODATA, -ENODATA);
			return -ENODATA;
		}
		for (i = 0; i < rtd->hw_chan; i++) {
			if (rtd->dma_chn[i])
				dmaengine_pause(rtd->dma_chn[i]);
		}
		rtd->cb_called = 0;
		pr_info("pcm E\n");
		normal_dma_protect_spin_unlock(substream);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int debug_pointer_log;
static snd_pcm_uframes_t sprd_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	snd_pcm_uframes_t x;
	int now_pointer;
	int bytes_of_pointer = -1;
	int shift = 1;
	enum AUDIO_MEM_TYPE_E tranf = rtd->transform_type;
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();

	if (rtd->interleaved)
		shift = 0;
	normal_dma_protect_spin_lock(substream);
	if (rtd->dma_chn[0]) {
		now_pointer = sprd_pcm_dma_get_addr(rtd->dma_chn[0],
					rtd->cookie[0], substream);
		if (debug_pointer_log)
			pr_info("now_pointer 1:%zu,dsp:%zx\n",
				(size_t)now_pointer,
				(size_t)audio_addr_ap2dsp(tranf,
				now_pointer, 1));
		now_pointer = ((tranf < 0) ? now_pointer :
			audio_addr_ap2dsp(tranf, now_pointer, 1))
			- runtime->dma_addr;
		bytes_of_pointer = now_pointer;
		if (debug_pointer_log)
			pr_info("now_pointer 2:%zx,runtime->dma_addr:%zx\n",
				(size_t)now_pointer, (size_t)runtime->dma_addr);
	}
	if (rtd->dma_chn[1]) {
		now_pointer = sprd_pcm_dma_get_addr(rtd->dma_chn[1],
					rtd->cookie[1], substream);
		if (debug_pointer_log)
			pr_info("now_pointer 21:%zu,dsp:%zx\n",
				(size_t)now_pointer,
				(size_t)audio_addr_ap2dsp(tranf,
				now_pointer, 1));

		now_pointer = ((tranf < 0) ? now_pointer :
			audio_addr_ap2dsp(tranf, now_pointer, 1)) -
			runtime->dma_addr -
			rtd->dma_addr_offset;
		if (debug_pointer_log)
			pr_info("now_pointer 22:%zx,runtime->dma_addr:%zx\n",
				(size_t)now_pointer, (size_t)runtime->dma_addr);
		if (bytes_of_pointer == -1) {
			bytes_of_pointer = now_pointer;
		} else {
			if (!rtd->dma_pos_wrapped[0] &&
				bytes_of_pointer < rtd->dma_pos_pre[0])
				rtd->dma_pos_wrapped[0] = 1;
			if (!rtd->dma_pos_wrapped[1] && now_pointer < rtd->dma_pos_pre[1])
				rtd->dma_pos_wrapped[1] = 1;

			if (rtd->dma_pos_wrapped[0] &&
				rtd->dma_pos_wrapped[1]) {
				rtd->dma_pos_wrapped[0] = 0;
				rtd->dma_pos_wrapped[1] = 0;
			}

			rtd->dma_pos_pre[0] = bytes_of_pointer;
			rtd->dma_pos_pre[1] = now_pointer;
			if (rtd->dma_pos_wrapped[0] != rtd->dma_pos_wrapped[1])
				bytes_of_pointer = max(
					bytes_of_pointer, now_pointer) << shift;
			else
				bytes_of_pointer = min(
					bytes_of_pointer, now_pointer) << shift;
		}
		if (debug_pointer_log)
			pr_info("now_pointer 31:%zx\n", (size_t)now_pointer);
	}
	normal_dma_protect_spin_unlock(substream);
	x = bytes_to_frames(runtime, bytes_of_pointer);
	if (debug_pointer_log)
		pr_info("x:%zx\n", (size_t)x);

	if (x == runtime->buffer_size)
		x = 0;
	if (debug_pointer_log)
		pr_info("x out:%zx\n", (size_t)x);
	return x;
}

static int sprd_pcm_mmap(struct snd_pcm_substream *substream,
			 struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	return remap_pfn_range(vma, vma->vm_start,
			       runtime->dma_addr >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static struct snd_pcm_ops sprd_pcm_ops = {
	.open = sprd_pcm_open,
	.close = sprd_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = sprd_pcm_hw_params,
	.hw_free = sprd_pcm_hw_free,
	.prepare = sprd_pcm_prepare,
	.trigger = sprd_pcm_trigger,
	.pointer = sprd_pcm_pointer,
	.mmap = sprd_pcm_mmap,
};

static u32 test_ddr_buffer_size;

static int sprd_pcm_preallocate_dma_ddr32_buffer(struct snd_pcm *pcm,
						 int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = AUDIO_BUFFER_BYTES_MAX;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	if (sprd_is_i2s(rtd->cpu_dai))
		size = I2S_BUFFER_BYTES_MAX;
	else
		if (!is_normal_cap_use_iram() && test_ddr_buffer_size)
			size = test_ddr_buffer_size;
	pr_info("%s alloc size = %#zx for data\n",
		__func__, size);
	buf->private_data = NULL;
	if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
		substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
			is_normal_cap_use_iram()) {
		buf->addr = audio_mem_alloc(IRAM_NORMAL_C_DATA,
			(unsigned int *)&size);
		if (!buf->addr) {
			pr_err("%s audio_mem_alloc failed for buf->addr\n",
				__func__);
			return -ENOMEM;
		}
	} else {
		buf->addr = audio_mem_alloc(DDR32, (unsigned int *)&size);
		if (!buf->addr) {
			pr_err("%s audio_mem_alloc failed for buf->addr\n",
				__func__);
			return -ENOMEM;
		}
	}

	buf->area = audio_mem_vmap(buf->addr, size, 1);
	if (!buf->area) {
		pr_err("%s audio_mem_vmap failed for buf->addr", __func__);
		if (srtd->cpu_dai->id == FE_DAI_ID_NORMAL_AP01 &&
			substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
				is_normal_cap_use_iram())
			audio_mem_free(IRAM_NORMAL_C_DATA, buf->addr, 0);
		else
			audio_mem_free(DDR32, buf->addr, size);
		return -ENOMEM;
	}

	pr_info("%s, sprd_pcm_preallocate_dma_buffer buf->area(virt)=%#lx, buf->addr(phy)=%#x, size=%zu\n",
			__func__, (unsigned long)buf->area,
			(unsigned int)buf->addr, size);

	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	return 0;
}

static int sprd_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	sp_asoc_pr_dbg("%s %s\n", __func__, sprd_dai_pcm_name(cpu_dai));

	if (!card->dev->dma_mask)
		dma_set_mask(card->dev, DMA_BIT_MASK(64));
#ifdef CONFIG_ARM64
	card->dev->coherent_dma_mask = DMA_BIT_MASK(64);
#else
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);
#endif
	sp_asoc_pr_dbg("return %i\n", ret);

	return ret;
}

static void sprd_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	sp_asoc_pr_dbg("%s\n", __func__);
}

#ifdef CONFIG_PROC_FS

int test_force_stop;
static void sprd_pcm_platform_proc_write(struct snd_info_entry *entry,
					 struct snd_info_buffer *buffer)
{
	char line[64] = { 0 };
	char name[128] = { 0 };
	unsigned int val;

	while (!snd_info_get_line(buffer, line, sizeof(line))) {
		if (sscanf(line, "%s %x", name, &val) != 2)
			continue;
		if (strcmp(name, "t_ddr_size") == 0) {
			test_ddr_buffer_size = val;
			pr_info("test_ddr_buffer_size =%#x\n",
				test_ddr_buffer_size);
		}
		if (strcmp(name, "t_use_iram") == 0) {
			test_normal_cap_use_iram = val;
			pr_info("test_normal_cap_use_iram =%#x\n",
				test_normal_cap_use_iram);
		}
		if (strcmp(name, "t_force_stop") == 0) {
			test_force_stop = val;
			pr_info("test_force_stop =%#x\n", test_force_stop);
		}
		if (strcmp(name, "debug_pointer_log") == 0) {
			debug_pointer_log = val;
			pr_info("debug_pointer_log=%#x\n", debug_pointer_log);
		}
	}
}

static void sprd_pcm_platform_proc_read(struct snd_info_entry *entry,
					struct snd_info_buffer *buffer)
{
	snd_iprintf(buffer, "test_ddr_buffer_size %#x\n", test_ddr_buffer_size);
	snd_iprintf(buffer, "test_normal_cap_use_iram %#x\n",
		test_normal_cap_use_iram);
	snd_iprintf(buffer, "debug_pointer_log %#x\n", debug_pointer_log);
}

static void sprd_pcm_platform_proc_init(struct snd_soc_platform *platform)
{
	struct snd_info_entry *entry;

	if (!snd_card_proc_new(platform->component.card->snd_card,
			       "sprd-dmaengine", &entry))
		snd_info_set_text_ops(entry, platform,
				      sprd_pcm_platform_proc_read);
	entry->c.text.write = sprd_pcm_platform_proc_write;
	entry->mode |= 0200;

}

#else /* !CONFIG_PROC_FS */
static inline void
sprd_pcm_platform_proc_init(struct snd_soc_platform *platform)
{
}
#endif

static int sprd_snd_platform_probe(struct snd_soc_platform *platform)
{
	sprd_pcm_platform_proc_init(platform);

	return 0;
}

static int sprd_snd_platform_remove(struct snd_soc_platform *platform)
{
	return 0;
}


static struct snd_soc_platform_driver sprd_soc_platform = {
	.ops = &sprd_pcm_ops,
	.probe = sprd_snd_platform_probe,
	.remove = sprd_snd_platform_remove,
	.pcm_new = sprd_pcm_new,
	.pcm_free = sprd_pcm_free_dma_buffers,
};

static void pm_normal_dma_chan_release(struct sprd_runtime_data *rtd)
{
	int i;
	struct dma_chan *temp_dma_chan;
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();
	if (!rtd) {
		pr_warn("%s, rtd is null\n", __func__);
		return;
	}
	/* mutex lock and spinlock is normal only if enter this function */
	mutex_lock(&pm_dma->pm_mtx_dma_prot);
	for (i = 0; i < rtd->hw_chan; i++) {
		if (rtd->dma_chn[i]) {
			pr_info("%s, release chan_id %d\n",
				__func__, rtd->dma_chn[i]->chan_id);
			temp_dma_chan = rtd->dma_chn[i];
			spin_lock(&pm_dma->pm_splk_dma_prot);
			rtd->dma_chn[i] = NULL;
			rtd->dma_tx_des[i] = NULL;
			rtd->cookie[i] = 0;
			spin_unlock(&pm_dma->pm_splk_dma_prot);
			dma_release_channel(temp_dma_chan);
		} else
			pr_info("%s i=%d has released\n", __func__, i);
	}
	mutex_unlock(&pm_dma->pm_mtx_dma_prot);
}

/* notify function only care dma does not care vbc */
static int sprd_pcm_pm_notifier(struct notifier_block *notifier,
				unsigned long pm_event, void *unused)
{
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pr_info("%s, PM_SUSPEND_PREPARE.\n", __func__);
		mutex_lock(&pm_dma->pm_mtx_cnt);
		if (pm_dma->no_pm_cnt == 0) {
			pm_normal_dma_chan_release(pm_dma->normal_rtd);
			if (pm_dma->normal_rtd &&
				pm_dma->normal_rtd->is_access_enabled) {
				agdsp_access_disable();
				pm_dma->normal_rtd->is_access_enabled = false;
			}
		}
		mutex_unlock(&pm_dma->pm_mtx_cnt);
		break;
	case PM_POST_SUSPEND:
		pr_info("%s, PM_POST_SUSPEND.\n", __func__);
		/*
		 * Just resum something about vbc. When system has resumed,
		 * HAL will get a xrun, and a 'close -> re-open' procedure
		 * will be done. Then the playback will be restored.
		 */
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct audio_pm_dma *get_pm_dma(void)
{
	return pm_dma;
}

static int init_pm_dma(void)
{
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();

	/* Prepare for pm actions */
	spin_lock_init(&pm_dma->pm_splk_dma_prot);
	mutex_init(&pm_dma->pm_mtx_dma_prot);
	mutex_init(&pm_dma->pm_mtx_cnt);
	pm_dma->pm_nb.notifier_call = sprd_pcm_pm_notifier;
	if (register_pm_notifier(&pm_dma->pm_nb))
		pr_warn("Register pm notifier error!\n");

	return 0;
}

static int sprd_soc_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;

	if (node == NULL) {
		pr_err("ERR: device_node is NULL!\n");
		return -1;
	}

	pm_dma = devm_kzalloc(&(pdev->dev), sizeof(*pm_dma), GFP_KERNEL);
	if (!pm_dma)
		return -ENOMEM;
	init_pm_dma();

	return snd_soc_register_platform(&pdev->dev, &sprd_soc_platform);
}

static int sprd_soc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sprd_pcm_of_match[] = {
	{.compatible = "sprd,sharkl5-pcm-platform",},
	{.compatible = "sprd,roc1-pcm-platform",},
	{},
};

MODULE_DEVICE_TABLE(of, sprd_pcm_of_match);
#endif

static struct platform_driver sprd_pcm_driver = {
	.driver = {
		.name = "sprd-pcm-audio",
		.owner = THIS_MODULE,
		.of_match_table = sprd_pcm_of_match,
	},

	.probe = sprd_soc_platform_probe,
	.remove = sprd_soc_platform_remove,
};

module_platform_driver(sprd_pcm_driver);

MODULE_DESCRIPTION("SPRD ASoC PCM DMA");
MODULE_AUTHOR("Jian chen <jian.chen@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sprd-audio");
