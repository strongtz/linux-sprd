/*
 * sound/soc/sprd/dai/sprd-dmaengine-pcm.c
 *
 * SpreadTrum DMA for the pcm stream.
 *
 * Copyright (C) 2015 SpreadTrum Ltd.
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
#include <linux/io.h>
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

#include "dfm.h"
#include "sprd-audio.h"
#include "sprd-asoc-common.h"
#include "sprd-2stage-dmaengine-pcm.h"
#include "sprd-i2s.h"
#include "vaudio.h"

#define SPRD_PCM_CHANNEL_MAX 2

#define VB_AUDRCD_FULL_WATERMARK 160

/* periodsize(9600) * periodcount (6) * chann (2) * bytes(2) =
 * 225 * 1024 and aligned 4k, so set to 228k
 */
#define DEEPBUFFER_PLAYBACK_BUFFER_BYTES_MAX		(228 * 1024)
#define NORMAL_PLAYBACK_BUFFER_BYTES_MAX            (64 * 1024)
#define NORMAL_CAPTURE_BUFFER_BYTES_MAX            (64 * 1024)
#define I2S_BUFFER_BYTES_MAX	(64 * 1024)
/* 0x0 */
#define OFFSET_RESDDR_NORMAL_PLY 0
/* 0x10000 */
#define OFFSET_RESDDR_NORMAL_CAP (OFFSET_RESDDR_NORMAL_PLY +\
	NORMAL_PLAYBACK_BUFFER_BYTES_MAX)
/* 0x20000 */
#define OFFSET_RESDDR_DEEPBUF_PLY (OFFSET_RESDDR_NORMAL_CAP +\
	NORMAL_CAPTURE_BUFFER_BYTES_MAX)
/* 0x59000 */
#define OFFSET_RESDDR_IIS_PLY (OFFSET_RESDDR_DEEPBUF_PLY +\
	DEEPBUFFER_PLAYBACK_BUFFER_BYTES_MAX)
/* 0x69000 */
#define OFFSET_RESDDR_IIS_CAP (OFFSET_RESDDR_IIS_PLY +\
	I2S_BUFFER_BYTES_MAX)

/* 0x400 */
#define RESDDR_LLST_ONE_CHAN_SIZE 1024
/* 0x800 */
#define RESDDR_LLST_SIZE (RESDDR_LLST_ONE_CHAN_SIZE * 2)

/* 0x79000 */
#define OFFSET_RESDDR_LLST_NORMAL_PLY (OFFSET_RESDDR_IIS_CAP +\
	I2S_BUFFER_BYTES_MAX)
/* 0x79800 */
#define OFFSET_RESDDR_LLST_NORMAL_CAP (OFFSET_RESDDR_LLST_NORMAL_PLY +\
	RESDDR_LLST_SIZE)
/* 0x7a000 */
#define OFFSET_RESDDR_LLST_DEEPBUF_PLY (OFFSET_RESDDR_LLST_NORMAL_CAP +\
	RESDDR_LLST_SIZE)
/* 0x7a800 */
#define OFFSET_RESDDR_LLST_IIS_PLY (OFFSET_RESDDR_LLST_DEEPBUF_PLY +\
	RESDDR_LLST_SIZE)
/* 0x7b000 */
#define OFFSET_RESDDR_LLST_IIS_CAP (OFFSET_RESDDR_LLST_IIS_PLY +\
	RESDDR_LLST_SIZE)

#include "audio_mem.h"

#ifndef DMA_LINKLIST_CFG_NODE_SIZE
#define DMA_LINKLIST_CFG_NODE_SIZE  (sizeof(struct sprd_dma_cfg))
#endif

#ifndef sprd_is_normal_playback
#define sprd_is_normal_playback(cpu_dai, stream) (false)
#endif

struct sprd_dma_callback_data {
	struct snd_pcm_substream *substream;
	struct dma_chan *dma_chn;
};

struct sprd_runtime_data {
	int dma_addr_offset;
	struct sprd_pcm_dma_params *params;
	struct dma_chan *dma_chn[2];
	void *dma_callback1_func[2];
	struct sprd_dma_cfg *dma_config_ptr[SPRD_PCM_CHANNEL_MAX];
	struct sprd_dma_callback_data *dma_cb_ptr[SPRD_PCM_CHANNEL_MAX];
	dma_addr_t dma_linklist_cfg_phy[2];
	void *dma_linklist_cfg_virt[2];
	struct dma_async_tx_descriptor *dma_tx_des[2];
	dma_cookie_t cookie[2];
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
	int hw_chan2;
	s32 interleaved2;
	struct dma_chan *dma_chn2;
	struct sprd_dma_cfg *dma_cfg_buf2;
	void *dma_callback2_func;
	struct sprd_dma_callback_data *dma_callback_data2;
	dma_addr_t dma_linklist_cfg_phy2;
	void *dma_linklist_cfg_virt2;
	u32 linklist_node_size;
	struct dma_async_tx_descriptor *dma_tx_des2;
	dma_cookie_t cookie2;
	int int_pos_update2;
	int dma_pos_pre2;
	u32 iram_phy_addr_ap;
	u32 iram_phy_addr_dma;
	u8 *iram_virt_addr;
	u32 iram_data_max_s2_1;
	u32 pointer2_step_bytes;
	u32 pointer2_step_max;
	u32 node_size_level1;
	u32 node_cfg_count_2;
};

static struct audio_pm_dma *pm_dma;

enum {
	DMA_CHAN_PLAYBACK0,
	DMA_CHAN_PLAYBACK1,
	DMA_CHAN_CAPTURE0,
	DMA_CHAN_CAPTURE1,
	DMA_CHAN_I2S_PLAYBACK0,
	DMA_CHAN_I2S_PLAYBACK1,
	DMA_CHAN_I2S_CAPTURE0,
	DMA_CHAN_I2S_CAPTURE1,
	DMA_CHAN_MAX
};

struct dma_chan_index_name {
	int index;
	const char *name;
};

#define SPRD_AUDIO_DMA_NODE_SIZE (1024)

static struct dma_chan *dma_chan[DMA_CHAN_MAX] = { NULL };

#define SPRD_SNDRV_PCM_INFO_COMMON ( \
	SNDRV_PCM_INFO_MMAP | \
	SNDRV_PCM_INFO_MMAP_VALID | \
	SNDRV_PCM_INFO_INTERLEAVED | \
	SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME)
#define SPRD_SNDRV_PCM_FMTBIT (SNDRV_PCM_FMTBIT_S16_LE | \
			       SNDRV_PCM_FMTBIT_S24_LE)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
int sprd_lightsleep_disable(const char *id, int disalbe)
__attribute__ ((weak, alias("__sprd_lightsleep_disable")));

u32 audio_addr_ap2dsp(enum AUDIO_MEM_TYPE_E mem_type, u32 addr,
		      bool invert)
__attribute__ ((weak, alias("__audio_addr_ap2dsp")));
static u32 __audio_addr_ap2dsp(enum AUDIO_MEM_TYPE_E mem_type,
			       u32 addr, bool invert)
{
	pr_debug("%s\n", __func__);
	return 0;
}

u32 audio_mem_alloc(enum AUDIO_MEM_TYPE_E mem_type, u32 *size_inout)
__attribute__ ((weak, alias("__audio_mem_alloc")));
static u32 __audio_mem_alloc(enum AUDIO_MEM_TYPE_E mem_type,
			     u32 *size_inout)
{
	pr_debug("%s\n", __func__);
	return 0;
}

u32 audio_mem_alloc_dsp(enum AUDIO_MEM_TYPE_E mem_type,
			u32 *size_inout)
__attribute__ ((weak, alias("__audio_mem_alloc_dsp")));
static u32 __audio_mem_alloc_dsp(enum AUDIO_MEM_TYPE_E mem_type,
				 u32 *size_inout)
{
	pr_debug("%s\n", __func__);
	return 0;
}

void audio_mem_free(enum AUDIO_MEM_TYPE_E mem_type, u32 addr,
		    u32 size)
__attribute__ ((weak, alias("__audio_mem_free")));
static void __audio_mem_free(enum AUDIO_MEM_TYPE_E mem_type, u32 addr,
			     u32 size)
{
	pr_debug("%s\n", __func__);
}

void *audio_mem_vmap(phys_addr_t start, size_t size, int noncached)
__attribute__ ((weak, alias("__audio_mem_vmap")));
static void *__audio_mem_vmap(phys_addr_t start, size_t size, int noncached)
{
	pr_debug("%s\n", __func__);
	return 0;
}

void audio_mem_unmap(const void *mem)
__attribute__ ((weak, alias("__audio_mem_unmap")));
static void __audio_mem_unmap(const void *mem)
{
	pr_debug("%s\n", __func__);
}

void vbc_clock_set_force(void)
__attribute__ ((weak, alias("__vbc_clock_set_force")));
static void __vbc_clock_set_force(void)
{
	pr_info("no vbc_clock_set_force\n");
}

void vbc_clock_clear_force(void)
__attribute__ ((weak, alias("__vbc_clock_clear_force")));
static void __vbc_clock_clear_force(void)
{
	pr_info("no vbc_clock_clear_force\n");
}

static int __sprd_lightsleep_disable(const char *id, int disable)
{
	sp_asoc_pr_dbg("NO lightsleep control function %d\n", disable);
	return 0;
}
#pragma GCC diagnostic pop

static void sprd_pcm_proc_init(struct snd_pcm_substream *substream);
static void sprd_pcm_proc_done(struct snd_pcm_substream *substream);

static inline int sprd_is_vaudio(struct snd_soc_dai *cpu_dai)
{
	return ((cpu_dai->driver->id == VAUDIO_MAGIC_ID) ||
		(cpu_dai->driver->id == VAUDIO_MAGIC_ID + 1));
}

static inline int sprd_is_i2s(struct snd_soc_dai *cpu_dai)
{
	return (cpu_dai->driver->id == I2S_MAGIC_ID);
}

static inline int sprd_is_dfm(struct snd_soc_dai *cpu_dai)
{
	return (cpu_dai->driver->id == DFM_MAGIC_ID);
}

static inline u32 sprd_pcm_dma_get_addr(struct dma_chan *dma_chn,
					dma_cookie_t cookie,
					struct snd_pcm_substream *substream)
{
	struct dma_tx_state dma_state;

	dmaengine_tx_status(dma_chn, cookie, &dma_state);

	return dma_state.residue;
}

static inline const char *sprd_dai_pcm_name(struct snd_soc_dai *cpu_dai)
{
	if (sprd_is_i2s(cpu_dai))
		return "I2S";
	else if (sprd_is_vaudio(cpu_dai))
		return "VAUDIO";
	else if (sprd_is_dfm(cpu_dai))
		return "DFM";
	return "VBC";
}

static struct audio_pm_dma *get_pm_dma(void);
static void normal_dma_protect_spin_lock(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct audio_pm_dma *pm_dma;

	if (!srtd)
		return;

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

	if (!srtd)
		return;

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

	if (!srtd)
		return;

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

	if (!srtd)
		return;

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

static bool is_use_resddr(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *srtd = substream->pcm->private_data;
	struct platform_pcm_priv *priv_data;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s priv_data is null\n", __func__);
		return 0;
	}

	return priv_data->dma_use_ddr_reserved;
}

enum RESERVED_DDR_CASE {
	DDR_NORMAL_PLY,
	DDR_NORMAL_CAP,
	DDR_DEEPBUF_PLY,
	DDR_IIS_PLY,
	DDR_IIS_CAP,
	DDR_RESERVED_CASE_MAX,
};

static int to_ddr_enum(struct snd_soc_dai *cpu_dai, int stream)
{
	int ddr_enum;
	bool is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK;

	switch (cpu_dai->id) {
	case VBC_DAI_NORMAL:
	case VBC_DAI_AD23:
		ddr_enum = is_playback ? DDR_NORMAL_PLY : DDR_NORMAL_CAP;
		break;
	case VBC_DAI_DEEP_BUF:
		ddr_enum = DDR_DEEPBUF_PLY;
		break;
	case I2S_MAGIC_ID:
		ddr_enum = is_playback ? DDR_IIS_PLY : DDR_IIS_CAP;
		break;
	default:
		pr_err("%s invalid cpu_dai_id=%d\n", __func__, cpu_dai->id);
		ddr_enum = DDR_RESERVED_CASE_MAX;
	}

	return ddr_enum;
}

static void reset_dma_level1_int_count(struct snd_pcm_substream *substream,
				       int normal);

static int alloc_linklist_cfg_resddr(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *sprd_rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = srtd->cpu_dai;
	struct platform_pcm_priv *priv_data;
	u32 resddr_phy_base;
	char *resddr_virt_base;
	u32 offset;
	int ddr_enum;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s priv_data is null\n", __func__);
		return -EINVAL;
	}
	ddr_enum = to_ddr_enum(cpu_dai, substream->stream);
	resddr_virt_base = priv_data->resddr_virt_addr;
	resddr_phy_base = priv_data->resddr_phy_addr;

	switch (ddr_enum) {
	case DDR_NORMAL_PLY:
		offset = OFFSET_RESDDR_LLST_NORMAL_PLY;
		break;
	case DDR_NORMAL_CAP:
		offset = OFFSET_RESDDR_LLST_NORMAL_CAP;
		break;
	case DDR_IIS_PLY:
		offset = OFFSET_RESDDR_LLST_IIS_PLY;
		break;
	case DDR_IIS_CAP:
		offset = OFFSET_RESDDR_LLST_IIS_CAP;
		break;
	default:
		pr_err("%s invalid ddr_enum=%d\n", __func__, ddr_enum);
		return -EINVAL;
	}

	sprd_rtd->dma_linklist_cfg_virt[0] = resddr_virt_base + offset;
	sprd_rtd->dma_linklist_cfg_phy[0] = resddr_phy_base + offset;
	sprd_rtd->dma_linklist_cfg_virt[1]
		= sprd_rtd->dma_linklist_cfg_virt[0] +
		RESDDR_LLST_ONE_CHAN_SIZE;
	sprd_rtd->dma_linklist_cfg_phy[1] = sprd_rtd->dma_linklist_cfg_phy[0] +
		RESDDR_LLST_ONE_CHAN_SIZE;
	memset_io(sprd_rtd->dma_linklist_cfg_virt[0], 0, RESDDR_LLST_SIZE);
	pr_info("%s linklist cfg vir[0] =%#lx, phy[0] =%#lx, cfg vir[1] =%#lx, phy[1] =%#lx\n",
		__func__, (unsigned long)sprd_rtd->dma_linklist_cfg_virt[0],
		(unsigned long)sprd_rtd->dma_linklist_cfg_phy[0],
		(unsigned long)sprd_rtd->dma_linklist_cfg_virt[1],
		(unsigned long)sprd_rtd->dma_linklist_cfg_phy[1]);

	return 0;
}

static int pcm_alloc_dma_linklist_cfg_ddr(
	struct snd_pcm_substream *substream, u32 linklist_node_size)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *sprd_rtd = runtime->private_data;
	int chan_cnt, i;

	if (is_use_resddr(substream)) {
		alloc_linklist_cfg_resddr(substream);
		return 0;
	}

	chan_cnt = sprd_rtd->hw_chan;
	for (i = 0; i < chan_cnt; i++) {
		sprd_rtd->dma_linklist_cfg_virt[i] =
			(void *)dmam_alloc_coherent(substream->pcm->card->dev,
			linklist_node_size,
			&sprd_rtd->dma_linklist_cfg_phy[i],
			GFP_KERNEL);
		if (!sprd_rtd->dma_linklist_cfg_virt[i])
			return -ENOMEM;
		pr_debug("%s chan=%d linklist cfg vir =%#lx, phy =%#lx, node_size =%lu\n ",
			 __func__, i,
			 (unsigned long)sprd_rtd->dma_linklist_cfg_virt[i],
	     (unsigned long)sprd_rtd->dma_linklist_cfg_phy[i],
	     (unsigned long)linklist_node_size);
	}

	return 0;
}

