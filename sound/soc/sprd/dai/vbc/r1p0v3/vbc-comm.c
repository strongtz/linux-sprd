/*
 * sound/soc/sprd/dai/vbc/r1p0v3/vbc-comm.c
 *
 * SPRD SoC VBC -- SpreadTrum SOC DAI for VBC Common function.
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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <sound/soc.h>

#include "sprd-asoc-common.h"
#include "vbc-comm.h"

static DEFINE_SPINLOCK(vbc_lock);

/* vbc local power suppliy and chan on */
static struct vbc_refcount {
	/*vbc reg power enable */
	atomic_t vbc_power_on;
	/*vbc enable */
	atomic_t vbc_on;
	atomic_t chan_on[VBC_IDX_MAX];
	atomic_t fifo_on[VBC_IDX_MAX];
	atomic_t prefill_data_ref;
	atomic_t vbc_eq_on;
	atomic_t vbc_sleep_xtl_on;
} vbc_refcnt;

struct sprd_vbc_src_reg_info {
	int reg;
	int clr_bit;
	int f1f2f3_bp_bit;
	int f1_sel_bit;
	int en_bit;
};

struct sprd_vbc_src_info {
	struct sprd_vbc_src_reg_info reg_info;
};

struct sprd_vbc_dac_src_reg_info {
	int reg;
	int clr_bit;
	int f0_bp_bit;
	int f0_sel_bit;
	int f1f2f3_bp_bit;
	int f1_sel_bit;
	int en_bit;
};

struct sprd_vbc_dac_src_info {
	struct sprd_vbc_dac_src_reg_info reg_info;
};

static struct sprd_vbc_src_info vbc_ad_src[VBC_AD_SRC_MAX] = {
	/* VBC_CAPTURE01 src */
	{ {REG_VBC_VBC_ADC_SRC_CTRL,
	  BIT_RF_ADC01_SRC_CLR,
	  BIT_RF_ADC01_SRC_F1F2F3_BP,
	  BIT_RF_ADC01_SRC_F1_SEL,
	  BIT_RF_ADC01_SRC_EN} },
	/* VBC_CAPTURE23 src */
	{ {REG_VBC_VBC_ADC_SRC_CTRL,
	  BIT_RF_ADC23_SRC_CLR,
	  BIT_RF_ADC23_SRC_F1F2F3_BP,
	  BIT_RF_ADC23_SRC_F1_SEL,
	  BIT_RF_ADC23_SRC_EN} },
};

static struct sprd_vbc_dac_src_info vbc_da_src = {
	{
	 REG_VBC_VBC_DAC_SRC_CTRL,
	 BIT_RF_DAC_SRC_CLR,
	 BIT_RF_DAC_SRC_F0_BP,
	 BIT_RF_DAC_SRC_F0_SEL,
	 BIT_RF_DAC_SRC_F1F2F3_BP,
	 BIT_RF_DAC_SRC_F1_SEL,
	 BIT_RF_DAC_SRC_EN,
	}
};

static const char *sprd_vbc_name[VBC_IDX_MAX] = {
	"DAC01",
	"DAC23",
	"ADC01",
	"ADC23",
};

static const char *sprd_vbc_dg_name[VBC_DG_MAX] = {
	"DAC_DG",
	"ADC01_DG",
	"ADC23_DG",
};

static const char *sprd_vbc_adsrc_name[VBC_AD_SRC_MAX] = {
	"ADC01_SRC",
	"ADC23_SRC",
};

static const char *sprd_vbc_eq_name[VBC_EQ_MAX] = {
	"DAC_EQ",
	"ADC01_EQ",
	"ADC23_EQ",
};

static int xtlbuf1_eb_set_cnt;
static DEFINE_SPINLOCK(xtlbuf1_eb_lock);


static int vbc_eb_set_cnt;
static DEFINE_SPINLOCK(vbc_module_lock);

static void vbc_da01_set_dgmixer_dg(u32 chan, u32 dg);
static void vbc_da23_set_dgmixer_dg(u32 chan, u32 dg);
static void vbc_da01_set_dgmixer_step(u32 step);

static inline int arch_audio_vbc_reset(void)
{
	int ret = 0;

	_arch_audio_vbc_reset();
	return ret;
}


int xtlbuf1_eb_set(void)
{

	spin_lock(&xtlbuf1_eb_lock);
	xtlbuf1_eb_set_cnt++;
	if (xtlbuf1_eb_set_cnt == 1) {
		pr_debug("%s xtlbuf1_eb_set_cnt=%d\n",
				__func__, xtlbuf1_eb_set_cnt);
		_xtlbuf1_eb_set();
	}
	spin_unlock(&xtlbuf1_eb_lock);

	return 0;
}

