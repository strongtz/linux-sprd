/*
 * sound/soc/sprd/sprd-i2s.h
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
#ifndef __SPRD_I2S_H
#define __SPRD_I2S_H

#define I2S_MAGIC_ID	(0x124)
#define I2S_FIFO_DEPTH 32

#define FUN_REG(f) ((unsigned short)(-((f) + 1)))

enum {
	FS = 0,
	HW_PROT,
	SLAVE_TIMEOUT,
	BUS_TYPE,
	BYTE_PER_CHAN,
	MODE,
	LSB,
	TRX_MODE,
	LRCK_INV,
	SYNC_MODE,
	CLK_INV,
	I2S_BUS_MODE,
	PCM_BUS_MODE,
	PCM_SLOT,
	PCM_CYCLE,
	TX_WATERMARK,
	RX_WATERMARK,
	I2S_CONFIG_MAX
};

struct i2s_config {
	u32 hw_port;
	u32 fs;
	u32 slave_timeout;
	u32 bus_type:1;
	u32 byte_per_chan:2;
	u32 mode:1;
	u32 lsb:1;
	u32 rtx_mode:2;
	u32 sync_mode:1;
	u32 lrck_inv:1;
	u32 clk_inv:2;
	u32 i2s_bus_mode:1;
	u32 pcm_bus_mode:1;
	u32 pcm_slot:3;
	u16 pcm_cycle;
	u16 tx_watermark;
	u16 rx_watermark;
};

#ifdef CONFIG_SND_SOC_SPRD_I2S
void i2s_debug_write(struct snd_info_entry *entry,
		     struct snd_info_buffer *buffer);
void i2s_debug_read(struct snd_info_entry *entry,
		    struct snd_info_buffer *buffer);
void i2s_register_proc_read(struct snd_info_entry *entry,
			    struct snd_info_buffer *buffer);
int i2s_config_get(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol);
int i2s_config_set(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol);
struct i2s_config *sprd_i2s_dai_to_config(struct snd_soc_dai *dai);

#else
static inline void i2s_debug_write(struct snd_info_entry *entry,
				   struct snd_info_buffer *buffer)
{
	pr_debug("%s is empty.\n", __func__);
}

static inline void i2s_debug_read(struct snd_info_entry *entry,
				  struct snd_info_buffer *buffer)
{
	pr_debug("%s is empty.\n", __func__);
}

static inline void i2s_register_proc_read(struct snd_info_entry *entry,
					  struct snd_info_buffer *buffer)
{
	pr_debug("%s is empty.\n", __func__);
}

static inline int i2s_config_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s is empty.\n", __func__);
	return 0;
}

static inline int i2s_config_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s is empty.\n", __func__);
	return 0;
}

static inline struct i2s_config *sprd_i2s_dai_to_config(struct snd_soc_dai *dai)
{
	pr_debug("%s is empty.\n", __func__);
	return 0;
}
#endif /* CONFIG_SND_SOC_SPRD_I2S */

#endif /* __SPRD_I2S_H */