static s32
pcm_free_dma_linklist_cfg_ddr(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *sprd_rtd = runtime->private_data;
	u32 linklist_node_size = sprd_rtd->linklist_node_size;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct platform_pcm_priv *priv_data;
	int chan_cnt = 0;
	int i = 0;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s priv_data is null\n", __func__);
		return -EINVAL;
	}
	/* reserved ddr need not release */
	if (is_use_resddr(substream)) {
		sprd_rtd->dma_linklist_cfg_virt[0] = NULL;
		sprd_rtd->dma_linklist_cfg_virt[1] = NULL;
		return 0;
	}

	chan_cnt = sprd_rtd->hw_chan;
	for (i = 0; i < chan_cnt; i++) {
		pr_debug("%s %d, node_size=%#x\n",
			 __func__, __LINE__, linklist_node_size);
		if (sprd_rtd->dma_linklist_cfg_virt[i]) {
			dmam_free_coherent(substream->pcm->card->dev,
					   linklist_node_size,
					   sprd_rtd->dma_linklist_cfg_virt[i],
					   sprd_rtd->dma_linklist_cfg_phy[i]);
			sprd_rtd->dma_linklist_cfg_virt[i] = NULL;
		}
	}

	return 0;
}

static int pcm_set_dma_linklist_cfg_iram_s1(struct snd_pcm_substream
					      *substream,
					      u32 linklist_node_size,
					      int normal)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *sprd_rtd = runtime->private_data;
	size_t offset = 0;
	dma_addr_t phy_addr_base = 0;
	char *virt_addr_base = 0;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct platform_pcm_priv *priv_data = NULL;
	int chan_cnt = 0;
	int i = 0;

	chan_cnt = sprd_rtd->hw_chan;
	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_warn("priv_data failed\n");
		return -1;
	}
	if (normal == PLY_NORMAL) {
		phy_addr_base = priv_data->iram_normal_phy_addr;
		virt_addr_base = priv_data->iram_normal_virt_addr;
		offset = priv_data->iram_normal_size - (2 * linklist_node_size);
		pr_debug("%s %d, phy_addr_base = %#zx, virt_addr_base=%p, offset=%#zx\n",
			 __func__, __LINE__, (size_t)phy_addr_base,
		     virt_addr_base, offset);
	} else {
		phy_addr_base = priv_data->iram_deepbuf_phy_addr;
		virt_addr_base = priv_data->iram_deepbuf_virt_addr;
		offset = priv_data->iram_deepbuf_size -
		    (2 * linklist_node_size);
	}

	for (i = 0; i < chan_cnt; i++) {
		sprd_rtd->dma_linklist_cfg_virt[i] = virt_addr_base +
			offset + i * linklist_node_size;
		sprd_rtd->dma_linklist_cfg_phy[i] = phy_addr_base +
			offset + i * linklist_node_size;
		memset_io(sprd_rtd->dma_linklist_cfg_virt[i], 0,
			  linklist_node_size);
		pr_debug("%s chan=%d linklist cfg vir=%p,phy=%#zx, size=%#x is_normal=%d\n",
			 __func__, i, sprd_rtd->dma_linklist_cfg_virt[i],
	     (size_t)sprd_rtd->dma_linklist_cfg_phy[i],
	     linklist_node_size, normal);
	}

	return 0;
}

static int alloc_linklist_cfg_resddr_s2_2(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *sprd_rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = srtd->cpu_dai;
	struct platform_pcm_priv *priv_data;
	u32 resddr_phy_base;
	char *resddr_virt_base;
	u32 offset;
	int ddr_enum;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s priv_data is null\n", __func__);
		return -EINVAL;
	}
	ddr_enum = to_ddr_enum(cpu_dai, substream->stream);
	resddr_virt_base = priv_data->resddr_virt_addr;
	resddr_phy_base = priv_data->resddr_phy_addr;
	switch (ddr_enum) {
	case DDR_DEEPBUF_PLY:
		offset = OFFSET_RESDDR_LLST_DEEPBUF_PLY;
		break;
	default:
		pr_err("%s invalid ddr_enum=%d\n", __func__, ddr_enum);
		return -EINVAL;
	}

	sprd_rtd->dma_linklist_cfg_virt2 = resddr_virt_base + offset;
	sprd_rtd->dma_linklist_cfg_phy2 = resddr_phy_base + offset;
	pr_info("%s vir=%p, phy=%#zx\n", __func__,
		sprd_rtd->dma_linklist_cfg_virt2,
		(size_t)sprd_rtd->dma_linklist_cfg_phy2);
	memset_io(sprd_rtd->dma_linklist_cfg_virt2, 0,
		  RESDDR_LLST_ONE_CHAN_SIZE);

	return 0;
}

static int pcm_alloc_dma_linklist_cfg_ddr_s2_2(
	struct snd_pcm_substream *substream, u32 linklist_node_size2)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *sprd_rtd = runtime->private_data;

	if (is_use_resddr(substream)) {
		alloc_linklist_cfg_resddr_s2_2(substream);
		return 0;
	}

	sprd_rtd->dma_linklist_cfg_virt2 =
	    (void *)dmam_alloc_coherent(substream->pcm->card->dev,
					linklist_node_size2,
					&sprd_rtd->dma_linklist_cfg_phy2,
					GFP_KERNEL);
	if (!sprd_rtd->dma_linklist_cfg_virt2) {
		pr_err("%s line[%d]: dmam_alloc_coherent failed size=%#x\n",
		       __func__, __LINE__, linklist_node_size2);
		return -ENOMEM;
	}
	pr_info("%s vir=%p, phy=%#zx, size=%#x\n", __func__,
		sprd_rtd->dma_linklist_cfg_virt2,
		(size_t)sprd_rtd->dma_linklist_cfg_phy2, linklist_node_size2);
	memset(sprd_rtd->dma_linklist_cfg_virt2, 0, linklist_node_size2);

	return 0;
}

static void pcm_free_dma_linklist_cfg_ddr_s2_2(struct snd_pcm_substream
					       *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *sprd_rtd = runtime->private_data;
	u32 linklist_node_size2 = sprd_rtd->linklist_node_size;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct platform_pcm_priv *priv_data;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s priv_data is null\n", __func__);
		return;
	}

	if (is_use_resddr(substream)) {
		sprd_rtd->dma_linklist_cfg_virt2 = NULL;
		return;
	}
	if (sprd_rtd->dma_linklist_cfg_virt2) {
		pr_info("%s\n vir=%p, phy=%#zx, size=%#x", __func__,
			sprd_rtd->dma_linklist_cfg_virt2,
			(size_t)sprd_rtd->dma_linklist_cfg_phy2,
			linklist_node_size2);
		dmam_free_coherent(substream->pcm->card->dev,
				   linklist_node_size2,
				   sprd_rtd->dma_linklist_cfg_virt2,
				   sprd_rtd->dma_linklist_cfg_phy2);
		sprd_rtd->dma_linklist_cfg_virt2 = NULL;
	}
}

/* @stages: 1 - for 1-stage dma; 2 - for 2-stage dma. */
static int sprd_pcm_alloc_dma_cfg(struct snd_pcm_substream *substream,
				  int stages, u32 linklist_node_size,
				  u32 linklist_node_size2)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *sprd_rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct device *dev = srtd->dev;
	int i = 0;
	int chan_cnt = 0;

	chan_cnt = sprd_rtd->hw_chan;
	pr_debug("%s line[%d] stage[%d]\n", __func__, __LINE__, stages);

	for (i = 0; i < chan_cnt; i++) {
		sprd_rtd->dma_config_ptr[i] = devm_kzalloc(dev,
							   linklist_node_size,
				GFP_KERNEL);
		if (!sprd_rtd->dma_config_ptr[i])
			return -ENOMEM;

		sprd_rtd->dma_config_ptr[i]->sg =
		(struct scatterlist *)((u8 *)sprd_rtd->dma_config_ptr[i] +
			sizeof(struct sprd_dma_cfg));

		sprd_rtd->dma_cb_ptr[i] =
			devm_kzalloc(
				dev,
				sizeof(struct sprd_dma_callback_data),
				GFP_KERNEL);
		if (!sprd_rtd->dma_cb_ptr[i])
			return -ENOMEM;
		pr_info("%s chan[%d] dma_config ptr=%p, size=%#x dma_pdata=%p, size_pdata =%#x\n",
			__func__, i, sprd_rtd->dma_config_ptr[i],
		    linklist_node_size, sprd_rtd->dma_cb_ptr[i],
		     (unsigned int)(sizeof(struct sprd_dma_callback_data)));
	}
	if (stages == DMA_STAGE_ONE)
		return 0;

	sprd_rtd->dma_cfg_buf2 =
	    devm_kzalloc(dev,
			 sprd_rtd->hw_chan2 * linklist_node_size2, GFP_KERNEL);

	sprd_rtd->dma_cfg_buf2->sg =
		(struct scatterlist *)((u8 *)sprd_rtd->dma_cfg_buf2 +
			sizeof(struct sprd_dma_cfg));

	pr_info
	    ("%s cfg_buf2=%p, stage[%d] size=%#x\n",
	     __func__, sprd_rtd->dma_cfg_buf2, stages,
	     sprd_rtd->hw_chan2 * linklist_node_size2);
	if (!sprd_rtd->dma_cfg_buf2) {
		pr_err("%s %d alloc failed size =%#x\n", __func__, __LINE__,
		       sprd_rtd->hw_chan2 * linklist_node_size2);
		return -ENOMEM;
	}

	sprd_rtd->dma_callback_data2 =
	    devm_kzalloc(dev, sprd_rtd->hw_chan2 *
			 sizeof(struct sprd_dma_callback_data), GFP_KERNEL);
	if (!sprd_rtd->dma_callback_data2) {
		pr_err("%s %d alloc failed size =%#x\n", __func__, __LINE__,
		       (unsigned int)(sprd_rtd->hw_chan2 *
			sizeof(struct sprd_dma_callback_data)));
		return -ENOMEM;
	}

	return 0;
}

/* @stages: 1 - for 1-stage dma; 2 - for 2-stage dma. */
static int sprd_pcm_free_dma_cfg(struct snd_pcm_substream *substream,
				 int stages)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *sprd_rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct device *dev = srtd->dev;
	int i = 0;
	int chan_cnt = 0;

	pr_info("%s stage[%d]\n", __func__, stages);
	chan_cnt = sprd_rtd->hw_chan;
	for (i = 0; i < chan_cnt; i++) {
		if (sprd_rtd->dma_cb_ptr[i]) {
			devm_kfree(dev, sprd_rtd->dma_cb_ptr[i]);
			sprd_rtd->dma_cb_ptr[i] = NULL;
		}
		if (sprd_rtd->dma_config_ptr[i]) {
			devm_kfree(dev, sprd_rtd->dma_config_ptr[i]);
			sprd_rtd->dma_config_ptr[i] = NULL;
		}
	}
	if (stages == DMA_STAGE_ONE)
		return 0;
	if (sprd_rtd->dma_callback_data2) {
		devm_kfree(dev, sprd_rtd->dma_callback_data2);
		sprd_rtd->dma_callback_data2 = NULL;
	}
	if (sprd_rtd->dma_cfg_buf2) {
		devm_kfree(dev, sprd_rtd->dma_cfg_buf2);
		sprd_rtd->dma_cfg_buf2 = NULL;
	}
	return 0;
}

/* @@return: true - use 2stage, false - use 1stage */
static bool is_use_2stage_dma(struct snd_soc_pcm_runtime *srtd, int stream)
{
	struct platform_pcm_priv *priv_data = NULL;
	struct snd_pcm_substream *substream =
	    srtd->pcm->streams[stream].substream;
	u32 cases = 0;
	s32 ret = false;

	if (!substream)
		return false;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_warn("priv_data failed\n");
		return false;
	}
	cases = priv_data->use_2stage_dma_case;
	if (srtd->cpu_dai->id == VBC_DAI_NORMAL &&
	    substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = (cases & TWO_STAGE_NORMAL) ? true:false;
	else if (srtd->cpu_dai->id == VBC_DAI_DEEP_BUF)
		ret = (cases & TWO_STAGE_DEEP) ? true:false;
	else
		ret = false;

	return ret;
}

static s32 dmabuffer_reserved_ddr_alloc(struct snd_pcm_substream *substream)
{
	struct snd_dma_buffer *dma_buffer = &substream->dma_buffer;
	struct snd_soc_pcm_runtime *srtd = substream->pcm->private_data;
	struct snd_soc_dai *cpu_dai = srtd->cpu_dai;
	struct platform_pcm_priv *priv_data;
	int stream = substream->stream;
	size_t size;
	u8 *area;
	dma_addr_t addr;
	u32 resddr_phy_base;
	char *resddr_virt_base;
	u32 offset;
	int ddr_enum;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s priv_data is null\n", __func__);
		return -EINVAL;
	}
	if (!is_use_resddr(substream)) {
		pr_err("%s dma do not use reserved ddr\n", __func__);
		return -EINVAL;
	}

	ddr_enum = to_ddr_enum(cpu_dai, stream);
	resddr_virt_base = priv_data->resddr_virt_addr;
	resddr_phy_base = priv_data->resddr_phy_addr;
	switch (ddr_enum) {
	case DDR_NORMAL_PLY:
		offset = OFFSET_RESDDR_NORMAL_PLY;
		size = NORMAL_PLAYBACK_BUFFER_BYTES_MAX;
		break;
	case DDR_NORMAL_CAP:
		offset = OFFSET_RESDDR_NORMAL_CAP;
		size = NORMAL_CAPTURE_BUFFER_BYTES_MAX;
		break;
	case DDR_DEEPBUF_PLY:
		offset = OFFSET_RESDDR_DEEPBUF_PLY;
		size = DEEPBUFFER_PLAYBACK_BUFFER_BYTES_MAX;
		break;
	case DDR_IIS_PLY:
		offset = OFFSET_RESDDR_IIS_PLY;
		size = I2S_BUFFER_BYTES_MAX;
		break;
	case DDR_IIS_CAP:
		offset = OFFSET_RESDDR_IIS_CAP;
		size = I2S_BUFFER_BYTES_MAX;
		break;
	default:
		pr_err("%s invalid ddr enum %d\n", __func__, ddr_enum);
		return -EINVAL;
	};

	area = resddr_virt_base + offset;
	addr = resddr_phy_base + offset;

	dma_buffer->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buffer->dev.dev = substream->pcm->card->dev;
	dma_buffer->private_data = dma_buffer;
	dma_buffer->area = area;
	dma_buffer->addr = addr;
	dma_buffer->bytes = size;

	return 0;
}

static s32 dmabuffer_ddr_alloc(struct snd_pcm_substream *substream,
			       u32 buffer_size)
{
	struct snd_dma_buffer *dma_buffer = &substream->dma_buffer;
	size_t size = buffer_size;

	if (is_use_resddr(substream)) {
		dmabuffer_reserved_ddr_alloc(substream);
		return 0;
	}
	dma_buffer->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buffer->dev.dev = substream->pcm->card->dev;
	dma_buffer->private_data = dma_buffer;
	dma_buffer->area =
		dma_alloc_writecombine(substream->pcm->card->dev,
				       size, &dma_buffer->addr, GFP_KERNEL);
	if (!dma_buffer->area) {
		pr_err("%s line[%d] buf->area=%p\n", __func__, __LINE__,
		       dma_buffer->area);
		return -ENOMEM;
	}

	dma_buffer->bytes = size;
	return 0;
}

static int dmabuffer_ddr_free(struct snd_pcm_substream *substream,
			      struct snd_dma_buffer *dma_buffer)
{
	struct snd_soc_pcm_runtime *srtd = substream->pcm->private_data;
	struct platform_pcm_priv *priv_data;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s priv_data is null\n", __func__);
		return 0;
	}
	if (is_use_resddr(substream)) {
		dma_buffer->area = NULL;
		dma_buffer->addr = 0;
		dma_buffer->bytes = 0;
		return 0;
	}

	if (dma_buffer->area &&
	    dma_buffer->dev.type == SNDRV_DMA_TYPE_DEV) {
		dma_free_writecombine(substream->pcm->card->dev,
				      dma_buffer->bytes, dma_buffer->area,
				      dma_buffer->addr);
		dma_buffer->area = NULL;
		dma_buffer->addr = 0;
		dma_buffer->bytes = 0;
	}

	return 0;
}