int xtlbuf1_eb_clr(void)
{

	spin_lock(&xtlbuf1_eb_lock);
	xtlbuf1_eb_set_cnt--;
	if (xtlbuf1_eb_set_cnt == 0) {
		pr_debug("%s  xtlbuf1_eb_set_cnt=%d\n",
			__func__, xtlbuf1_eb_set_cnt);
		_xtlbuf1_eb_clr();
	}
	spin_unlock(&xtlbuf1_eb_lock);

	return 0;
}
static int vbc_eb_set(void)
{
	spin_lock(&vbc_module_lock);
	vbc_eb_set_cnt++;
	if (vbc_eb_set_cnt == 1) {
		_vbc_eb_set();
		xtlbuf1_eb_set();
		/* a.vbc reset not depended on vbc enable.
		 * b. we reset vbc when vbc enalbe.
		 */
		arch_audio_vbc_reset();
		pr_debug("vbc enable and vbc reset\n");
	}
	spin_unlock(&vbc_module_lock);

	return 0;
}

static int vbc_eb_clear(void)
{
	spin_lock(&vbc_module_lock);
	vbc_eb_set_cnt--;
	if (vbc_eb_set_cnt == 0) {
		pr_debug("vbc disable\n");
		_vbc_eb_clear();
		xtlbuf1_eb_clr();
	}
	spin_unlock(&vbc_module_lock);

	return 0;
}

/* set/clear force used by pm notifier. */
int xtlbuf1_eb_set_force(void)
{
	pr_info("%s\n", __func__);
	spin_lock(&xtlbuf1_eb_lock);
	_xtlbuf1_eb_set();
	spin_unlock(&xtlbuf1_eb_lock);

	return 0;
}

int xtlbuf1_eb_clr_force(void)
{
	pr_info("%s\n", __func__);
	spin_lock(&xtlbuf1_eb_lock);
	_xtlbuf1_eb_clr();
	spin_unlock(&xtlbuf1_eb_lock);

	return 0;
}

void vbc_clock_set_force(void)
{
	spin_lock(&vbc_module_lock);
	_vbc_eb_set();
	xtlbuf1_eb_set_force();
	/* a.vbc reset not depended on vbc enable.
	 * b. we reset vbc when vbc enable.
	 */
	arch_audio_vbc_reset();
	spin_unlock(&vbc_module_lock);

	pr_info("vbc enable and vbc reset FORCE.\n");
}

void vbc_clock_clear_force(void)
{
	spin_lock(&vbc_module_lock);
	_vbc_eb_clear();
	xtlbuf1_eb_clr_force();
	spin_unlock(&vbc_module_lock);

	pr_info("vbc disable FORCE.\n");
}