static s32 pcm_preallocate_dma_buffer(struct snd_soc_pcm_runtime *srtd,
				      int stream)
{
	int ret = -1;

	struct snd_pcm_substream *substream =
	    srtd->pcm->streams[stream].substream;
	struct snd_dma_buffer *dma_buffer = NULL;
	struct snd_soc_dai *cpu_dai = srtd->cpu_dai;
	struct platform_pcm_priv *priv_data = NULL;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);

	if (cpu_dai->id != I2S_MAGIC_ID &&
	    cpu_dai->id != VBC_DAI_NORMAL &&
		    cpu_dai->id != VBC_DAI_AD23 &&
		    cpu_dai->id != VBC_DAI_DEEP_BUF) {
		return 0;
	}

	dma_buffer = &substream->dma_buffer;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (cpu_dai->id == VBC_DAI_NORMAL) {
			if (priv_data->iram_normal_size == 0) {
				ret =
			    dmabuffer_ddr_alloc(
				    substream,
				    NORMAL_PLAYBACK_BUFFER_BYTES_MAX);
				if (ret)
					goto ERROR;
			} else {
				dma_buffer->addr =
					priv_data->iram_normal_phy_addr;
				dma_buffer->bytes =
					priv_data->iram_normal_size -
					(2 * SPRD_AUDIO_DMA_NODE_SIZE);
				dma_buffer->area =
					priv_data->iram_normal_virt_addr;
				dma_buffer->dev.type = SNDRV_DMA_TYPE_DEV_IRAM;
			}
			pr_info("%s stream = %s daiid[%d], phy[%#lx], bytes[%#zx], area[%p], dmabuffer type=%#x\n",
				__func__, "playback", cpu_dai->id,
			     (unsigned long)dma_buffer->addr, dma_buffer->bytes,
			     dma_buffer->area, dma_buffer->dev.type);
		} else if (cpu_dai->id == VBC_DAI_DEEP_BUF) {
			ret =
			    dmabuffer_ddr_alloc(
				    substream,
				    DEEPBUFFER_PLAYBACK_BUFFER_BYTES_MAX);
			if (ret)
				goto ERROR;
		} else if (cpu_dai->id == I2S_MAGIC_ID) {
			ret =
				dmabuffer_ddr_alloc(substream,
						    I2S_BUFFER_BYTES_MAX);
			if (ret)
				goto ERROR;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (cpu_dai->id == I2S_MAGIC_ID) {
			ret =
				dmabuffer_ddr_alloc(substream,
						    I2S_BUFFER_BYTES_MAX);
			if (ret)
				goto ERROR;
		} else {
			ret =
			    dmabuffer_ddr_alloc(
				    substream,
				    NORMAL_CAPTURE_BUFFER_BYTES_MAX);
			if (ret)
				goto ERROR;
		}
	}

	return 0;
ERROR:
	pr_warn("%s cpu dai[%d], stream[%d], memtype[%d] need not alloc\n",
		__func__, cpu_dai->id, substream->stream, dma_buffer->dev.type);

	return -ENOMEM;
}

static s32 pcm_preallocate_dma_buffer_free(struct snd_pcm_substream
					       *substream)
{
	struct snd_dma_buffer *dma_buffer = NULL;

	dma_buffer = &substream->dma_buffer;
	if (dma_buffer->addr && dma_buffer->dev.type != SNDRV_DMA_TYPE_DEV_IRAM)
		dmabuffer_ddr_free(substream, dma_buffer);

	return 0;
}

static void hw_params_config(struct snd_pcm_substream *substream,
			     struct snd_pcm_hardware *hw)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct platform_pcm_priv *priv_data = NULL;
	struct snd_dma_buffer *dma_buffer = NULL;

	dma_buffer = &substream->dma_buffer;
	priv_data = snd_soc_platform_get_drvdata(srtd->platform);

	/* default config */
	hw->periods_min = 1;
	if (sprd_is_i2s(srtd->cpu_dai)) {
		hw->info = SPRD_SNDRV_PCM_INFO_COMMON;
		hw->formats = SPRD_SNDRV_PCM_FMTBIT;
		hw->period_bytes_min = 8 * 2;
		hw->period_bytes_max = I2S_BUFFER_BYTES_MAX;
		hw->buffer_bytes_max = I2S_BUFFER_BYTES_MAX;
		hw->periods_max = PAGE_SIZE / DMA_LINKLIST_CFG_NODE_SIZE;
	} else {
		hw->info = SPRD_SNDRV_PCM_INFO_COMMON |
			SNDRV_PCM_INFO_NONINTERLEAVED |
			SNDRV_PCM_INFO_NO_PERIOD_WAKEUP;
		hw->formats = SPRD_SNDRV_PCM_FMTBIT;
		hw->period_bytes_min = VBC_FIFO_FRAME_NUM * 4;
		hw->period_bytes_max =
			DEEPBUFFER_PLAYBACK_BUFFER_BYTES_MAX;
		hw->buffer_bytes_max =
			DEEPBUFFER_PLAYBACK_BUFFER_BYTES_MAX;
		hw->periods_max = PAGE_SIZE / DMA_LINKLIST_CFG_NODE_SIZE;
	}
	/* capture : normal captre, fm capture
	 * playback: normal, deepbuf
	 */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (sprd_is_i2s(srtd->cpu_dai)) {
			hw->period_bytes_max =
			hw->buffer_bytes_max =
				I2S_BUFFER_BYTES_MAX;
		} else {
			hw->period_bytes_max =
			hw->buffer_bytes_max =
			NORMAL_CAPTURE_BUFFER_BYTES_MAX;
		}
	} else {
		if (srtd->cpu_dai->id == VBC_DAI_DEEP_BUF) {
			/* period_bytes_max <= iram max */
			if (!priv_data) {
				pr_err("%s %d failed\n", __func__, __LINE__);
				return;
			}
			hw->period_bytes_max = priv_data->iram_deepbuf_size -
				2 * SPRD_AUDIO_DMA_NODE_SIZE;
			hw->buffer_bytes_max =
				DEEPBUFFER_PLAYBACK_BUFFER_BYTES_MAX;
		} else if (srtd->cpu_dai->id == VBC_DAI_NORMAL) {
			hw->period_bytes_max =
			hw->buffer_bytes_max =
				NORMAL_PLAYBACK_BUFFER_BYTES_MAX;
		} else if (sprd_is_i2s(srtd->cpu_dai)) {
			hw->period_bytes_max = 32 * 2 * 100;
			hw->buffer_bytes_max = I2S_BUFFER_BYTES_MAX;
		}
	}
	if (dma_buffer->dev.type == SNDRV_DMA_TYPE_DEV_IRAM) {
		hw->periods_max =
			SPRD_AUDIO_DMA_NODE_SIZE / DMA_LINKLIST_CFG_NODE_SIZE;
	} else {
		hw->periods_max =
			PAGE_SIZE / DMA_LINKLIST_CFG_NODE_SIZE;
	}
}

static int sprd_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct snd_dma_buffer *dma_buffer = NULL;
	struct device *dev = srtd->dev;
	struct sprd_runtime_data *rtd;
	struct i2s_config *config;
	int burst_len = 0;
	int ret = 0;
	struct snd_pcm_hardware sprd_pcm_hardware = {0};
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();
	sp_asoc_pr_info("%s Open %s\n", sprd_dai_pcm_name(srtd->cpu_dai),
			PCM_DIR_NAME(substream->stream));

	dma_buffer = &substream->dma_buffer;

	if (sprd_is_i2s(srtd->cpu_dai)) {
		config = sprd_i2s_dai_to_config(srtd->cpu_dai);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			burst_len = I2S_FIFO_DEPTH - config->tx_watermark;
		else
			burst_len = config->rx_watermark;
		burst_len <<= config->byte_per_chan;
	} else {
		burst_len = (VBC_FIFO_FRAME_NUM * 4);
	}
	hw_params_config(substream, &sprd_pcm_hardware);
	snd_soc_set_runtime_hwparams(substream, &sprd_pcm_hardware);

	pr_info("cpu dai = %d  period_bytes_min=%#zx,period_bytes_max =%#zx, buffer_bytes_max=%#zx, dma_buffer.dev.type=%d, periods_min=%d,periods_max=%d, burst_len=%#x\n",
		srtd->cpu_dai->id,
			runtime->hw.period_bytes_min,
			runtime->hw.period_bytes_max,
			runtime->hw.buffer_bytes_max,
			dma_buffer->dev.type,
			runtime->hw.periods_min,
			runtime->hw.periods_max, burst_len);

	ret = snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_PERIODS,
					   1, runtime->hw.periods_max);
	if (ret < 0)
		goto out;
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
	if (!rtd) {
		pr_err("%s %d alloc rtd failed!\n", __func__, __LINE__);
		goto out;
	}
	ret = 0;
	runtime->private_data = rtd;

	mutex_lock(&pm_dma->pm_mtx_cnt);
	if (!sprd_is_normal_playback(srtd->cpu_dai->id,
				     substream->stream))
		pm_dma->no_pm_cnt++;
	else
		pm_dma->normal_rtd = rtd;
	mutex_unlock(&pm_dma->pm_mtx_cnt);

	if (sprd_is_dfm(srtd->cpu_dai) || sprd_is_vaudio(srtd->cpu_dai)) {
		runtime->private_data = rtd;
		ret = 0;
		goto out;
	}

	rtd->burst_len = burst_len;
	normal_dma_protect_mutex_lock(substream);
	normal_dma_protect_spin_lock(substream);
	rtd->dma_chn[0] = NULL;
	rtd->dma_chn[1] = NULL;
	rtd->dma_tx_des[0] = NULL;
	rtd->dma_tx_des[1] = NULL;
	rtd->cookie[0] = 0;
	rtd->cookie[1] = 0;
	normal_dma_protect_spin_unlock(substream);
	normal_dma_protect_mutex_unlock(substream);

out:
	pr_info("%s ret=%d\n", __func__, ret);
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
	sp_asoc_pr_info("%s Close %s\n", sprd_dai_pcm_name(srtd->cpu_dai),
			PCM_DIR_NAME(substream->stream));
	mutex_lock(&pm_dma->pm_mtx_cnt);
	if (!sprd_is_normal_playback(srtd->cpu_dai->id,
				     substream->stream)) {
		if (--pm_dma->no_pm_cnt < 0) {
			pr_warn("%s no_pm_cnt=%d\n",
				__func__, pm_dma->no_pm_cnt);
			pm_dma->no_pm_cnt = 0;
		}
	} else {
		pm_dma->normal_rtd = NULL;
	}
	mutex_unlock(&pm_dma->pm_mtx_cnt);
	if (sprd_is_dfm(srtd->cpu_dai) || sprd_is_vaudio(srtd->cpu_dai)) {
		devm_kfree(dev, rtd);
		return 0;
	}
	devm_kfree(dev, rtd);
	pr_info("return %s line[%d]\n", __func__, __LINE__);

	return 0;
}

static void reset_dma_level1_int_count(struct snd_pcm_substream *substream,
				       int normal)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	u32 count_max = 0;
	u32 offset;
	u32 offset_total;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct platform_pcm_priv *priv_data = NULL;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return;
	}

	/* reset count_index */
	offset = (normal == PLY_NORMAL) ?
		COUNT_IDX_OFFSET_NORMAL : COUNT_IDX_OFFSET_DEPBUF;
	writel_relaxed(0, (void *)priv_data->iram_4arm7_virt_addr + offset);

	/* notify cm4 count max */
	count_max = (rtd->pointer2_step_max);
	offset = (normal == PLY_NORMAL) ?
		COUNT_MAX_OFFSET_NORMAL : COUNT_MAX_OFFSET_DEPBUF;
	writel_relaxed(count_max,
		       (void *)(priv_data->iram_4arm7_virt_addr + offset));
	/* reset totale count */
	offset_total = (normal == PLY_NORMAL) ?
		TOTAL_CNT_OFFSET_NORMAL : TOTAL_CNT_OFFSET_DEPBUF;
	writel_relaxed(0, (void *)priv_data->iram_4arm7_virt_addr +
		      offset_total);
	pr_info("%s iram_4arm7_virt_addr=%p, reset count_max =%u, node_cfg_count_2=%#x\n",
		__func__,
		priv_data->iram_4arm7_virt_addr, count_max,
		rtd->node_cfg_count_2);
}

static void sprd_pcm_dma_buf_done_level1(void *data)
{
	struct sprd_dma_callback_data *dma_cb_data =
	    (struct sprd_dma_callback_data *)data;
	struct snd_pcm_substream *substream = dma_cb_data->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	u32 pointer2_step_bytes = 0;
	u32 count_max = 0;
	struct platform_pcm_priv *priv_data = NULL;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return;
	}
	priv_data->dma_2stage_level_1_ap_int_count++;

	pointer2_step_bytes = rtd->pointer2_step_bytes;
	count_max =
	    frames_to_bytes(runtime,
			    runtime->buffer_size) / pointer2_step_bytes;
	if (frames_to_bytes(runtime, runtime->buffer_size) %
	    pointer2_step_bytes) {
		pr_debug("%#zx, node_size_level1 =%#x failed\n",
			 frames_to_bytes(runtime, runtime->buffer_size),
			 pointer2_step_bytes);
		return;
	}

	if (priv_data->dma_2stage_level_1_ap_int_count > count_max)
		priv_data->dma_2stage_level_1_ap_int_count = 1;
}

static void sprd_pcm_dma_buf_done_level2(void *data)
{
	struct sprd_dma_callback_data *dma_cb_data =
	    (struct sprd_dma_callback_data *)data;
	u32 temp = 0;
	struct snd_pcm_runtime *runtime = dma_cb_data->substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;

	temp =
	    sprd_pcm_dma_get_addr(rtd->dma_chn2, rtd->cookie2,
				  dma_cb_data->substream);
	snd_pcm_period_elapsed(dma_cb_data->substream);
}

static void sprd_pcm_dma_buf_done(void *data)
{
	struct sprd_dma_callback_data *dma_cb_data =
	    (struct sprd_dma_callback_data *)data;
	struct snd_pcm_runtime *runtime = dma_cb_data->substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = dma_cb_data->substream->private_data;
	int i = 0;

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

static s32 used_hw_chan(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	s32 used_count = 0;
	s32 params_count = 0;

	/* if use i2s use hw chan is one */
	params_count = params_channels(params);
	if (sprd_is_i2s(srtd->cpu_dai)) {
		pr_info("%s cpu dai is i2s\n", __func__);
		used_count = 1;
		return used_count;
	}
#if (defined(CONFIG_SND_SOC_SPRD_VBC_R3P0))
	/* all dais except VBC_DAI_ID_NORMAL_OUTDSP
	 * and VBC_DAI_ID_FM_CAPTURE use mcdt
	 * so they use 1 dma channel.
	 */
	if (params_count == 2) {
		switch (srtd->cpu_dai->id) {
		case VBC_DAI_ID_NORMAL_OUTDSP:
		case VBC_DAI_ID_FM_CAPTURE:
		case VBC_DAI_ID_BT_CAPTURE:
			used_count = 2;
			break;
		default:
			used_count = 1;
			break;
		}
	} else if (params_count == 1) {
		used_count = 1;
	} else {
		pr_err("%s unknown params_count[%d]\n", __func__, params_count);
		return -1;
	}
#else
	used_count = params_count;
#endif
	return used_count;
}

static int sprd_pcm_request_dma_channel(struct snd_pcm_substream *substream,
					struct device_node *np,
					struct sprd_runtime_data *rtd,
					int ch_cnt)
{
	int i;
	struct dma_chan *dma_chn_request = NULL;
	struct sprd_pcm_dma_params *dma_data = rtd->params;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct audio_pm_dma *pm_dma;
	struct dma_chan *temp_dma_chan;

	pm_dma = get_pm_dma();
	if (!dma_data) {
		pr_err("ERR: %s, dma_data is NULL!\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ch_cnt; i++) {
		if (!dma_data->used_dma_channel_name[i]) {
			pr_err
			    ("ERR: dma_data->used_dma_channel_name[i] NULL\n");
			dma_chn_request = NULL;
			break;
		}

		dma_chn_request =
		    of_dma_request_slave_channel(np,
						 dma_data->used_dma_channel_name
						 [i]);
		if (IS_ERR(dma_chn_request)) {
			pr_err("ERR: request dma channel(%s) failed!(%ld)\n",
			       dma_data->used_dma_channel_name[i],
			       PTR_ERR(dma_chn_request));
			dma_chn_request = NULL;
			break;
		}
		normal_dma_protect_mutex_lock(substream);
		normal_dma_protect_spin_lock(substream);
		rtd->dma_chn[i] = dma_chn_request;
		normal_dma_protect_spin_unlock(substream);
		normal_dma_protect_mutex_unlock(substream);
		pr_info("Chan%d DMA device ID=%d\n",
			dma_chn_request->chan_id, rtd->params->channels[i]);
	}

	if (!dma_chn_request) {
		pr_err("ERR:PCM Request DMA Error %d\n", dma_data->channels[i]);
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
	if (!is_use_2stage_dma(srtd, substream->stream))
		return 0;

	if (!dma_data->used_dma_channel_name2) {
		pr_err
		    ("%s L%d dma_data->used_dma_channel_name\n",
		     __func__, __LINE__);
		rtd->dma_chn2 = NULL;
		return -EINVAL;
	}

	dma_chn_request =
	    of_dma_request_slave_channel(np, dma_data->used_dma_channel_name2);
	if (IS_ERR(dma_chn_request)) {
		pr_err("ERR: request dma channel(%s) failed!(%ld)\n",
		       dma_data->used_dma_channel_name2,
		       PTR_ERR(dma_chn_request));
		rtd->dma_chn2 = NULL;
		return PTR_ERR(dma_chn_request);
	}
	rtd->dma_chn2 = dma_chn_request;

	pr_info("Chan%d\n", rtd->dma_chn2->chan_id);

	return 0;
}

static int dma_step_cal(u32 datawidth, int ch_cnt, int interleaved)
{
	u32 step = 0;

	if (interleaved) {
		switch (datawidth) {
		case DMA_SLAVE_BUSWIDTH_2_BYTES:
			step = 2 * ch_cnt;
			break;
		case DMA_SLAVE_BUSWIDTH_4_BYTES:
			step = 4 * ch_cnt;
			break;
		default:
			step = 0;
			pr_err("err: datawidth %u is not supported yet.\n",
			       datawidth);
			break;
		}
	} else {
		step = (datawidth == DMA_SLAVE_BUSWIDTH_4_BYTES) ? 4 : 2;
	}
	return step;
}

static void dma_cfg_com(struct sprd_dma_cfg *dma_cfg,
			unsigned long cfg_addr_p, unsigned long cfg_addr_v,
			u32 datawidth, u32 fragment,
			unsigned long source_addr_p,
			unsigned long dst_addr_p, u32 step,
			unsigned long dma_confg_flag,
			u32 direction, u32 dev_id)
{
	dma_cfg->datawidth = datawidth;
	dma_cfg->fragmens_len = fragment;
	dma_cfg->dma_config_flag =  dma_confg_flag;
	dma_cfg->config.src_addr_width = datawidth;
	dma_cfg->config.dst_addr_width = datawidth;
	dma_cfg->config.src_maxburst = fragment;
	dma_cfg->config.direction = direction;
	dma_cfg->config.slave_id = dev_id;
	dma_cfg->config.step = step;
	dma_cfg->config.src_addr = source_addr_p;
	dma_cfg->config.dst_addr = dst_addr_p;
	dma_cfg->ll_cfg.virt_addr = cfg_addr_v;
	dma_cfg->ll_cfg.phy_addr = cfg_addr_p;
}

/* iram_count = 0, means ignore iram_count(used by one stage dma) */
static int dma_cfg_list(struct sprd_dma_cfg *dma_cfg,
			u32 datawidth, unsigned long start_addr_p, u32 step,
		u32 transaction, u32 wrap_ptr_offset,
		u32 node_bytes, u32 node_count, u32 iram_count,
		u32 offset_node_idx, enum DMA_LEVEL level)
{
	unsigned long addr_offset = 0;
	unsigned long dst_offset = 0;
	u32 wrap_ptr = 0;
	int i = 0;
	struct scatterlist *sg = NULL;
	struct sprd_dma_cfg *cfg = dma_cfg;
	u32 fact = (datawidth == DMA_SLAVE_BUSWIDTH_2_BYTES) ? 2 : 4;
	u32 step_num = node_bytes / fact;

	switch (level) {
	case DMA_LEVEL_2:
		addr_offset = step_num * step;
		dst_offset = 0;
		if (wrap_ptr_offset != 0) {
			/* dst address wrap(iram wrap) */
			wrap_ptr = (start_addr_p + wrap_ptr_offset);
		}
	break;
	case DMA_LEVEL_1:
		if (iram_count <= 0)
			return -1;
		/* iram_count default is 1 */
		if (iram_count == 1) {
			addr_offset = 0;
			dst_offset = 0;
			pr_info("used iram_count =%d\n", iram_count);
		} else {
			addr_offset = step_num * step;
		}
	break;
	case DMA_LEVEL_ONESTAGE:
			addr_offset = step_num * step;
	break;
	default:
		pr_err("%s invalid level=%d\n", __func__, level);
		return -1;
	}

	cfg->ll_cfg.wrap_ptr = wrap_ptr;

	pr_info("step = %#x, addr_offset=%#lx,  step_num =%#x, fact=%#x, iram_count=%#x node_bytes=%#x,node_count=%#x\n",
		step, addr_offset, step_num, fact,
	     iram_count, node_bytes, node_count);
	for (i = 0; i < node_count; i++) {
		sg = (struct scatterlist *)((u8 *)
				cfg->sg +
				i * sizeof(struct scatterlist));
		if (i == 0) {
			sg_init_table(sg, node_count);
			cfg->sg_num = node_count;
		}
		sg_dma_len(sg) = transaction;
		sg_dma_address(sg) =
		(unsigned long)(start_addr_p +
					addr_offset * ((i + offset_node_idx) %
						  node_count));
	}

	return 0;
}

static struct dma_async_tx_descriptor *dma_cfg_hw(struct dma_chan *chn,
						  struct sprd_dma_cfg *dma_cfg,
						  void *callback,
						  void *data)
{
	int ret = 0;
	struct dma_async_tx_descriptor *desp = NULL;
	/* config dma channel */
	ret = dmaengine_slave_config(chn, &dma_cfg->config);
	if (ret < 0) {
		sp_asoc_pr_dbg("%s, DMA chan ID %d config is failed!\n",
			       __func__, chn->chan_id);
		goto ERROR;
	}
	/* get dma desc from dma config */
	desp = chn->device->device_prep_slave_sg(chn, dma_cfg->sg,
			    dma_cfg->sg_num, dma_cfg->config.direction,
			    dma_cfg->dma_config_flag,
			    &dma_cfg->ll_cfg);
	if (!desp) {
		sp_asoc_pr_dbg("%s, DMA chan ID %d memcpy is failed!\n",
			       __func__, chn->chan_id);
		goto ERROR;
	}
	if (callback) {
		sp_asoc_pr_info
		    ("%s, Register Callback func for DMA chan ID %d\n",
		     __func__, chn->chan_id);
		desp->callback = callback;
		desp->callback_param = data;
	}

	return desp;
ERROR:

	return NULL;
}

static int dma_cfg_set(struct sprd_dma_cfg *dma_cfg,
		       long source_addr_p,
			unsigned long dst_addr_p,
			u32 wrap_ptr_offset, unsigned long cfg_addr_p,
			unsigned long cfg_addr_v, u32 step,
			u32 datawidth, u32 fragments_len, u32 translen,
			u32 dev_id, u32 iram_count, u32 node_count,
			u32 offset_node_idx, u32 node_bytes,
			enum DMA_LEVEL level, unsigned long flag_1,
			int direction)
{
	int ret = 0;
	unsigned long start_addr_p = 0;

	if (direction == DMA_DEV_TO_MEM)
		start_addr_p = dst_addr_p;
	else
		start_addr_p = source_addr_p;

	dma_cfg_com(dma_cfg, cfg_addr_p, cfg_addr_v, datawidth,
		    fragments_len, source_addr_p, dst_addr_p, step, flag_1,
		direction, dev_id);
	ret = dma_cfg_list(dma_cfg, datawidth, start_addr_p,
			   step, translen, wrap_ptr_offset, node_bytes,
				node_count, iram_count, offset_node_idx, level);
	if (ret < 0)
		return ret;

	return 0;
}

/* dma data and linklist cfg assigned here */
static int pcm_set_dma_linklist_data_s2_1(struct snd_pcm_substream *substream,
					  struct sprd_runtime_data *sprd_rtd)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct platform_pcm_priv *priv_data = NULL;
	u32 phy_addr_ap;
	u32 phy_addr_dma;
	u32 offset;
	u8 *virt_addr;
	u32 linklist_node_size = SPRD_AUDIO_DMA_NODE_SIZE;
	int normal = 0;
	int chan_cnt = 0;
	int i = 0;

	chan_cnt = sprd_rtd->hw_chan;

	if (srtd->cpu_dai->id == VBC_DAI_NORMAL) {
		normal = PLY_NORMAL;
	} else if (srtd->cpu_dai->id == VBC_DAI_DEEP_BUF) {
		normal = PLY_DEEPBUF;
	} else {
		pr_err("%s not supported\n", __func__);
		return -1;
	}

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_warn("priv_data failed\n");
		return -1;
	}

	if (normal == PLY_NORMAL) {
		phy_addr_ap = priv_data->iram_normal_phy_addr;
		phy_addr_dma = priv_data->iram_normal_phy_addr;
		virt_addr = (u8 *)(priv_data->iram_normal_virt_addr);
		offset =
		    priv_data->iram_normal_size - (2 * linklist_node_size);
	} else {
		phy_addr_ap = priv_data->iram_deepbuf_phy_addr;
		phy_addr_dma = priv_data->iram_deepbuf_phy_addr;
		virt_addr = (u8 *)(priv_data->iram_deepbuf_virt_addr);
		offset =
		    priv_data->iram_deepbuf_size - (2 * linklist_node_size);
	}
	/* iram dma data */
	sprd_rtd->iram_phy_addr_ap = phy_addr_ap;
	sprd_rtd->iram_phy_addr_dma = phy_addr_dma;
	sprd_rtd->iram_virt_addr = virt_addr;
	sprd_rtd->iram_data_max_s2_1 = offset;
	pr_info("%s iram_phy_addr_dma =%#x", __func__,
		sprd_rtd->iram_phy_addr_dma);
	/* iram dma linklist cfg */
	for (i = 0; i < chan_cnt; i++) {
		sprd_rtd->dma_linklist_cfg_virt[i] = virt_addr +
			offset + i * linklist_node_size;
		sprd_rtd->dma_linklist_cfg_phy[i] = phy_addr_dma +
			offset + i * linklist_node_size;
		pr_info("chan[%d] dma_linklist_cfg_phy=%#lx, linklist_node_size=%#x\n",
			i, (unsigned long)sprd_rtd->dma_linklist_cfg_phy[i],
		     linklist_node_size);
		memset_io(sprd_rtd->dma_linklist_cfg_virt[i], 0,
			  linklist_node_size);
	}

	return 0;
}

static u32 sprd_pcm_get_sync_ap(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct platform_pcm_priv *priv_data = NULL;
	u32 phy_addr;
	u32 sync_ap;
	void __iomem *virt_addr;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return 0;
	}
	virt_addr = priv_data->iram_4arm7_virt_addr + SPRD_PCM_SYNC_AP;
	phy_addr = priv_data->iram_4arm7_phy_addr + SPRD_PCM_SYNC_AP;
	sync_ap = readl_relaxed(virt_addr);
	pr_info("%s sync_ap =%u virt_addr=%p, phy_addr = %#x\n", __func__,
		sync_ap, virt_addr, phy_addr);

	return sync_ap;
}

static void sprd_pcm_set_sync_ap(struct snd_pcm_substream *substream,
				 u32 sync_ap)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct platform_pcm_priv *priv_data = NULL;
	u32 phy_addr;
	void __iomem *virt_addr;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return;
	}
	virt_addr = priv_data->iram_4arm7_virt_addr + SPRD_PCM_SYNC_AP;
	phy_addr = priv_data->iram_4arm7_phy_addr + SPRD_PCM_SYNC_AP;
	writel_relaxed(sync_ap, virt_addr);
	pr_info("%s sync_ap =%u virt_addr=%p, phy_addr = %#x\n", __func__,
		sync_ap, virt_addr, phy_addr);
}

static u32 sprd_pcm_get_sync_cm4(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct platform_pcm_priv *priv_data = NULL;
	u32 phy_addr;
	u32 sync_cm4;
	void __iomem *virt_addr;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return 0;
	}
	virt_addr = priv_data->iram_4arm7_virt_addr + SPRD_PCM_SYNC_CM4;
	phy_addr = priv_data->iram_4arm7_phy_addr + SPRD_PCM_SYNC_CM4;
	sync_cm4 = readl_relaxed(virt_addr);
	pr_info("%s sync_cm4 =%u virt_addr=%p, phy_addr = %#x\n", __func__,
		sync_cm4, virt_addr, phy_addr);

	return sync_cm4;
}