static void vbc_module_clear(u32 id, enum VBC_MODULE module, u32 chan)
{
	u32 bit = 0;
	u32 reg = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}

	switch (module) {
	case DA_SRC:
		reg = REG_VBC_VBC_DAC_SRC_CTRL;
		bit = BIT_RF_DAC_SRC_CLR;
		break;
	case DA_DG:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_DAC_DG_CLR;
		break;
	case DA_EQ6:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_DAC_EQ6_CLR;
		break;
	case DA_ALC:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_DAC_ALC_CLR;
		break;
	case DA_EQ4:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_DAC_EQ4_CLR;
		break;
	case DA_DGMIXER:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_DAC_DGMIXER_CLR;
		break;
	case DA_NOISE_GEN:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_DAC_NGC_CLR;
		break;
	case DA_FIFO:
		reg = REG_VBC_VBC_MODULE_CLR;
		if ((id == VBC_PLAYBACK01) && AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_DAC0_FIFO_CLR;
		if ((id == VBC_PLAYBACK01) && AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_DAC1_FIFO_CLR;
		if ((id == VBC_PLAYBACK23) && AUDIO_IS_CHAN_L(chan))
			bit |= BIT_RF_DAC2_FIFO_CLR;
		if ((id == VBC_PLAYBACK23) && AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_DAC3_FIFO_CLR;
		break;
	case DA_IIS:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_DAC_IIS_CLR;
		break;
	case DA_IIS_AFIFO:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_DAC_IIS_FIFO_CLR;
		break;
	case DA_NCHFLT:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_DAC_NCHFLT_CLR;
		break;
	case DA_NCHTONE:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_DAC_NCHTONE_CLR;
		break;
	case AD_FIFO:
		reg = REG_VBC_VBC_MODULE_CLR;
		if (id == VBC_CAPTURE01 && AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_ADC0_FIFO_CLR;
		if (id == VBC_CAPTURE01 && AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_ADC1_FIFO_CLR;
		if (id == VBC_CAPTURE23 && AUDIO_IS_CHAN_L(chan))
			bit |= BIT_RF_ADC2_FIFO_CLR;
		if (id == VBC_CAPTURE23 && AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_ADC3_FIFO_CLR;
		break;
	case AD_IIS:
		reg = REG_VBC_VBC_MODULE_CLR;
		if (id == VBC_CAPTURE01)
			bit = BIT_RF_ADC01_IIS_CLR;
		else if (id == VBC_CAPTURE23)
			bit = BIT_RF_ADC23_IIS_CLR;
		else {
			pr_err("%s %d failed\n", __func__, __LINE__);
			return;
		}
		break;
	case AD_IIS_AFIFO:
		reg = REG_VBC_VBC_MODULE_CLR;
		if (id == VBC_CAPTURE01)
			bit = BIT_RF_ADC01_IIS_FIFO_CLR;
		else if (id == VBC_CAPTURE23)
			bit = BIT_RF_ADC23_IIS_FIFO_CLR;
		else {
			pr_err("%s %d failed\n", __func__, __LINE__);
			return;
		}
		break;
	case AD_EQ6:
		reg = REG_VBC_VBC_MODULE_CLR;
		if (id == VBC_CAPTURE01)
			bit = BIT_RF_ADC01_EQ6_CLR;
		else if (id == VBC_CAPTURE23)
			bit = BIT_RF_ADC23_EQ6_CLR;
		else {
			pr_err("%s %d failed\n", __func__, __LINE__);
			return;
		}
		break;
	case AD_DG:
		reg = REG_VBC_VBC_MODULE_CLR;
		if (id == VBC_CAPTURE01)
			bit = BIT_RF_ADC01_DG_CLR;
		else if (id == VBC_CAPTURE23)
			bit = BIT_RF_ADC23_DG_CLR;
		else {
			pr_err("%s %d failed\n", __func__, __LINE__);
			return;
		}
		break;
	case AD_SRC:
		reg = REG_VBC_VBC_ADC_SRC_CTRL;
		if (id == VBC_CAPTURE01)
			bit = BIT_RF_ADC01_SRC_CLR;
		else if (id == VBC_CAPTURE23)
			bit = BIT_RF_ADC23_SRC_CLR;
		else {
			pr_err("%s %d failed\n", __func__, __LINE__);
			return;
		}
		break;
	case SIDE_TONE:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_ST_CLR;
		break;
	case FM_MUTE:
		reg = REG_VBC_VBC_FM_MUTE_CTL;
		bit = BIT_RF_FM_MUTE_CLR;
		break;
	case SIDE_TONE_FIFO:
		reg = REG_VBC_VBC_MODULE_CLR;
		bit = BIT_RF_ST_FIFO_CLR;
		break;
	default:
		return;
	}
	pr_info("reg %#x, bit%#x\n", reg, bit);
	/* use vbc_reg_update other than vbc_reg_write
	 * because this function not only operate
	 * reg REG_VBC_VBC_MODULE_CLR. It also operates
	 * REG_VBC_VBC_FM_MUTE_CTL.
	 */
	vbc_reg_update(reg, bit, bit);
}

/* for safe, clear module after module enable except dac fifo
 * adc fifo, iis fifo which are async fifo
 * (read and write use different clock).
 * a. side tone fifo is not asyc fifo, we should clear it.
 * side tone enable in vbc_st_module_enable_event we clear it there.
 * b.eq alc moudle are alse used in "eq setting's user send", so do not
 * forget clear them.
 */
static void vbc_module_en(u32 id, enum VBC_MODULE module, u32 en, u32 chan)
{
	u32 bit = 0;
	u32 reg = 0;
	u32 val = 0;
	u32 need_clear = false;
	u32 old_val = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}

	switch (module) {
	case AD_SRC:
		need_clear = true;
		reg = REG_VBC_VBC_ADC_SRC_CTRL;
		if (id == VBC_CAPTURE01)
			bit = BIT_RF_ADC01_SRC_EN;
		else if (id == VBC_CAPTURE23)
			bit = BIT_RF_ADC23_SRC_EN;
		break;
	case AD_DG:
		need_clear = true;
		if (id == VBC_CAPTURE01) {
			reg = REG_VBC_VBC_ADC01_DG_CTRL;
			if (AUDIO_IS_CHAN_L(chan))
				bit = BIT_RF_ADC0_DG_EN;
			else if (AUDIO_IS_CHAN_R(chan))
				bit = BIT_RF_ADC1_DG_EN;
			else
				pr_err("%s %d failed\n", __func__, __LINE__);
		} else if (id == VBC_CAPTURE23) {
			reg = REG_VBC_VBC_ADC23_DG_CTRL;
			if (AUDIO_IS_CHAN_L(chan))
				bit = BIT_RF_ADC2_DG_EN;
			else if (AUDIO_IS_CHAN_R(chan))
				bit = BIT_RF_ADC3_DG_EN;
			else
				pr_err("%s %d failed\n", __func__, __LINE__);
		} else
			pr_err("%s %d failed\n", __func__, __LINE__);
		break;
	case AD_EQ6:
		need_clear = true;
		reg = REG_VBC_VBC_ADC_SRC_CTRL;
		if (id == VBC_CAPTURE01)
			bit = BIT_RF_ADC01_EQ6_EN;
		else if (id == VBC_CAPTURE23)
			bit = BIT_RF_ADC23_EQ6_EN;
		else
			pr_err("%s %d failed\n", __func__, __LINE__);
		break;
	case AD_FIFO:
		reg = REG_VBC_VBC_ENABLE_CTRL;
		if (id == VBC_CAPTURE01 && AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_ADC0_FIFO_EN;
		if (id == VBC_CAPTURE01 && AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_ADC1_FIFO_EN;
		if (id == VBC_CAPTURE23 && AUDIO_IS_CHAN_L(chan))
			bit |= BIT_RF_ADC2_FIFO_EN;
		if (id == VBC_CAPTURE23 && AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_ADC3_FIFO_EN;
		break;
	case DA_SRC:
		need_clear = true;
		bit = BIT_RF_DAC_SRC_EN;
		reg = REG_VBC_VBC_DAC_SRC_CTRL;
		break;
	case DA_DG:
		need_clear = false;
		if (AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_DAC0_DG_EN;
		if (AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_DAC1_DG_EN;
		reg = REG_VBC_VBC_DAC_DG_CTRL;
		break;
	case DA_EQ6:
		need_clear = true;
		bit = BIT_RF_DAC_EQ6_EN;
		reg = REG_VBC_VBC_DAC_HP_CTRL;
		break;
	case DA_ALC:
		need_clear = true;
		bit = BIT_RF_DAC_ALC_EN;
		reg = REG_VBC_VBC_DAC_HP_CTRL;
		break;
	case DA_EQ4:
		need_clear = true;
		bit = BIT_RF_DAC_EQ4_EN;
		reg = REG_VBC_VBC_DAC_HP_CTRL;
		break;
	case DA_DGMIXER:
		/* dac01 enable, da23 no mixer moudle need not enable */
		need_clear = false;
		if (AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_DAC0_DGMIXER_EN;
		if (AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_DAC1_DGMIXER_EN;
		reg = REG_VBC_VBC_DAC_DG_CTRL;
		break;
	case DA_NOISE_GEN:
		need_clear = true;
		bit = BIT_RF_DAC_NGC_EN;
		reg = REG_VBC_VBC_DAC_NGC_CTRL;
		break;
	case DA_FIFO:
		if (id == VBC_PLAYBACK01 && AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_DAC0_FIFO_EN;
		if (id == VBC_PLAYBACK01 && AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_DAC1_FIFO_EN;
		if (id == VBC_PLAYBACK23 && AUDIO_IS_CHAN_L(chan))
			bit |= BIT_RF_DAC2_FIFO_EN;
		if (id == VBC_PLAYBACK23 && AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_DAC3_FIFO_EN;
		reg = REG_VBC_VBC_ENABLE_CTRL;
		break;
	case DA_NCHTONE:
		need_clear = true;
		bit = BIT_RF_DAC_VT_TONE_EN | BIT_RF_DAC_PT_TONE_EN;
		reg = REG_VBC_VBC_DAC_TONE_GEN_EN;
		break;
	case DA_NCHFLT:
		need_clear = true;
		bit = BIT_RF_DAC_NCHFLT_EN |
		    BIT_RF_DAC_NCHFLT_BD0_EN_0 |
		    BIT_RF_DAC_NCHFLT_BD1_EN_0 |
		    BIT_RF_DAC_NCHFLT_BD2_EN_0 |
		    BIT_RF_DAC_NCHFLT_BD0_EN_1 |
		    BIT_RF_DAC_NCHFLT_BD1_EN_1 | BIT_RF_DAC_NCHFLT_BD2_EN_1;
		reg = REG_VBC_VBC_DAC_TONE_GEN_EN;
		break;
	default:
		return;
	}
	val = en ? bit : 0;
	if (en)
		old_val = vbc_reg_read(reg);
	vbc_reg_update(reg, val, bit);
	if (need_clear == true && en) {
		if (!(old_val & bit))
			vbc_module_clear(id, module, chan);
	}
}

static void vbc_da_clk_enable(u32 id, u32 en, u32 chan)
{
	unsigned int bit = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}

	switch (id) {
	case VBC_PLAYBACK01:
		if (AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_DAC0_DP_EN;
		if (AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_DAC1_DP_EN;
		break;
	case VBC_PLAYBACK23:
		if (AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_DAC2_DP_EN;
		if (AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_DAC3_DP_EN;
		break;
	default:
		return;
	}

	if (en)
		vbc_reg_update(REG_VBC_VBC_CHN_EN, bit, bit);
	else
		vbc_reg_update(REG_VBC_VBC_CHN_EN, ~bit, bit);
}

static void vbc_ad_clk_enable(u32 id, u32 en, u32 chan)
{
	unsigned int bit = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}

	switch (id) {
	case VBC_CAPTURE01:
		if (AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_ADC0_DP_EN;
		if (AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_ADC1_DP_EN;
		break;
	case VBC_CAPTURE23:
		if (AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_ADC2_DP_EN;
		if (AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_ADC3_DP_EN;
		break;
	default:
		return;
	}

	if (en)
		vbc_reg_update(REG_VBC_VBC_CHN_EN, bit, bit);
	else
		vbc_reg_update(REG_VBC_VBC_CHN_EN, ~bit, bit);
}

static inline int32_t vbc_idx_to_ad_src_idx(int vbc_idx)
{
	switch (vbc_idx) {
	case VBC_CAPTURE01:
		return VBC_AD01_SRC;
	case VBC_CAPTURE23:
		return VBC_AD23_SRC;
	case VBC_PLAYBACK01:
	case VBC_PLAYBACK23:
	default:
		return -1;
	}
}

static inline int32_t vbc_idx_to_eq_idx(int vbc_idx)
{
	switch (vbc_idx) {
	case VBC_PLAYBACK01:
	case VBC_PLAYBACK23:
		return VBC_DA_EQ;
	case VBC_CAPTURE01:
		return VBC_AD01_EQ;
	case VBC_CAPTURE23:
		return VBC_AD23_EQ;
	default:
		return -1;
		pr_err("invalid vbc_idx[%d] for %s\n", vbc_idx, __func__);
	}
}

static inline int32_t vbc_idx_to_dg_idx(int vbc_idx)
{
	switch (vbc_idx) {
	case VBC_PLAYBACK01:
	case VBC_PLAYBACK23:
		return VBC_DAC_DG;
	case VBC_CAPTURE01:
		return VBC_ADC01_DG;
	case VBC_CAPTURE23:
		return VBC_ADC23_DG;
	default:
		pr_err("invalid vbc_idx[%d] for %s\n", vbc_idx, __func__);
		return -1;
	}
}

static inline const char *vbc_get_eq_name(int vbc_eq_idx)
{
	return sprd_vbc_eq_name[vbc_eq_idx];
}

static inline const char *vbc_get_ad_src_name(int vbc_idx)
{
	return sprd_vbc_adsrc_name[vbc_idx];
}

static inline const char *vbc_get_dg_name(int dg_idx)
{
	return sprd_vbc_dg_name[dg_idx];
}

static inline const char *vbc_get_name(int vbc_idx)
{
	return sprd_vbc_name[vbc_idx];
}

static inline u32 vbc_reg_read(unsigned int reg)
{
	unsigned long ret = 0;

	vbc_eb_set();
	ret = readl_relaxed((void *__iomem)(reg - VBC_BASE + sprd_vbc_base));
	vbc_eb_clear();

	return ret;
}

static inline void vbc_reg_raw_write(unsigned int reg, uint32_t val)
{
	vbc_eb_set();
	writel_relaxed(val, (void *__iomem)(reg - VBC_BASE + sprd_vbc_base));
	vbc_eb_clear();
}

static int vbc_reg_write(unsigned int reg, uint32_t val)
{
	spin_lock(&vbc_lock);
	vbc_reg_raw_write(reg, val);
	spin_unlock(&vbc_lock);
	sp_asoc_pr_reg("VBC:[0x%04x] W:[0x%08x] R:[0x%08x]\n",
		       (reg)&0xFFFF, val, vbc_reg_read(reg));
	return 0;
}

/*
 * Returns 1 for change, 0 for no change, or negative error code.
 */
static int vbc_reg_update(unsigned int reg, uint32_t val, uint32_t mask)
{
	u32 new, old;

	spin_lock(&vbc_lock);
	old = vbc_reg_read(reg);
	new = (old & ~mask) | (val & mask);
	vbc_reg_raw_write(reg, new);
	spin_unlock(&vbc_lock);
	sp_asoc_pr_reg("[0x%04x] old:[0x%08x] U:[0x%08x] R:[0x%08x]\n",
		       reg & 0xFFFF, old, new, vbc_reg_read(reg));

	return old != new;
}

static inline void vbc_reg_enable(void)
{
	vbc_eb_set();
}

static inline void vbc_reg_disable(void)
{
	vbc_eb_clear();

}

static inline int arch_audio_vbc_switch(int master,
					unsigned int vbc_use_dma_type[],
					int vbc_idx_max)
{
	int ret = 0;

	switch (master) {
	case VBC_TO_AP_CTRL:
	case VBC_TO_WTLCP_CTRL:
	case VBC_TO_PUBCP_CTRL:
		arch_audio_vbc_reset();
		ret = _arch_audio_vbc_switch(master, vbc_use_dma_type,
					  vbc_idx_max);
		break;

	case VBC_NO_CHANGE:
		ret = _arch_audio_vbc_switch(master, vbc_use_dma_type,
					  vbc_idx_max);
		break;
	default:
		pr_err("ERR: %s, Invalid master(%d)!\n", __func__, master);
		return -ENODEV;
	}

	return ret;
}

static int vbc_power(int enable)
{
	atomic_t *vbc_power_on = &vbc_refcnt.vbc_power_on;

	if (enable) {
		atomic_inc(vbc_power_on);
		if (atomic_read(vbc_power_on) == 1) {
			vbc_reg_enable();
			pr_info("VBC Power On\n");
		}
	} else {
		if (atomic_dec_and_test(vbc_power_on)) {
			vbc_reg_disable();
			pr_info("VBC Power Off\n");
		}
		if (atomic_read(vbc_power_on) < 0)
			atomic_set(vbc_power_on, 0);
	}
	pr_info("VBC Power REF: %d", atomic_read(vbc_power_on));
	return 0;
}

static int vbc_sleep_xtl_en(bool enable)
{
	atomic_t *vbc_sleep_xtl_on = &vbc_refcnt.vbc_sleep_xtl_on;

	if (enable) {
		if (atomic_inc_return(vbc_sleep_xtl_on) == 1) {
			pr_info("XTL on when ap in deep sleep.\n");
			arch_audio_sleep_xtl_enable();
		}
	} else {
		if (atomic_dec_and_test(vbc_sleep_xtl_on)) {
			pr_info("XTL off when ap in deep sleep.\n");
			arch_audio_sleep_xtl_disable();
		}
		if (atomic_read(vbc_sleep_xtl_on) < 0)
			atomic_set(vbc_sleep_xtl_on, 0);
	}

	pr_info("VBC sleep XTL enable reference: %d",
		atomic_read(vbc_sleep_xtl_on));

	return 0;
}

static inline int vbc_da01_enable_raw(int enable, int chan)
{
	vbc_da_clk_enable(VBC_PLAYBACK01, enable, chan);

	return 0;
}

static inline int vbc_da23_enable_raw(int enable, int chan)
{
	vbc_da_clk_enable(VBC_PLAYBACK23, enable, chan);

	return 0;
}

static inline int vbc_ad01_enable_raw(int enable, int chan)
{
	if (enable)
		vbc_ad_clk_enable(VBC_CAPTURE01, enable, chan);
	else
		/*  to check why ? */
		pr_info("Not close %s chan[%d]!", __func__, chan);

	return 0;
}

static inline int vbc_ad23_enable_raw(int enable, int chan)
{
	if (enable)
		vbc_ad_clk_enable(VBC_CAPTURE23, enable, chan);
	else
		pr_info("Not close %s chan[%d]!", __func__, chan);

	return 0;
}

static inline void vbc_da_eq6_enable(int enable)
{
	/* id for DA_EQ6 is ignored */
	vbc_module_en(VBC_PLAYBACK01, DA_EQ6, enable, AUDIO_CHAN_ALL);
}

static inline void vbc_da_eq4_enable(int enable)
{
	/* id for DA_EQ4 is ignored */
	vbc_module_en(VBC_PLAYBACK01, DA_EQ4, enable, AUDIO_CHAN_ALL);
}

static inline void vbc_da_alc_enable(int enable)
{
	/* id for DA_ALC is ignored */
	vbc_module_en(VBC_PLAYBACK01, DA_ALC, enable, AUDIO_CHAN_ALL);
}

static inline void vbc_ad01_eq6_enable(int enable)
{
	vbc_module_en(VBC_CAPTURE01, AD_EQ6, enable, AUDIO_CHAN_ALL);
}

static inline void vbc_ad23_eq6_enable(int enable)
{
	vbc_module_en(VBC_CAPTURE23, AD_EQ6, enable, AUDIO_CHAN_ALL);
}

typedef int (*vbc_chan_enable_raw) (int enable, int chan);
static vbc_chan_enable_raw vbc_chan_enable_fun[VBC_IDX_MAX] = {
	[VBC_PLAYBACK01] = vbc_da01_enable_raw,
	[VBC_PLAYBACK23] = vbc_da23_enable_raw,
	[VBC_CAPTURE01] = vbc_ad01_enable_raw,
	[VBC_CAPTURE23] = vbc_ad23_enable_raw,
};

/* vbc_chan_enable:
 * enable da01 da23 ad01 ad23 clock.
 */
static int vbc_chan_enable(int enable, int vbc_idx, int chan)
{
	atomic_t *chan_on = &vbc_refcnt.chan_on[vbc_idx];

	if (enable) {
		atomic_inc(chan_on);
		if (atomic_read(chan_on) == 1) {
			vbc_chan_enable_fun[vbc_idx] (1, chan);
			sp_asoc_pr_dbg("VBC %s %d On\n", vbc_get_name(vbc_idx),
				       chan);
		}
	} else {
		if (atomic_dec_and_test(chan_on)) {
			vbc_chan_enable_fun[vbc_idx] (0, chan);
			sp_asoc_pr_dbg("VBC %s%d Off\n", vbc_get_name(vbc_idx),
				       chan);
		}
		if (atomic_read(chan_on) < 0)
			atomic_set(chan_on, 0);
	}
	sp_asoc_pr_dbg("%s%d REF: %d", vbc_get_name(vbc_idx), chan,
		       atomic_read(chan_on));
	return 0;
}

static DEFINE_SPINLOCK(vbc_pre_data_lock);
static void pre_fill_data(int enable)
{
	atomic_t *prefill_data_ref = &vbc_refcnt.prefill_data_ref;
	/* todo not consider sidetone case */
	spin_lock(&vbc_pre_data_lock);
	if (enable) {
		if (atomic_read(prefill_data_ref) == 0) {
			vbc_reg_write(REG_VBC_VBC_DAC0_FIFO_ADDR, 0x0);
			vbc_reg_write(REG_VBC_VBC_DAC1_FIFO_ADDR, 0x0);
		}
		atomic_inc(prefill_data_ref);
	} else {
		atomic_dec(prefill_data_ref);
		if (atomic_read(prefill_data_ref) < 0) {
			pr_warn("%s prefill_data_ref[%d] < 0\n",
				__func__, atomic_read(prefill_data_ref));
			atomic_set(prefill_data_ref, 0);
		}
	}
	sp_asoc_pr_dbg("VBC prefill_data_ref: enable[%s] %d",
		       enable ? "true" : "false",
		       atomic_read(prefill_data_ref));
	spin_unlock(&vbc_pre_data_lock);
}

/* this is adc src. note dac src not use */
static int vbc_ad_src_set(int rate, int ad_src_idx)
{
	unsigned int f1f2f3_bp = 0;
	unsigned int f1_sel = 0;
	unsigned int en_sel = 0;
	unsigned int val = 0;
	unsigned int mask = 0;
	struct sprd_vbc_src_reg_info *reg_info;

	if (ad_src_idx < 0) {
		pr_warn("invalid ad_src_idx[%d]\n", ad_src_idx);
		return -EINVAL;
	}

	reg_info = &vbc_ad_src[ad_src_idx].reg_info;

	if (!vbc_ad_src[ad_src_idx].reg_info.reg)
		return -EINVAL;

	sp_asoc_pr_dbg("Rate:%d, Chan: %s", rate,
		       vbc_get_ad_src_name(ad_src_idx));

	/* src_clr is WC(write then auto clear) and need not delay */
	vbc_reg_update(reg_info->reg, reg_info->clr_bit, reg_info->clr_bit);

	switch (rate) {
	case 32000:
		f1f2f3_bp = 0;
		f1_sel = 1;
		en_sel = 1;
		break;
	case 48000:
		f1f2f3_bp = 0;
		f1_sel = 0;
		en_sel = 1;
		break;
	case 44100:
		f1f2f3_bp = 1;
		f1_sel = 0;
		en_sel = 1;
		break;
	default:
		f1f2f3_bp = 0;
		f1_sel = 0;
		en_sel = 0;
		break;
	}

	/*src_set */
	mask = reg_info->f1f2f3_bp_bit |
	    reg_info->f1_sel_bit | reg_info->en_bit;

	(f1f2f3_bp ? (val |= reg_info->f1f2f3_bp_bit)
	 : (val &= ~reg_info->f1f2f3_bp_bit));
	(f1_sel ? (val |= reg_info->f1_sel_bit)
	 : (val &= ~reg_info->f1_sel_bit));
	(en_sel ? (val |= reg_info->en_bit) : (val &= ~reg_info->en_bit));

	vbc_reg_update(reg_info->reg, val, mask);

	return 0;
}

/* delay max 10ms */
#define MAX_CNT (1000)
static int vbc_da01_fifo_enable_raw(int enable, int chan)
{
	u32 cnt_1 = 0;
	u32 cnt_2 = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	if (enable == false) {
		while (cnt_1 < MAX_CNT) {
			if (vbc_reg_read(REG_VBC_VBC_DAC0_FIFO_STS) &
				BIT_DAC0_FIFO_EMPTY)
				break;

			cnt_1++;
			udelay(10);
		}
		if (chan == AUDIO_CHAN_ALL) {
			while (cnt_2 < MAX_CNT) {
				if (vbc_reg_read(REG_VBC_VBC_DAC1_FIFO_STS) &
					BIT_DAC1_FIFO_EMPTY)
					break;

				cnt_2++;
				udelay(10);
			}
		}
		pr_info("%s wait fifo empty %d channel, cnt_1=%d, cnt_2=%d\n",
			__func__, (chan == AUDIO_CHAN_ALL) ? 2 : 1,
			cnt_1, cnt_2);
	}
	vbc_module_en(VBC_PLAYBACK01, DA_FIFO, enable, chan);
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

static int vbc_da23_fifo_enable_raw(int enable, int chan)
{
	u32 cnt_1 = 0;
	u32 cnt_2 = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	if (enable == false) {
		while (cnt_1 < MAX_CNT) {
			if (vbc_reg_read(REG_VBC_VBC_DAC2_FIFO_STS) &
				BIT_DAC2_FIFO_EMPTY)
				break;

			cnt_1++;
			udelay(10);
		}
		if (chan == AUDIO_CHAN_ALL) {
			while (cnt_2 < MAX_CNT) {
				if (vbc_reg_read(REG_VBC_VBC_DAC3_FIFO_STS) &
					BIT_DAC3_FIFO_EMPTY)
					break;
				cnt_2++;
				udelay(10);
			}
		}
		pr_info("%s wait fifo empty %d channel, cnt_1=%d, cnt_2=%d\n",
			__func__, (chan == AUDIO_CHAN_ALL) ? 2 : 1,
			cnt_1, cnt_2);
	}
	vbc_module_en(VBC_PLAYBACK23, DA_FIFO, enable, chan);
	if (enable == false) {
		vbc_da23_set_dgmixer_dg(chan, 0);
		vbc_da23_set_dgmixer_dg(chan, 0);
	}
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

static int vbc_ad01_fifo_enable_raw(int enable, int chan)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	vbc_module_en(VBC_CAPTURE01, AD_FIFO, enable, chan);
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

static int vbc_ad23_fifo_enable_raw(int enable, int chan)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	vbc_module_en(VBC_CAPTURE23, AD_FIFO, enable, chan);
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

typedef int (*vbc_fifo_enable_raw) (int enable, int chan);
static vbc_chan_enable_raw vbc_fifo_enable_fun[VBC_IDX_MAX] = {
	[VBC_PLAYBACK01] = vbc_da01_fifo_enable_raw,
	[VBC_PLAYBACK23] = vbc_da23_fifo_enable_raw,
	[VBC_CAPTURE01] = vbc_ad01_fifo_enable_raw,
	[VBC_CAPTURE23] = vbc_ad23_fifo_enable_raw,
};

static int vbc_fifo_enable(int enable, int vbc_idx, int chan)
{
	atomic_t *fifo_on = &vbc_refcnt.fifo_on[vbc_idx];

	if (enable) {
		atomic_inc(fifo_on);
		if (atomic_read(fifo_on) == 1) {
			vbc_fifo_enable_fun[vbc_idx] (true, chan);
			/* alc moudle clear would work fifo enable,
			 * and we clear it only when dac01 fifo enable.
			 * for example:
			 * 1. mp3 playing, we can play touch tones or fm any
			 * times and alc clear only one time because da01
			 * fifo only enable once.
			 * 2. while playing normal(da01) or fm, we can play
			 * mp3 any times, and alc clear only one time too
			 * because da01 fifo only enable once.
			 */
			if (vbc_idx == VBC_PLAYBACK01) {
				vbc_reg_update(REG_VBC_VBC_MODULE_CLR,
					BIT_RF_DAC_ALC_CLR, BIT_RF_DAC_ALC_CLR);
				pr_debug("clear alc module %s\n", __func__);
			}
			sp_asoc_pr_dbg("VBC fifo %s %d On\n",
				vbc_get_name(vbc_idx),
				chan);
		}
	} else {
		if (atomic_dec_and_test(fifo_on)) {
			vbc_fifo_enable_fun[vbc_idx] (false, chan);
			sp_asoc_pr_dbg("VBC fifo %s%d Off\n",
				vbc_get_name(vbc_idx),
				chan);
		}
		if (atomic_read(fifo_on) < 0)
			atomic_set(fifo_on, 0);
	}
	pr_info("%s fifo %d REF: %d", vbc_get_name(vbc_idx), chan,
		       atomic_read(fifo_on));
	return 0;
}

static int vbc_da01_fifo_enable(int chan)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	vbc_fifo_enable(true, VBC_PLAYBACK01, chan);
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

static int vbc_da01_fifo_disable(int chan)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	vbc_fifo_enable(false, VBC_PLAYBACK01, chan);
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

static int vbc_da23_fifo_enable(int chan)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	vbc_fifo_enable(true, VBC_PLAYBACK23, chan);
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

static int vbc_da23_fifo_disable(int chan)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	vbc_fifo_enable(false, VBC_PLAYBACK23, chan);
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

static int vbc_ad01_fifo_enable(int chan)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	vbc_fifo_enable(true, VBC_CAPTURE01, chan);
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

static int vbc_ad01_fifo_disable(int chan)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	vbc_fifo_enable(false, VBC_CAPTURE01, chan);
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;

}

static int vbc_ad23_fifo_enable(int chan)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	vbc_fifo_enable(true, VBC_CAPTURE23, chan);
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

static int vbc_ad23_fifo_disable(int chan)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return -1;
	}
	vbc_fifo_enable(false, VBC_CAPTURE23, chan);
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

static int vbc_da_src_set(int rate)
{
	unsigned int f0_bp = 0;
	unsigned int f0_sel = 0;
	unsigned int f1f2f3_bp = 0;
	unsigned int f1_sel = 0;
	unsigned int en_sel = 0;
	unsigned int val = 0;
	unsigned int mask = 0;
	struct sprd_vbc_dac_src_reg_info *reg_info = &vbc_da_src.reg_info;

	if (!vbc_da_src.reg_info.reg)
		return -EINVAL;

	sp_asoc_pr_dbg("Rate:%d, Chan: %s\n", rate, "da src");

	/* src_clr is WC(write then auto clear) and need not delay */
	vbc_reg_update(reg_info->reg, reg_info->clr_bit, reg_info->clr_bit);

	switch (rate) {
	case 8000:
		break;
	case 11025:
		break;
	case 12000:
		break;
	case 16000:
		break;
	case 22005:
		break;
	case 24000:
		break;
	case 32000:
		f0_sel = 0;
		f0_bp = 1;
		f1f2f3_bp = 0;
		f1_sel = 1;
		en_sel = 1;
		break;
	case 44100:
		break;
	case 48000:
		break;
	default:
		break;
	}

	/*src_set */
	mask = reg_info->f0_sel_bit |
	    reg_info->f0_bp_bit |
	    reg_info->f1f2f3_bp_bit | reg_info->f1_sel_bit | reg_info->en_bit;

	(f0_sel ? (val |= reg_info->f0_sel_bit)
	 : (val &= (~reg_info->f0_sel_bit)));
	(f0_bp ? (val |= reg_info->f0_bp_bit) : (val &= ~reg_info->f0_bp_bit));
	(f1f2f3_bp ? (val |= reg_info->f1f2f3_bp_bit)
	 : (val &= ~reg_info->f1f2f3_bp_bit));
	(f1_sel ? (val |= reg_info->f1_sel_bit)
	 : (val &= ~reg_info->f1_sel_bit));
	(en_sel ? (val |= reg_info->en_bit) : (val &= ~reg_info->en_bit));
	vbc_reg_update(reg_info->reg, val, mask);

	return 0;
}