static int sprd_pcm_hw_params_2stage(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_pcm_dma_params *dma_data;
	size_t totsize = params_buffer_bytes(params);
	size_t period = params_period_bytes(params);
	int ret = 0;
	int ch_cnt_1 = 0;
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	int width_in_bytes =
	    snd_pcm_format_physical_width(params_format(params)) / 8;
	int channels = params_channels(params);
	struct platform_pcm_priv *priv_data = NULL;
	u32 linklist_node_size_1 = 0;
	u32 linklist_node_size_2 = 0;
	/* level 1 variable define */
	dma_addr_t dma_buff_phys_1[SPRD_PCM_CHANNEL_MAX];
	unsigned long flag_1 = 0;
	u32 irq_mode_1 = 0;
	void *callback_1 = NULL;
	void *callback_param_1 = NULL;
	u32 fragments_len_1 = 0;
	u32 block_len_1 = 0;
	u32 trans_len_1 = 0;
	/* for dma driver limit,node_cfg_count_1 set to
	 *2 just for dma loop
	 */
	u32 node_cfg_count_1 = 2;
	/* iram_count_1 from dts:
	 * 1: not use pingpong buf
	 * 2 use pingpong buf
	 */
	u32 iram_count_1 = 0;
	u32 nodebytes_1 = 0;
	u32 step = 0;
	u32 src_step_1 = 0;
	u32 dst_step_1 = 0;
	unsigned long source_addr_p_1 = 0;
	unsigned long dst_addr_p_1 = 0;
	int i = 0;
	u32 temp_count = 0;
	u32 temp_count_r = 0;
	/* level 2 variable define */
	int ch_cnt_2 = 0;
	u32 flag_2 = 0;
	void *callback_2 = NULL;
	void *callback_param_2 = NULL;
	u32 fragments_len_2 = 0;
	u32 block_len_2 = 0;
	u32 trans_len_2 = 0;
	u32 node_cfg_count_2 = 0;
	u32 nodebytes_2 = 0;
	u32 src_step_2 = 0;
	u32 dst_step_2 = 0;
	unsigned long source_addr_p_2 = 0;
	unsigned long dst_addr_p_2 = 0;
	u32 wrap_ptr2 = 0;
	u32 offset_node_idx_2 = 0;
	u32 count_max_r = 0;
	unsigned long start_addr_p = 0;
	int direction;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return -1;
	}

	sp_asoc_pr_info("(pcm) %s, cpudai_id=%d\n", __func__,
			srtd->cpu_dai->id);
	dma_data = snd_soc_dai_get_dma_data(srtd->cpu_dai, substream);
	if (!dma_data)
		goto no_dma;

	sprd_pcm_set_sync_ap(substream, 0);
	ch_cnt_1 = used_hw_chan(substream, params);
	rtd->hw_chan = ch_cnt_1;
	rtd->hw_chan2 = 1;
	rtd->dma_chn2 = NULL;
	rtd->dma_tx_des2 = NULL;
	rtd->cookie2 = 0;
	if (!rtd->params) {
		rtd->params = dma_data;
		ret =
		    sprd_pcm_request_dma_channel(substream,
						 srtd->platform->dev->of_node,
					 rtd, rtd->hw_chan);
	if (ret)
		goto hw_param_err;
	}
	sprd_pcm_set_sync_ap(substream, 1);
	node_cfg_count_2 = totsize / period;
	linklist_node_size_1 = sizeof(struct sprd_dma_cfg) +
		node_cfg_count_1 * sizeof(struct scatterlist);
	linklist_node_size_2 = sizeof(struct sprd_dma_cfg) +
		node_cfg_count_2 * sizeof(struct scatterlist);
	rtd->linklist_node_size = linklist_node_size_2;
	pcm_alloc_dma_linklist_cfg_ddr_s2_2(substream, linklist_node_size_2);
	ret = sprd_pcm_alloc_dma_cfg(substream, DMA_STAGE_TWO,
				     linklist_node_size_1,
				     linklist_node_size_2);
	if (ret < 0) {
		pr_err("failed %s %d\n", __func__, __LINE__);
		goto hw_param_err;
	}
	ret = 0;

	/* dma level2 dma data buffer */
	pcm_set_dma_linklist_data_s2_1(substream, rtd);
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	runtime->dma_bytes = totsize;
	pr_info("hw_chan=%d, hw_chan2=%d totsize=%#zx(%#zx frames)\n",
		rtd->hw_chan, rtd->hw_chan2, totsize,
		totsize / width_in_bytes / channels);
	pr_info("period=iram_size=%#zx(%#zx frames), width_in_bytes=%d, channels=%d\n",
		period, period / width_in_bytes / channels,
		width_in_bytes, channels);

	rtd->interleaved = (rtd->hw_chan == 2) &&
		sprd_pcm_is_interleaved(runtime);

	/* level 1 */
	/* get buffer phys level 1 */
	if (sprd_pcm_is_interleaved(runtime)) {
		if (dma_data->desc.datawidth == DMA_SLAVE_BUSWIDTH_2_BYTES)
			rtd->dma_addr_offset = 2;
		else if (dma_data->desc.datawidth == DMA_SLAVE_BUSWIDTH_4_BYTES)
			rtd->dma_addr_offset = 4;
	}
	for (i = 0; i < ch_cnt_1; i++) {
		rtd->dma_cb_ptr[i]->dma_chn = rtd->dma_chn[i];
		rtd->dma_cb_ptr[i]->substream = substream;
		dma_buff_phys_1[i] =
		    rtd->iram_phy_addr_dma + i * rtd->dma_addr_offset;
	}

	if (is_playback) {
		src_step_1 =
		    dma_step_cal(dma_data->desc.datawidth, ch_cnt_1,
				 sprd_pcm_is_interleaved(runtime));
		dst_step_1 = 0;
	} else {
		src_step_1 = 0;
		dst_step_1 =
		    dma_step_cal(dma_data->desc.datawidth, ch_cnt_1,
				 sprd_pcm_is_interleaved(runtime));
	}

	for (i = 0; i < ch_cnt_1; i++) {
		if (is_playback) {
			source_addr_p_1 = dma_buff_phys_1[i];
			dst_addr_p_1 = dma_data->dev_paddr[i];
		} else {
			source_addr_p_1 = dma_data->dev_paddr[i];
			dst_addr_p_1 = dma_buff_phys_1[i];
		}
		pr_info("%s chanel[%d], is_plabak=%d\n", __func__, i,
			is_playback);
		/* set irq mod, only one channel trigger interrupt */
		if (priv_data->dma_2stage_level_1_int_source !=
		     DMA_2STAGE_INT_SOURCE_NONE && i == 0) {
			irq_mode_1 = 1;
			if (priv_data->dma_2stage_level_1_int_source ==
			    DMA_2STAGE_INT_SOURCE_AP) {
				callback_1 = sprd_pcm_dma_buf_done_level1;
				callback_param_1 = (void *)(rtd->dma_cb_ptr[i]);
			}
		} else {
			irq_mode_1 = 0;
			callback_1 = NULL;
			callback_param_1 = NULL;
		}
		rtd->dma_callback1_func[i] = callback_1;
		/* set flag */
		if (i == 0) {
			if (srtd->cpu_dai->id == VBC_DAI_NORMAL) {
				pr_info("cpu_dai[%d] use group-1\n",
					srtd->cpu_dai->id);

				if (!irq_mode_1) {
					flag_1 =
						SPRD_DMA_FLAGS(
							SPRD_DMA_SRC_CHN1,
							SPRD_DMA_TRANS_DONE_TRG,
							SPRD_DMA_FRAG_REQ,
							SPRD_DMA_NO_INT);
				} else {
					flag_1 =
						SPRD_DMA_FLAGS(
							SPRD_DMA_SRC_CHN1,
							SPRD_DMA_TRANS_DONE_TRG,
							SPRD_DMA_FRAG_REQ,
							SPRD_DMA_TRANS_INT);
				}

			} else if (srtd->cpu_dai->id ==
					VBC_DAI_DEEP_BUF) {
				if (!irq_mode_1) {
					flag_1 =
						SPRD_DMA_FLAGS(
							SPRD_DMA_SRC_CHN1,
							SPRD_DMA_TRANS_DONE_TRG,
							SPRD_DMA_FRAG_REQ,
							SPRD_DMA_NO_INT);
				} else {
					flag_1 =
						SPRD_DMA_FLAGS(
							SPRD_DMA_SRC_CHN1,
							SPRD_DMA_TRANS_DONE_TRG,
							SPRD_DMA_FRAG_REQ,
							SPRD_DMA_FRAG_INT);
				}
				pr_info("cpu_dai[%d] use group-2\n",
					srtd->cpu_dai->id);
			} else {
				pr_err("%s %d failed unknown cpu_dai_id=%d\n",
				       __func__, __LINE__, srtd->cpu_dai->id);
				goto hw_param_err;
			}
		} else {
			flag_1 = SPRD_DMA_FLAGS(SPRD_DMA_CHN_MODE_NONE,
						SPRD_DMA_NO_TRG,
						SPRD_DMA_FRAG_REQ,
						SPRD_DMA_NO_INT);
		}
		fragments_len_1 =
		    dma_data->desc.fragmens_len * (DMA_SLAVE_BUSWIDTH_2_BYTES ==
						   dma_data->desc.datawidth ?
						   2 : 4);
		block_len_1 = fragments_len_1;

		iram_count_1 = priv_data->node_count_2stage_level1;
		if (iram_count_1 <= 0) {
			pr_err("%s invalid iram_count\n", __func__);
			goto hw_param_err;
		}

		temp_count =
		    period / (iram_count_1 * ch_cnt_1 * fragments_len_1);
		temp_count_r = period % (iram_count_1 *
					 ch_cnt_1 * fragments_len_1);
		if (temp_count_r != 0) {
			pr_err("%s %d period[%#zx] invalid, iram_count_1=%d, ch_cnt_1=%d, fragments_len_1=%#x\n",
			       __func__, __LINE__, period, iram_count_1,
				ch_cnt_1, fragments_len_1);
			goto hw_param_err;
		}
		block_len_1 = fragments_len_1;
		trans_len_1 = temp_count * fragments_len_1;
		nodebytes_1 = trans_len_1;

		if (is_playback) {
			start_addr_p = source_addr_p_1;
			step = src_step_1;
			direction = DMA_MEM_TO_DEV;
		} else {
			start_addr_p = dst_addr_p_1;
			step = dst_step_1;
			direction = DMA_DEV_TO_MEM;
		}

		/* level 1 */
		ret = dma_cfg_set(rtd->dma_config_ptr[i],
				  source_addr_p_1, dst_addr_p_1, 0,
				    (unsigned long)
				    rtd->dma_linklist_cfg_phy[i],
				    (unsigned long)
				    rtd->dma_linklist_cfg_virt[i],
				    step, dma_data->desc.datawidth,
				    fragments_len_1, trans_len_1,
				    dma_data->channels[i], iram_count_1,
				    node_cfg_count_1, 0, nodebytes_1,
				    DMA_LEVEL_1, flag_1, direction);
		if (ret < 0)
			goto hw_param_err;
		rtd->dma_tx_des[i] =
			dma_cfg_hw(
				rtd->dma_chn[i],
				rtd->dma_config_ptr[i],
				callback_1,
				callback_param_1);
	}

	/* level 2 */
	ch_cnt_2 = rtd->hw_chan2;
	rtd->dma_callback_data2->dma_chn = rtd->dma_chn2;
	rtd->dma_callback_data2->substream = substream;
	/* when level_1_int_source source use CP(arm7), level2 use CP too */
	if (!(params->flags & SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP) &&
	    priv_data->dma_2stage_level_1_int_source ==
			    DMA_2STAGE_INT_SOURCE_AP) {
		callback_2 = sprd_pcm_dma_buf_done_level2;
		callback_param_2 = (void *)(rtd->dma_callback_data2);
	}
	rtd->dma_callback2_func = callback_2;
	if (srtd->cpu_dai->id == VBC_DAI_NORMAL) {
		flag_2 = SPRD_DMA_FLAGS(SPRD_DMA_DST_CHN1,
					SPRD_DMA_TRANS_DONE_TRG,
					SPRD_DMA_FRAG_REQ,
					SPRD_DMA_NO_INT);
		pr_info("cpu_dai[%d] use group-1\n", srtd->cpu_dai->id);
	} else if (srtd->cpu_dai->id == VBC_DAI_DEEP_BUF) {
		flag_2 = SPRD_DMA_FLAGS(SPRD_DMA_DST_CHN1,
					SPRD_DMA_TRANS_DONE_TRG,
					SPRD_DMA_FRAG_REQ,
						SPRD_DMA_NO_INT);
		pr_info("cpu_dai[%d] use group-2\n", srtd->cpu_dai->id);
	} else {
		pr_err("%s %d failed unknown cpu_dai_id=%d\n",
		       __func__, __LINE__, srtd->cpu_dai->id);
		goto hw_param_err;
	}

	if (is_playback) {
		src_step_2 =
		    dma_step_cal(dma_data->desc2.datawidth, ch_cnt_2, false);
		dst_step_2 = src_step_2;
		step = dst_step_2;
	} else {
		dst_step_2 =
		    dma_step_cal(dma_data->desc2.datawidth, ch_cnt_2, false);
		dst_step_2 = src_step_2;
		step = dst_step_2;
	}
	if (is_playback) {
		source_addr_p_2 = runtime->dma_addr;
		dst_addr_p_2 = rtd->iram_phy_addr_dma;
		start_addr_p = source_addr_p_2;

	} else {
		source_addr_p_2 = rtd->iram_phy_addr_dma;
		dst_addr_p_2 = runtime->dma_addr;
		start_addr_p = dst_addr_p_2;
	}

	fragments_len_2 = trans_len_1 * ch_cnt_1;
	block_len_2 = fragments_len_2;
	if (period != fragments_len_2 * iram_count_1) {
		pr_err("%s period size != fragments_len_2(%#x) * iram_count_1(%#x)\n",
		       __func__,
			fragments_len_2, iram_count_1);
		goto hw_param_err;
	}
	trans_len_2 = period;
	nodebytes_2 = period;
	/* nodebytes_2 == used iram size(period) need not use wrap,
	 * (only when nodebytes_2 > used iram size we would consider using wrap)
	 */
	wrap_ptr2 = period;

	/* desc2.transcation_len used in trigger, init iram */
	dma_data->desc2.transcation_len = trans_len_2;
	/* pointer2 step bytes */
	/* pointer2 step num max */
	rtd->pointer2_step_bytes = fragments_len_1 * ch_cnt_1;
	rtd->pointer2_step_max = totsize / rtd->pointer2_step_bytes;
	count_max_r = totsize % rtd->pointer2_step_bytes;
	if (srtd->cpu_dai->id == VBC_DAI_NORMAL) {
		priv_data->pointer2_step_bytes[PLY_NORMAL] =
		rtd->pointer2_step_bytes;
		priv_data->chan_cnt[PLY_NORMAL] = rtd->hw_chan;
		priv_data->width_in_bytes[PLY_NORMAL] = width_in_bytes;
	} else {
		priv_data->pointer2_step_bytes[PLY_DEEPBUF] =
		rtd->pointer2_step_bytes;
		priv_data->chan_cnt[PLY_DEEPBUF] = rtd->hw_chan;
		priv_data->width_in_bytes[PLY_DEEPBUF] = width_in_bytes;
	}
	rtd->node_cfg_count_2 = node_cfg_count_2;
	if (count_max_r != 0) {
		pr_err("%s totsize (%#zx)\n", __func__, totsize);
		pr_err("must be multiple of pointer2_step_bytes(%#x)\n",
		       (rtd->pointer2_step_bytes));
		ret = -1;
		goto hw_param_err;
	}

	/* node number to step, used_iram_size / nodebytes_2 */
	offset_node_idx_2 = period / nodebytes_2;
	/* level 2 */
	ret = dma_cfg_set(rtd->dma_cfg_buf2, source_addr_p_2, dst_addr_p_2,
			  wrap_ptr2, (unsigned long)rtd->dma_linklist_cfg_phy2,
		    (unsigned long)rtd->dma_linklist_cfg_virt2, step,
		    dma_data->desc2.datawidth, fragments_len_2,
		    trans_len_2, 0, 0,
		    node_cfg_count_2, offset_node_idx_2, nodebytes_2,
		    DMA_LEVEL_2, flag_2, direction);
	if (ret < 0)
		goto hw_param_err;
	rtd->dma_tx_des2 =
	    dma_cfg_hw(rtd->dma_chn2, rtd->dma_cfg_buf2, callback_2,
		       callback_param_2);
	sprd_pcm_proc_init(substream);
	/* init int count */
	priv_data->dma_2stage_level_1_ap_int_count = 0;
	if (srtd->cpu_dai->id == VBC_DAI_NORMAL)
		reset_dma_level1_int_count(substream,
					   PLY_NORMAL);
	else if (srtd->cpu_dai->id == VBC_DAI_DEEP_BUF) {
		reset_dma_level1_int_count(substream,
					   PLY_DEEPBUF);
	} else {
		pr_err("%s, unknown cpu_dai[%d]\n", __func__,
		       srtd->cpu_dai->id);
		return -EINVAL;
	}
	goto ok_go_out;

no_dma:
	sp_asoc_pr_dbg("no dma\n");
	rtd->params = NULL;
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = 0;

	return ret;
hw_param_err:
	pr_err("ERR:%s line[%d] failed!\n", __func__, __LINE__);
	if (srtd->cpu_dai->id == VBC_DAI_NORMAL) {
		priv_data->pointer2_step_bytes[PLY_NORMAL] = 0;
		priv_data->chan_cnt[PLY_NORMAL] = 0;
		priv_data->width_in_bytes[PLY_NORMAL] = 0;
	} else {
		priv_data->pointer2_step_bytes[PLY_DEEPBUF] = 0;
		priv_data->chan_cnt[PLY_DEEPBUF] = 0;
		priv_data->width_in_bytes[PLY_DEEPBUF] = 0;
	}
	sprd_pcm_set_sync_ap(substream, 0);
	for (i = 0; i < rtd->hw_chan; i++) {
		if (rtd->dma_chn[i]) {
			pr_info("%s release chan_id %d, channels %d\n",
				__func__, rtd->dma_chn[i]->chan_id,
				rtd->params->channels[i]);
			dma_release_channel(rtd->dma_chn[i]);
			rtd->dma_chn[i] = NULL;
			rtd->dma_tx_des[i] = NULL;
			rtd->cookie[i] = 0;
		}
	}

	if (rtd->dma_chn2) {
		pr_info("%s dma_chan2 release chan id = %d\n", __func__,
			rtd->dma_chn2->chan_id);
		dma_release_channel(rtd->dma_chn2);
		rtd->dma_chn2 = NULL;
		rtd->dma_tx_des2 = NULL;
		rtd->cookie2 = 0;
	}
	rtd->params = NULL;
	pcm_free_dma_linklist_cfg_ddr_s2_2(substream);
	sprd_pcm_free_dma_cfg(substream, DMA_STAGE_TWO);

ok_go_out:
	sp_asoc_pr_dbg("%s return %i\n", __func__, ret);

	return ret;
}

static DEFINE_SPINLOCK(dma_chanall_lslp_lock);
static int dma_chanall_lslp_ena(int enable)
{
	static int chanall_lslp_bypass;

	spin_lock(&dma_chanall_lslp_lock);
	if (enable) {
		if (++chanall_lslp_bypass == 1)
			arch_dma_chanall_lslp_ena(true);
	} else {
		if (chanall_lslp_bypass > 0) {
			if (--chanall_lslp_bypass == 0)
				arch_dma_chanall_lslp_ena(false);
		}
	}
	spin_unlock(&dma_chanall_lslp_lock);
	pr_info("dma_chanall_lslp_bypass REF: %d\n",
		chanall_lslp_bypass);

	return 0;
}

static int sprd_pcm_hw_params1(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_pcm_dma_params *dma_data;
	dma_addr_t dma_buff_phys[SPRD_PCM_CHANNEL_MAX];
	int ret = 0;
	int i = 0;
	u32 ch_cnt;
	u32 linklist_node_size = 0;
	u32 flag;
	void *callback = NULL;
	u32 fragments_len = 0;
	u32 block_len = 0;
	u32 trans_len = 0;
	u32 node_count = 0;
	u32 node_bytes = 0;
	size_t totsize = params_buffer_bytes(params);
	size_t period = params_period_bytes(params);
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	u32 src_step = 0;
	u32 dst_step = 0;
	u32 step = 0;
	int direction;
	unsigned long source_addr_p = 0;
	unsigned long dst_addr_p = 0;
	struct snd_dma_buffer *dma_buffer;
	struct dma_async_tx_descriptor *tmp_tx_des;
	struct dma_chan *temp_dma_chan;
	struct platform_pcm_priv *priv_data;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s priv_data is null\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_info("(pcm) %s, cpudai_id=%d\n", __func__,
			srtd->cpu_dai->id);
	dma_data = snd_soc_dai_get_dma_data(srtd->cpu_dai, substream);
	if (!dma_data)
		goto no_dma;

	ch_cnt = used_hw_chan(substream, params);
	rtd->hw_chan = ch_cnt;
	sp_asoc_pr_info("chan=%d totsize=%#zx period=%#zx\n", ch_cnt,
			totsize, period);
	node_count = totsize / period;
	if (ch_cnt > SPRD_PCM_CHANNEL_MAX) {
		pr_err("ERR: channel count(%d) is greater than %d\n",
		       ch_cnt, SPRD_PCM_CHANNEL_MAX);
		return -EINVAL;
	}

	/* this may get called several times by oss emulation
	 * with different params
	 */
	if (!rtd->params) {
		rtd->params = dma_data;
		ret = sprd_pcm_request_dma_channel(substream,
						   srtd->platform->dev->of_node,
						   rtd, ch_cnt);
		if (ret)
			goto hw_param_err;
	}

	/* alloc dma linklist config buffer */
	dma_buffer = &substream->dma_buffer;
	pr_info("%s dma_buffer->dev.type %d\n", __func__, dma_buffer->dev.type);
	if (dma_buffer->dev.type == SNDRV_DMA_TYPE_DEV_IRAM) {
		linklist_node_size = SPRD_AUDIO_DMA_NODE_SIZE;
		if (is_playback) {
			if (srtd->cpu_dai->id == VBC_DAI_NORMAL) {
				pcm_set_dma_linklist_cfg_iram_s1(
					substream, linklist_node_size,
					PLY_NORMAL);
			} else {
				pcm_set_dma_linklist_cfg_iram_s1(
					    substream,
					    linklist_node_size,
					    PLY_DEEPBUF);
			}
		}
	} else {
		linklist_node_size = sizeof(struct sprd_dma_cfg) +
			   node_count * sizeof(struct scatterlist);
		rtd->linklist_node_size = linklist_node_size;
		pcm_alloc_dma_linklist_cfg_ddr(substream, linklist_node_size);
	}

	ret =
		sprd_pcm_alloc_dma_cfg(substream, DMA_STAGE_ONE,
				       linklist_node_size,
			0);
	if (ret < 0) {
		pr_err("failed %s %d\n", __func__, __LINE__);
		goto hw_param_err;
	}
	pr_info("linklist_node_size = %#x\n", linklist_node_size);
	ret = 0;

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
		rtd->dma_cb_ptr[i]->dma_chn = rtd->dma_chn[i];
		rtd->dma_cb_ptr[i]->substream = substream;
		dma_buff_phys[i] = runtime->dma_addr + i * rtd->dma_addr_offset;
	}
	normal_dma_protect_spin_unlock(substream);
	normal_dma_protect_mutex_unlock(substream);
	for (i = 0; i < ch_cnt; i++)
		pr_info("dma_buff_phys[%d] %#x\n", i,
			(u32)dma_buff_phys[i]);
	fragments_len = dma_data->desc.fragmens_len *
	    ((dma_data->desc.datawidth == DMA_SLAVE_BUSWIDTH_2_BYTES) ? 2 : 4);
	block_len = period / ch_cnt;
	trans_len = block_len;
	node_bytes = period / ch_cnt;
	/* to do i2s */
	rtd->interleaved = (ch_cnt == 2) &&
		    sprd_pcm_is_interleaved(runtime);

	if (is_playback) {
		src_step =
		    dma_step_cal(dma_data->desc.datawidth, ch_cnt,
				 sprd_pcm_is_interleaved(runtime));
		dst_step = 0;
		step = src_step;
		direction = DMA_MEM_TO_DEV;
	} else {
		src_step = 0;
		dst_step =
		    dma_step_cal(dma_data->desc.datawidth, ch_cnt,
				 sprd_pcm_is_interleaved(runtime));
		step = dst_step;
		direction = DMA_DEV_TO_MEM;
	}

	if (!(params->flags & SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP)) {
		callback = sprd_pcm_dma_buf_done;
		flag = SPRD_DMA_FLAGS(0,
				      0, SPRD_DMA_FRAG_REQ, SPRD_DMA_TRANS_INT);
	} else {
		flag = SPRD_DMA_FLAGS(0,
				      0, SPRD_DMA_FRAG_REQ, SPRD_DMA_NO_INT);
	}
	rtd->dma_callback1_func[0] = callback;
	rtd->dma_callback1_func[1] = callback;
	normal_dma_protect_mutex_lock(substream);
	/*
	 * if PM_POST_SUSPEND resumed the dma_chn has become null,
	 * so add protected code here.
	 */
	if (sprd_is_normal_playback(srtd->cpu_dai->id, substream->stream) &&
	    !rtd->dma_chn[0]) {
		normal_dma_protect_mutex_unlock(substream);
		pr_err("%s dam_chan is null for normalplayback\n", __func__);
		goto hw_param_err;
	}

	for (i = 0; i < ch_cnt; i++) {
		if (is_playback) {
			source_addr_p = dma_buff_phys[i];
			dst_addr_p = dma_data->dev_paddr[i];
		} else {
			source_addr_p = dma_data->dev_paddr[i];
			dst_addr_p = dma_buff_phys[i];
		}
		pr_info("%s chanel[%d], is_plabak=%d\n", __func__, i,
			is_playback);

		ret = dma_cfg_set(rtd->dma_config_ptr[i], source_addr_p,
				  dst_addr_p, 0,
			    (unsigned long)rtd->dma_linklist_cfg_phy[i],
			    (unsigned long)rtd->dma_linklist_cfg_virt[i],
			    step, dma_data->desc.datawidth,
			    fragments_len, trans_len,
			    dma_data->channels[i], 0, node_count, 0,
			    node_bytes, DMA_LEVEL_ONESTAGE, flag, direction);
		if (ret < 0) {
			normal_dma_protect_mutex_unlock(substream);
			goto hw_param_err;
		}
		tmp_tx_des =
			dma_cfg_hw(rtd->dma_chn[i], rtd->dma_config_ptr[i],
				   rtd->dma_callback1_func[i],
				   rtd->dma_cb_ptr[i]);
		if (!tmp_tx_des) {
			normal_dma_protect_mutex_unlock(substream);
			pr_err("%s, dma_cfg_hw failed!\n", __func__);
			goto hw_param_err;
		}
		normal_dma_protect_spin_lock(substream);
		rtd->dma_tx_des[i] = tmp_tx_des;
		normal_dma_protect_spin_unlock(substream);
	}
	normal_dma_protect_mutex_unlock(substream);
	goto ok_go_out;

no_dma:
	sp_asoc_pr_dbg("no dma\n");
	rtd->params = NULL;
	snd_pcm_set_runtime_buffer(substream, NULL);
	runtime->dma_bytes = 0;

	return ret;
hw_param_err:
	pr_err("ERR:%s line[%d] failed!\n", __func__, __LINE__);
	normal_dma_protect_mutex_lock(substream);
	for (i = 0; i < rtd->hw_chan; i++) {
		if (rtd->dma_chn[i]) {
			pr_err("%s chan_id %d, channels %d\n",
			       __func__, rtd->dma_chn[i]->chan_id,
				rtd->params->channels[i]);
			temp_dma_chan = rtd->dma_chn[i];
			normal_dma_protect_spin_lock(substream);
			rtd->dma_chn[i] = NULL;
			rtd->dma_tx_des[i] = NULL;
			rtd->cookie[i] = 0;
			normal_dma_protect_spin_unlock(substream);
			dma_release_channel(temp_dma_chan);
		}
	}
	normal_dma_protect_mutex_unlock(substream);
	rtd->params = NULL;
	pcm_free_dma_linklist_cfg_ddr(substream);
	sprd_pcm_free_dma_cfg(substream, DMA_STAGE_ONE);
ok_go_out:
	pr_info("%s return %i\n", __func__, ret);

	return ret;
}

static int sprd_pcm_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;

	if (is_use_2stage_dma(srtd, substream->stream)) {
		pr_info("%s use 2 stage dma\n", __func__);
		return sprd_pcm_hw_params_2stage(substream, params);
	}
	pr_info("%s use 1 stage dma\n", __func__);

	return sprd_pcm_hw_params1(substream, params);
}

#define SPRD_PCM_WAIT_CM4_CNT 2000
static int sprd_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct sprd_runtime_data *rtd = substream->runtime->private_data;
	struct sprd_pcm_dma_params *dma = rtd->params;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	int i;
	bool use_2stage_dma = is_use_2stage_dma(srtd, substream->stream);
	struct platform_pcm_priv *priv_data = NULL;
	struct dma_chan *temp_dma_chan;
	int wait_cm4_cnt;

	pr_info("%s\n", __func__);
	if (!dma)
		return 0;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return -1;
	}
	if (use_2stage_dma) {
		if (substream->dma_buffer.dev.type != SNDRV_DMA_TYPE_DEV_IRAM)
			pcm_free_dma_linklist_cfg_ddr_s2_2(substream);
		sprd_pcm_free_dma_cfg(substream, DMA_STAGE_TWO);
	} else {
		if (substream->dma_buffer.dev.type != SNDRV_DMA_TYPE_DEV_IRAM)
			pcm_free_dma_linklist_cfg_ddr(substream);
		sprd_pcm_free_dma_cfg(substream, DMA_STAGE_ONE);
	}
	snd_pcm_set_runtime_buffer(substream, NULL);
	if (is_use_2stage_dma(srtd, substream->stream)) {
		sprd_pcm_set_sync_ap(substream, 0);
		wait_cm4_cnt = 0;
		while (sprd_pcm_get_sync_cm4(substream) != 0 &&
		       ++wait_cm4_cnt < SPRD_PCM_WAIT_CM4_CNT)
			usleep_range(10, 20);
		if (wait_cm4_cnt == SPRD_PCM_WAIT_CM4_CNT) {
			if (sprd_pcm_get_sync_cm4(substream) != 0) {
				pr_err("ASOC: %s cm4 not release dma, sync_ap=%u sync_cm4=%u\n",
				       __func__,
					sprd_pcm_get_sync_ap(substream),
					sprd_pcm_get_sync_cm4(substream));
			}
		}
	}
	normal_dma_protect_mutex_lock(substream);
	for (i = 0; i < rtd->hw_chan; i++) {
		if (rtd->dma_chn[i]) {
			pr_info("%s chan_id %d, channels %d\n",
				__func__, rtd->dma_chn[i]->chan_id,
				rtd->params->channels[i]);
			temp_dma_chan = rtd->dma_chn[i];
			normal_dma_protect_spin_lock(substream);
			rtd->dma_chn[i] = NULL;
			rtd->dma_tx_des[i] = NULL;
			rtd->cookie[i] = 0;
			normal_dma_protect_spin_unlock(substream);
			dma_release_channel(temp_dma_chan);
		}
	}
	normal_dma_protect_mutex_unlock(substream);

	if (is_use_2stage_dma(srtd, substream->stream) && rtd->dma_chn2) {
		dma_release_channel(rtd->dma_chn2);
		rtd->dma_chn2 = NULL;
		rtd->dma_tx_des2 = NULL;
		rtd->cookie2 = 0;
	}
	if (is_use_2stage_dma(srtd, substream->stream)) {
		if (srtd->cpu_dai->id == VBC_DAI_NORMAL) {
			priv_data->pointer2_step_bytes[PLY_NORMAL] = 0;
			priv_data->chan_cnt[PLY_NORMAL] = 0;
			priv_data->width_in_bytes[PLY_NORMAL] = 0;
		} else {
			priv_data->pointer2_step_bytes[PLY_DEEPBUF] = 0;
			priv_data->chan_cnt[PLY_DEEPBUF] = 0;
			priv_data->width_in_bytes[PLY_DEEPBUF] = 0;
		}
	}

	if (is_use_2stage_dma(srtd, substream->stream)) {
		priv_data->dma_2stage_level_1_ap_int_count = 0;
		if (srtd->cpu_dai->id == VBC_DAI_NORMAL)
			reset_dma_level1_int_count(substream,
						   PLY_NORMAL);
		else if (srtd->cpu_dai->id == VBC_DAI_DEEP_BUF) {
			reset_dma_level1_int_count(substream,
						   PLY_DEEPBUF);
		} else {
			pr_err("%s, unknown cpu_dai[%d]\n", __func__,
			       srtd->cpu_dai->id);
			return -EINVAL;
		}
	}

	rtd->params = NULL;

	if (is_use_2stage_dma(srtd, substream->stream))
		sprd_pcm_proc_done(substream);

	return 0;
}

static int sprd_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static void init_iram_data_2stage(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	struct sprd_pcm_dma_params *dma = rtd->params;
	u8 *iram_addr_virt = rtd->iram_virt_addr;
	u8 *ddr_addr_virt = runtime->dma_area;
	u32 transction2 = dma->desc2.transcation_len;

	/*transaction2 size must == iram data size */
	if (transction2 <= rtd->iram_data_max_s2_1) {
		pr_warn("%s transction2(%#x) > rtd->iram_data_max_s2_1(%#x)\n",
			__func__, transction2, rtd->iram_data_max_s2_1);
		memcpy(iram_addr_virt, ddr_addr_virt, transction2);
	}
	pr_info("%s level1_iram_addr_virt =%p , level2_ddr_addr_virt=%p, transction2=%#x, iram_data_max_s2_1=%#x, iram_phy_addr=%#x,dma_addr_phy=%#zx\n",
		__func__, iram_addr_virt, ddr_addr_virt, transction2,
	     rtd->iram_data_max_s2_1,
	     rtd->iram_phy_addr_ap, (size_t)runtime->dma_addr);
}

static int sprd_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct sprd_runtime_data *rtd = substream->runtime->private_data;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_pcm_dma_params *dma = rtd->params;
	int ret = 0;
	int i;
	struct platform_pcm_priv *priv_data = NULL;
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();
	sp_asoc_pr_info("%s, %s cpu_dai->id = %d Trigger %s cmd:%d\n", __func__,
			sprd_dai_pcm_name(srtd->cpu_dai), srtd->cpu_dai->id,
			PCM_DIR_NAME(substream->stream), cmd);
	if (!dma) {
		sp_asoc_pr_info("no trigger");
		return 0;
	}
	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return -1;
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (is_use_2stage_dma(srtd, substream->stream)) {
			/*copy form ddr to iram */
			init_iram_data_2stage(substream);
			if (!rtd->dma_tx_des2) {
				rtd->dma_tx_des2 =
					dma_cfg_hw(rtd->dma_chn2,
					rtd->dma_cfg_buf2,
					rtd->dma_callback2_func,
					rtd->dma_callback_data2);
			}
			if (rtd->dma_tx_des2)
				rtd->cookie2 =
					dmaengine_submit(rtd->dma_tx_des2);
			else {
				pr_err("%s, dma2_cfg_hw failed!\n", __func__);
				return -ENOMEM;
			}
		} else {
			dma_chanall_lslp_ena(true);
		}
		normal_dma_protect_spin_lock(substream);
		for (i = 0; i < rtd->hw_chan; i++) {
			if (!rtd->dma_tx_des[i]) {
				rtd->dma_tx_des[i] =
				dma_cfg_hw(rtd->dma_chn[i],
					rtd->dma_config_ptr[i],
					rtd->dma_callback1_func[i],
					rtd->dma_cb_ptr[i]);
				if (!rtd->dma_tx_des[i]) {
					pr_err("%s, dma_cfg_hw :%d failed!\n",
						__func__, i);
					return -ENOMEM;
				}
			}
			rtd->cookie[i] =
			    dmaengine_submit(rtd->dma_tx_des[i]);
		}

		if (rtd->dma_chn2)
			dma_async_issue_pending(rtd->dma_chn2);

		for (i = 0; i < rtd->hw_chan; i++) {
			if (rtd->dma_chn[i])
				dma_async_issue_pending(rtd->dma_chn[i]);
		}
		normal_dma_protect_spin_unlock(substream);
		pr_info("S\n");
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		normal_dma_protect_spin_lock(substream);
		if (!rtd->dma_chn[0]) {
			normal_dma_protect_spin_unlock(substream);
			pr_info("%s dma_chn[0] is null\n",
				__func__);
			return 0;
		}
		for (i = 0; i < rtd->hw_chan; i++) {
			if (rtd->dma_chn[i]) {
				dmaengine_terminate_all(rtd->dma_chn[i]);
				rtd->dma_tx_des[i] = NULL;
			}
		}
		normal_dma_protect_spin_unlock(substream);
		if (rtd->dma_chn2) {
			dmaengine_terminate_all(rtd->dma_chn2);
			rtd->dma_tx_des2 = NULL;
		}
		rtd->cb_called = 0;
		if (is_use_2stage_dma(srtd, substream->stream) == false)
			dma_chanall_lslp_ena(false);
		pr_info("E\n");
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static u32 get_dma_used_total_cnt(struct platform_pcm_priv *priv_data,
				  int normal)
{
	u32 offset_total = 0;
	u32 bytes = 0;
	u32 idx_cnt = 0;
	u32 frames = 0;

	if (!priv_data) {
		pr_err("%s %d platform_get_drvdata failed\n",
		       __func__, __LINE__);
		return 0;
	}

	if (normal == PLY_NORMAL)
		offset_total = TOTAL_CNT_OFFSET_NORMAL;
	else
		offset_total = TOTAL_CNT_OFFSET_DEPBUF;

	idx_cnt = readl_relaxed((void *)priv_data->iram_4arm7_virt_addr +
			offset_total);
	bytes = idx_cnt *  priv_data->pointer2_step_bytes[normal];
	if (priv_data->chan_cnt[normal] &&
	    priv_data->width_in_bytes[normal]){
		frames = bytes / priv_data->chan_cnt[normal] /
			priv_data->width_in_bytes[normal];
	} else {
		pr_warn("%s chan =%#x, width_in_bytes=%#x\n",
			__func__,  priv_data->chan_cnt[normal],
			priv_data->width_in_bytes[normal]);
		frames = 0;
	}

	return frames;
}

static inline u32 get_dma_level1_int_count(struct snd_pcm_substream
						*substream, int normal)
{
	u32 offset = (normal == PLY_NORMAL) ? 0 : 8;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct platform_pcm_priv *priv_data = NULL;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return -1;
	}

	return readl_relaxed((void *)priv_data->iram_4arm7_virt_addr + offset);
}

/* pointer use interrupt dma level1 */
static snd_pcm_uframes_t sprd_pcm_pointer_2stage(struct snd_pcm_substream
						 *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_runtime_data *rtd = runtime->private_data;
	u32 offset = 0;
	snd_pcm_uframes_t x = 0;
	snd_pcm_uframes_t app_pointer = 0;
	u32 cm4_dma_level1_buf_done_count = 0;
	struct platform_pcm_priv *priv_data = NULL;

	priv_data = snd_soc_platform_get_drvdata(srtd->platform);
	if (!priv_data) {
		pr_warn("%s platform priv_data set failed\n", __func__);
		return 0;
	}
	app_pointer = runtime->control->appl_ptr;

	if (priv_data->dma_2stage_level_1_int_source ==
	    DMA_2STAGE_INT_SOURCE_ARM) {
		if (srtd->cpu_dai->id == VBC_DAI_NORMAL) {
			cm4_dma_level1_buf_done_count =
			    get_dma_level1_int_count(substream, PLY_NORMAL);
		} else if (srtd->cpu_dai->id == VBC_DAI_DEEP_BUF) {
			cm4_dma_level1_buf_done_count =
			    get_dma_level1_int_count(substream, PLY_DEEPBUF);
		} else {
			pr_err("%s %d unknown cpu_dai[%d]\n", __func__,
			       __LINE__, srtd->cpu_dai->id);
			return 0;
		}
		if (priv_data->proc[PROC_POINTER_LOG] != 0) {
			pr_info("cpu_dai[%d]cm4_arm7_dma_level1_buf_done_count =%u\n",
				srtd->cpu_dai->id,
				cm4_dma_level1_buf_done_count);
			pr_info("app_pointer=%#lx, count_max=%d\n",
				app_pointer, rtd->pointer2_step_max);
		}
		offset =
		    rtd->pointer2_step_bytes * cm4_dma_level1_buf_done_count;
	} else if (priv_data->dma_2stage_level_1_int_source ==
		   DMA_2STAGE_INT_SOURCE_AP) {
		if (priv_data->proc[PROC_POINTER_LOG] != 0) {
			pr_info("cpu_dai[%d] dma_2stage_level_1_ap_int_count =%u, app_pointer=%#lx\n",
				srtd->cpu_dai->id,
			     priv_data->dma_2stage_level_1_ap_int_count,
			     app_pointer);
		}
		offset = rtd->pointer2_step_bytes *
		    priv_data->dma_2stage_level_1_ap_int_count;
	} else {
		pr_warn("%s, level_1_int_source[%#x] not supported\n",
			__func__, priv_data->dma_2stage_level_1_int_source);
	}
	x = bytes_to_frames(runtime, offset);
	if (x == runtime->buffer_size) {
		pr_debug("%s x == runtime->buffer_size == %#lx(frames)\n",
			 __func__, runtime->buffer_size);
		x = 0;
	}

	return x;
}

static snd_pcm_uframes_t
sprd_pcm_pointer_1stage(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sprd_runtime_data *rtd = runtime->private_data;
	snd_pcm_uframes_t x = 0;
	int now_pointer;
	int bytes_of_pointer = -1;
	int shift = 1;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;

	if (sprd_is_dfm(srtd->cpu_dai) || sprd_is_vaudio(srtd->cpu_dai)) {
		sp_asoc_pr_dbg("no pointer");
		return 0;
	}

	if (rtd->interleaved)
		shift = 0;
	normal_dma_protect_spin_lock(substream);
	if (rtd->dma_chn[0]) {
		now_pointer = sprd_pcm_dma_get_addr(rtd->dma_chn[0],
						    rtd->cookie[0], substream) -
		    runtime->dma_addr;
		bytes_of_pointer = now_pointer;
	}
	if (rtd->dma_chn[1]) {
		now_pointer = sprd_pcm_dma_get_addr(rtd->dma_chn[1],
						    rtd->cookie[1], substream) -
		    runtime->dma_addr - rtd->dma_addr_offset;
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
			if (rtd->dma_pos_wrapped[0] != rtd->dma_pos_wrapped[1]) {
				bytes_of_pointer =
				    max(bytes_of_pointer, now_pointer) << shift;
			} else {
				bytes_of_pointer =
				    min(bytes_of_pointer, now_pointer) << shift;
			}
		}
	}
	normal_dma_protect_spin_unlock(substream);
	x = bytes_to_frames(runtime, bytes_of_pointer);
	if (x == runtime->buffer_size)
		x = 0;

	return x;
}

static snd_pcm_uframes_t sprd_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *srtd = substream->private_data;

	if (is_use_2stage_dma(srtd, substream->stream))
		return sprd_pcm_pointer_2stage(substream);
	else
		return sprd_pcm_pointer_1stage(substream);
}

static int sprd_pcm_mmap(struct snd_pcm_substream *substream,
			 struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	int ret = 0;

	if (sprd_is_dfm(srtd->cpu_dai) || sprd_is_vaudio(srtd->cpu_dai)) {
		sp_asoc_pr_dbg("no mmap");
		return 0;
	}
	pr_info("%s line[%d] dma_addr=%#zx dma_area= %p, runtime->dma_bytes = %#zx",
		__func__, __LINE__, (size_t)runtime->dma_addr,
		runtime->dma_area, runtime->dma_bytes);

	pr_info("vm_start %#lx, vm_end %#lx, end-start =%#lx\n",
		vma->vm_start, vma->vm_end,
	     vma->vm_end - vma->vm_start);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret = remap_pfn_range(vma, vma->vm_start,
			      runtime->dma_addr >> PAGE_SHIFT,
			      vma->vm_end - vma->vm_start, vma->vm_page_prot);

	pr_info("%s line[%d] ret =%d\n", __func__, __LINE__, ret);

	return ret;
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

#ifdef CONFIG_ARM64
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wshift-count-overflow"
static u64 sprd_pcm_dmamask = DMA_BIT_MASK(64);
#pragma GCC diagnostic pop
#else
static u64 sprd_pcm_dmamask = DMA_BIT_MASK(32);
#endif

static void sprd_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *srtd = pcm->private_data;
	struct snd_card *card = srtd->card->snd_card;
	struct snd_soc_dai *cpu_dai = srtd->cpu_dai;
	struct snd_pcm_substream *substream = NULL;
	int i = 0;
	int ret = 0;

	pr_info("%s %s dai[%d]\n",
		__func__, sprd_dai_pcm_name(cpu_dai), cpu_dai->id);
	if (!card->dev->dma_mask)
		card->dev->dma_mask = &sprd_pcm_dmamask;
#ifdef CONFIG_ARM64
	card->dev->coherent_dma_mask = DMA_BIT_MASK(64);
#else
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);
#endif

	for (i = SNDRV_PCM_STREAM_PLAYBACK; i <= SNDRV_PCM_STREAM_LAST; i++) {
		substream = srtd->pcm->streams[i].substream;
		if (!substream)
			continue;
		pcm_preallocate_dma_buffer_free(substream);
	}

	pr_info("ret = %d\n", ret);
}

static int sprd_pcm_new(struct snd_soc_pcm_runtime *srtd)
{
	struct snd_card *card = srtd->card->snd_card;
	struct snd_soc_dai *cpu_dai = srtd->cpu_dai;
	struct snd_pcm_substream *substream = NULL;
	int i = 0;
	int ret = 0;

	pr_info("%s %s daiid[%d]\n", __func__,
		sprd_dai_pcm_name(cpu_dai), cpu_dai->id);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &sprd_pcm_dmamask;
#ifdef CONFIG_ARM64
	card->dev->coherent_dma_mask = DMA_BIT_MASK(64);
#else
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);
#endif
	for (i = SNDRV_PCM_STREAM_PLAYBACK; i <= SNDRV_PCM_STREAM_LAST; i++) {
		substream = srtd->pcm->streams[i].substream;
		if (!substream)
			continue;
		ret = pcm_preallocate_dma_buffer(srtd, i);
		if (ret)
			goto err;
	}

	pr_info("ret = %d\n", ret);

	return 0;
err:
	pr_err("ERR:%s line[%d] alloc failed, cpudai[%d], stream[%d]\n",
	       __func__, __LINE__, cpu_dai->id, i);
	sprd_pcm_free_dma_buffers(srtd->pcm);
	ret = -ENOMEM;

	return ret;
}

s32 platform_pcm_parse_dt(struct device_node *node,
			  struct platform_pcm_priv *priv_data)
{
	s32 ret = 0;
	u32 dma_2stage_usecase = 0;
	u32 node_count_2stage_level1 = 0;
	u32 dma_2stage_level_1_int_source = 0;

	pr_debug("%s %d\n", __func__, __LINE__);
	if (!node) {
		pr_err("ERR: device_node is NULL!\n");
		return -1;
	}
	ret = of_property_read_u32(node,
				   "sprd,dma-2stage-usecase",
				   &dma_2stage_usecase);
	if (ret) {
		pr_err("%s, parse 'sprd,dma-2stage-dma-usecase' failed!\n",
		       __func__);
		return -EINVAL;
	}
	ret = of_property_read_u32(node,
				   "sprd,node-count-2stage-level-1",
				   &node_count_2stage_level1);
	if (ret) {
		pr_err("%s, parse 'sprd,node-count-2stage-level-1' failed!\n",
		       __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "sprd,dma-2stage-level-1-int-source",
				   &dma_2stage_level_1_int_source);
	if (ret) {
		pr_err("%s, parse 'sprd,iram-restore' failed!\n", __func__);
		return -EINVAL;
	}

	priv_data->use_2stage_dma_case = dma_2stage_usecase;
	priv_data->node_count_2stage_level1 = node_count_2stage_level1;
	priv_data->dma_2stage_level_1_int_source =
	    dma_2stage_level_1_int_source;
	pr_info("%s audio use_2stage_dma_case = %#x, node_count_2stage_level1=%#x\n",
		__func__, priv_data->use_2stage_dma_case,
	     priv_data->node_count_2stage_level1);
	pr_info("audio dma_2stage_level_1_int_source=%#x\n",
		priv_data->dma_2stage_level_1_int_source);

	return 0;
}

#ifdef CONFIG_PROC_FS
static void sprd_pcm_platform_proc_write(struct snd_info_entry *entry,
					 struct snd_info_buffer *buffer)
{
	char line[64] = { 0 };
	char name[128] = { 0 };
	unsigned int val;
	struct snd_soc_platform *platform = entry->private_data;
	struct platform_pcm_priv *priv_data = NULL;

	priv_data = snd_soc_platform_get_drvdata(platform);
	if (!priv_data) {
		pr_warn("%s %s set failed\n", __func__, name);
		return;
	}

	while (!snd_info_get_line(buffer, line, sizeof(line))) {
		if (sscanf(line, "%s %x", name, &val) != 2)
			continue;
		if (strcmp(name, "2stagedmacase") == 0) {
			pr_info("2stagedmacase=%#x, it will not allowed to change,",
				priv_data->use_2stage_dma_case);
			pr_info("because alloc mem for 2 stage dma move to pcm new\n");
		}
		if (strcmp(name, "pointer_log") == 0) {
			priv_data->proc[PROC_POINTER_LOG] = val;
			pr_info("pointer_log =%#x\n",
				priv_data->proc[PROC_POINTER_LOG]);
		}
		/* we can extend it */
		pr_err("%s, name[%s]:val[%#x]\n", __func__, name, val);
	}
}

static void sprd_pcm_platform_proc_read(struct snd_info_entry *entry,
					struct snd_info_buffer *buffer)
{
	struct snd_soc_platform *platform = entry->private_data;
	struct platform_pcm_priv *priv_data = NULL;
	int i = 0;

	priv_data = snd_soc_platform_get_drvdata(platform);
	if (!priv_data) {
		pr_warn("%s  priv_data null\n", __func__);
		return;
	}
	snd_iprintf(buffer, "2stagedmacase %#x\n",
		    priv_data->use_2stage_dma_case);
	snd_iprintf(buffer, "pointer_log ");
	for (i = 0; i < PROC_MAX; i++)
		snd_iprintf(buffer, " %#x", priv_data->proc[i]);
	snd_iprintf(buffer, "\n:");
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

#ifdef CONFIG_SND_VERBOSE_PROCFS

/* for 2stage dma */
static void sprd_pcm_proc_read(struct snd_info_entry *entry,
			       struct snd_info_buffer *buffer)
{
	struct snd_pcm_substream *substream = NULL;
	struct snd_pcm_runtime *runtime = NULL;
	struct sprd_runtime_data *rtd = NULL;
	struct snd_soc_pcm_runtime *srtd = NULL;
	u32 now_pointer = 0;

	substream = entry->private_data;
	if (substream) {
		runtime = substream->runtime;
		srtd = substream->private_data;
	} else {
		return;
	}
	if (runtime)
		rtd = runtime->private_data;
	else
		return;
	if (!rtd)
		return;

	/* dma address */
	if (rtd->dma_chn[0]) {
		now_pointer = sprd_pcm_dma_get_addr(rtd->dma_chn[0],
						    rtd->cookie[0], substream);
		snd_iprintf(buffer,
			    "level_1 leftchannel: channel_id=[%d] uid [%d] now_pointer=%#x\n",
			rtd->dma_chn[0]->chan_id, rtd->params->channels[0],
			now_pointer);
	}
	if (rtd->dma_chn[1]) {
		now_pointer = sprd_pcm_dma_get_addr(rtd->dma_chn[1],
						    rtd->cookie[1], substream);
		snd_iprintf(buffer,
			    "levle1 rightchannel: channel_id=[%d] uid [%d] now_pointer=%#x\n",
			rtd->dma_chn[1]->chan_id, rtd->params->channels[1],
			now_pointer);
	}
	if (rtd->dma_chn2) {
		now_pointer = sprd_pcm_dma_get_addr(rtd->dma_chn2,
						    rtd->cookie2, substream);
		snd_iprintf(buffer,
			    "level2: channel_id=[%d] need not uid, now_pointer=%#x\n",
			rtd->dma_chn2->chan_id, now_pointer);
	}
}

static void sprd_pcm_proc_init(struct snd_pcm_substream *substream)
{
	struct snd_info_entry *entry;
	struct snd_pcm_str *pstr = substream->pstr;
	struct snd_pcm *pcm = pstr->pcm;
	struct sprd_runtime_data *rtd = substream->runtime->private_data;

	entry = snd_info_create_card_entry(pcm->card, "DMA_STAGE2",
					   pstr->proc_root);
	if (entry) {
		snd_info_set_text_ops(entry, substream, sprd_pcm_proc_read);
		if (snd_info_register(entry) < 0) {
			snd_info_free_entry(entry);
			entry = NULL;
		}
	}
	rtd->proc_info_entry = entry;
}

static void sprd_pcm_proc_done(struct snd_pcm_substream *substream)
{
	struct sprd_runtime_data *rtd = substream->runtime->private_data;

	snd_info_free_entry(rtd->proc_info_entry);
	rtd->proc_info_entry = NULL;
}

#else
static void sprd_pcm_proc_init(struct snd_pcm_substream *substream)
{
}

static void sprd_pcm_proc_done(struct snd_pcm_substream *substream)
{
}

#endif

static int aud_init_iram_addr(struct snd_soc_platform *platform)
{
	struct platform_pcm_priv *priv_data = NULL;
	u32 offset = 0;

	priv_data = snd_soc_platform_get_drvdata(platform);
	if (!priv_data) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return -1;
	}

	priv_data->iram_phy_addr = audio_mem_alloc(IRAM_BASE,
						   &priv_data->iram_size);
	if (priv_data->iram_size > 0) {
		priv_data->iram_virt_addr =
		    (char *)audio_mem_vmap(priv_data->iram_phy_addr,
						 priv_data->iram_size,
						 1);
		if (!priv_data->iram_virt_addr) {
			pr_err("%s %d failed\n", __func__, __LINE__);
			return -1;
		}
		memset((void *)priv_data->iram_virt_addr, 0,
			  priv_data->iram_size);
	}

	priv_data->iram_normal_phy_addr =
	    audio_mem_alloc(IRAM_NORMAL, &priv_data->iram_normal_size);
	offset = (priv_data->iram_normal_phy_addr - priv_data->iram_phy_addr);
	priv_data->iram_normal_virt_addr = priv_data->iram_virt_addr + offset;

	priv_data->iram_deepbuf_phy_addr =
	    audio_mem_alloc(IRAM_DEEPBUF, &priv_data->iram_deepbuf_size);
	offset = (priv_data->iram_deepbuf_phy_addr - priv_data->iram_phy_addr);
	priv_data->iram_deepbuf_virt_addr = priv_data->iram_virt_addr + offset;

	priv_data->iram_4arm7_phy_addr =
	    audio_mem_alloc(IRAM_4ARM7, &priv_data->iram_4arm7_size);
	offset = (priv_data->iram_4arm7_phy_addr - priv_data->iram_phy_addr);
	priv_data->iram_4arm7_virt_addr = priv_data->iram_virt_addr + offset;
	pr_info("%s normal:phy[%#x] virt[%p] size=%#x, deepbuf:phy=%#x, virt[%p], size=%#x, arm7:phy=%#x, virt[%p], size=%#x\n",
		__func__, priv_data->iram_normal_phy_addr,
	     priv_data->iram_normal_virt_addr, priv_data->iram_normal_size,
	     priv_data->iram_deepbuf_phy_addr,
	     priv_data->iram_deepbuf_virt_addr, priv_data->iram_deepbuf_size,
		 priv_data->iram_4arm7_phy_addr,
		 priv_data->iram_4arm7_virt_addr,
	     priv_data->iram_4arm7_size);

	return 0;
}

static void aud_clear_iram_addr(struct snd_soc_platform *platform)
{
	struct platform_pcm_priv *priv_data = NULL;

	priv_data = snd_soc_platform_get_drvdata(platform);
	if (!priv_data) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return;
	}

	audio_mem_free(IRAM_BASE, priv_data->iram_phy_addr,
		       priv_data->iram_size);
	if (priv_data->iram_size > 0) {
		audio_mem_unmap(priv_data->iram_virt_addr);
		priv_data->iram_virt_addr = NULL;
		priv_data->iram_size = 0;
	}
}

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
		} else {
			pr_info("%s i=%d has released\n", __func__, i);
		}
	}
	mutex_unlock(&pm_dma->pm_mtx_dma_prot);
}

static int sprd_pcm_pm_notifier(struct notifier_block *notifier,
				unsigned long pm_event, void *unused)
{
	struct audio_pm_dma *pm_dma;

	pm_dma = get_pm_dma();
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pr_info("%s, PM_SUSPEND_PREPARE.\n", __func__);
		mutex_lock(&pm_dma->pm_mtx_cnt);
		if (pm_dma->no_pm_cnt == 0 && pm_dma->normal_rtd) {
			pm_normal_dma_chan_release(pm_dma->normal_rtd);
			vbc_clock_clear_force();
			pm_dma->vbc_forcec_cleared = true;
		}
		mutex_unlock(&pm_dma->pm_mtx_cnt);
		break;
	case PM_POST_SUSPEND:
		pr_info("%s, PM_POST_SUSPEND.\n", __func__);
		/* Just resum something about vbc. When system has resumed,
		 * HAL will get a xrun, and a 'close -> re-open' procedure
		 * will be done. Then the playback will be restored.
		 */
		mutex_lock(&pm_dma->pm_mtx_cnt);
		if (pm_dma->vbc_forcec_cleared) {
			vbc_clock_set_force();
			pm_dma->vbc_forcec_cleared = false;
		}
		mutex_unlock(&pm_dma->pm_mtx_cnt);
		break;
	default:
		pr_err("%s no corresponding processing", __func__);
		break;
	}

	return NOTIFY_DONE;
}

static struct audio_pm_dma *get_pm_dma(void)
{
	return pm_dma;
}

static void init_pm_dma(void)
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
}

static void *sprd_snd_ram_vmap(const struct device *dev, phys_addr_t start,
			       size_t size, unsigned int memtype)
{
	struct page **pages;
	phys_addr_t page_start = start - offset_in_page(start);
	unsigned int page_count =
		DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	if (memtype)
		prot = pgprot_noncached(PAGE_KERNEL);
	else
		prot = pgprot_writecombine(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		pr_err("vaddr failed!\n");
		return NULL;
	}
	memset_io(vaddr, 0, size);

	return vaddr + offset_in_page(start);
}

static int sprd_snd_platform_probe(struct snd_soc_platform *platform)
{
	struct platform_pcm_priv *priv_data = NULL;
	s32 ret = 0;
	struct regmap *pmu_apb_gpr;
	struct regmap *pmu_com_apb_gpr;
	struct device_node *memnp;
	struct resource res = { };
	static int sprd_snd_ram_vmap_done;
	static void *resddr_virt_addr_first;

	priv_data = snd_soc_platform_get_drvdata(platform);
	if (!priv_data) {
		pr_warn("%s %d priv_data failed use 1 stage dma\n",
			__func__, __LINE__);
		return -1;
	}
	if (priv_data->platform_type == PLATFORM_SHARKL2) {
		ret = platform_pcm_parse_dt(platform->dev->of_node, priv_data);
		if (ret < 0) {
			pr_err("%s line[%d] parse dt failed ret[%d]\n",
			       __func__, __LINE__, ret);
			return -1;
		}

		ret = aud_init_iram_addr(platform);
		if (ret < 0) {
			pr_err("%s line[%d] init iram addr failed\n",
			       __func__, __LINE__);
			return -1;
		}
		pmu_apb_gpr =
			syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							"sprd,syscon-pmu-apb");
		if (IS_ERR(pmu_apb_gpr)) {
			pr_warn("Get the 2stage dma pcm pmu apb syscon failed!\n");
			pmu_apb_gpr = NULL;
		} else {
			arch_audio_set_pmu_apb_gpr(pmu_apb_gpr);
		}
		pmu_com_apb_gpr =
			syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							"sprd,sys-aon-com-pmu-apb");
		if (IS_ERR(pmu_com_apb_gpr)) {
			pr_warn("Get the 2stage dma pcm pmu com apb syscon failed!\n");
			pmu_com_apb_gpr = NULL;
		} else {
			arch_audio_set_pmu_com_apb_gpr(pmu_com_apb_gpr);
		}
		/* Prepare for pm actions */
		pm_dma = devm_kzalloc(platform->dev, sizeof(*pm_dma),
				      GFP_KERNEL);
		if (!pm_dma)
			return -ENOMEM;
		init_pm_dma();

		memnp = of_parse_phandle(platform->dev->of_node,
					 "memory-region", 0);
		if (memnp) {
			ret = of_address_to_resource(memnp, 0, &res);
			if (ret != 0) {
				pr_err("of_address_to_resource failed!\n");
				return ret;
			}
			of_node_put(memnp);
			priv_data->dma_use_ddr_reserved = true;

			priv_data->resddr_phy_addr = (u32)res.start;
			priv_data->resddr_size = (u32)resource_size(&res);
			if (!sprd_snd_ram_vmap_done) {
				sprd_snd_ram_vmap_done = 1;
				resddr_virt_addr_first = sprd_snd_ram_vmap(
					platform->dev,
					priv_data->resddr_phy_addr,
					priv_data->resddr_size, 1);
				if (!resddr_virt_addr_first) {
					pr_err("resddr_virt_addr failed!\n");
					return -ENOMEM;
				}
				priv_data->resddr_virt_addr =
					resddr_virt_addr_first;
			} else {
				priv_data->resddr_virt_addr =
						resddr_virt_addr_first;
			}

			pr_info("resddr_phy_addr=%#x, resddr_size=%#x,resddr_virt_addr=%p\n",
				priv_data->resddr_phy_addr,
				priv_data->resddr_size,
				priv_data->resddr_virt_addr);

		} else {
			priv_data->dma_use_ddr_reserved = false;
		}
	} else {
		pr_err("%s %d unknown platform_type[%d]\n",
		       __func__, __LINE__,
		       (unsigned int)priv_data->platform_type);
		return -1;
	}

	sprd_pcm_platform_proc_init(platform);

	return 0;
}

static int sprd_snd_platform_remove(struct snd_soc_platform *platform)
{
	struct platform_pcm_priv *priv_data = NULL;
	static int sprd_snd_ram_vmap_free_done;

	priv_data = snd_soc_platform_get_drvdata(platform);
	if (!priv_data) {
		pr_warn("%s %d priv_data failed use 1 stage dma\n",
			__func__, __LINE__);
		return -1;
	}

	if (priv_data->platform_type == PLATFORM_SHARKL2) {
		aud_clear_iram_addr(platform);

		if (!sprd_snd_ram_vmap_free_done) {
			sprd_snd_ram_vmap_free_done = 1;
			iounmap(priv_data->resddr_virt_addr);
		}
	} else {
		pr_err("%s %d unknown platform_type[%d]\n",
		       __func__, __LINE__,
		       (unsigned int)priv_data->platform_type);
		return -1;
	}

	return 0;
}

static int pcm_total_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = NULL;
	struct platform_pcm_priv *priv_data = NULL;
	struct soc_mixer_control *mc =
	(struct soc_mixer_control *)kcontrol->private_value;
	int id = mc->shift;
	u32 total_frams = 0;

	platform = snd_soc_kcontrol_platform(kcontrol);
	if (!platform) {
		pr_err("%s platform null\n", __func__);
		return 0;
	}
	priv_data = snd_soc_platform_get_drvdata(platform);
	if (id == PLY_NORMAL)
		total_frams = get_dma_used_total_cnt(priv_data, PLY_NORMAL);
	else if (id == PLY_DEEPBUF)
		total_frams = get_dma_used_total_cnt(priv_data, PLY_DEEPBUF);
	else
		pr_err("%s invalid id\n", __func__);
	ucontrol->value.integer.value[0] = total_frams;
	pr_debug("id %d frames=%#x\n",
		 id, total_frams);

	return 0;
}

static const struct snd_kcontrol_new pcm_controls[] = {
	SOC_SINGLE_EXT("PCM_TOTAL_NORMAL", SND_SOC_NOPM, PLY_NORMAL,
		       MAX_32_BIT, 0,
		pcm_total_get, NULL),
	SOC_SINGLE_EXT("PCM_TOTAL_DEEPBUF", SND_SOC_NOPM, PLY_DEEPBUF,
		       MAX_32_BIT, 0,
	pcm_total_get, NULL),
};

static struct snd_soc_platform_driver sprd_soc_platform = {
	.ops = &sprd_pcm_ops,
	.pcm_new = sprd_pcm_new,
	.pcm_free = sprd_pcm_free_dma_buffers,
	.probe = sprd_snd_platform_probe,
	.remove = sprd_snd_platform_remove,
	.component_driver = {
		.controls = pcm_controls,
		.num_controls = ARRAY_SIZE(pcm_controls),
	},
};

#ifdef CONFIG_OF
static const struct of_device_id sprd_pcm_of_match[] = {
	{.compatible = "sprd,sharkl3-pcm-platform",
	 .data = (void *)PLATFORM_SHARKL2},
	{},
};

MODULE_DEVICE_TABLE(of, sprd_pcm_of_match);
#endif

static int sprd_soc_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct platform_pcm_priv *priv_data = NULL;
	const struct of_device_id *of_id;

	if (!node) {
		pr_err("ERR: device_node is NULL!\n");
		return -1;
	}
	of_id = of_match_node(sprd_pcm_of_match, pdev->dev.of_node);
	if (!of_id) {
		pr_err("%s line[%d] Get the pcm of device id failed!\n",
		       __func__, __LINE__);
		return -ENODEV;
	}
	priv_data = devm_kzalloc(&pdev->dev, sizeof(*priv_data), GFP_KERNEL);
	if (!priv_data)
		return -ENOMEM;

	priv_data->platform_type = (unsigned long)of_id->data;
	platform_set_drvdata(pdev, priv_data);
	pr_info("%s probed\n", __func__);

	return snd_soc_register_platform(&pdev->dev, &sprd_soc_platform);
}

static int sprd_soc_platform_remove(struct platform_device *pdev)
{
	int i;
	struct platform_pcm_priv *priv_data = NULL;

	for (i = 0; i < DMA_CHAN_MAX; i++) {
		if (!IS_ERR(dma_chan[i]))
			dma_release_channel(dma_chan[i]);
	}

	snd_soc_unregister_platform(&pdev->dev);
	priv_data = platform_get_drvdata(pdev);
	if (priv_data) {
		devm_kfree(&pdev->dev, priv_data);
		priv_data = NULL;
	}

	return 0;
}

static struct platform_driver sprd_pcm_driver = {
	.driver = {
		   .name = "sprd-pcm-audio",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(sprd_pcm_of_match),
		   },

	.probe = sprd_soc_platform_probe,
	.remove = sprd_soc_platform_remove,
};

module_platform_driver(sprd_pcm_driver);

MODULE_DESCRIPTION("SPRD ASoC PCM DMA");
MODULE_AUTHOR("Jian chen <jian.chen@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sprd-audio");
