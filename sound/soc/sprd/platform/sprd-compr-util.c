/*
 * sound/soc/sprd/platform/sprd-compr-util.c
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
#define pr_fmt(fmt) pr_sprd_fmt("COMPR")""fmt

#include "audio-sipc.h"

#define COMPR_WAIT_FOREVER AUDIO_SIPC_WAIT_FOREVER

static int compr_ch_open(uint8_t dst)
{
	int ret = 0;

	if (dst >= AUD_IPC_NR) {
		pr_info("%s, invalid dst:%d\n", __func__, dst);
		return -EINVAL;
	}
	ret = aud_ipc_ch_open(AMSG_CH_MP3_OFFLOAD);
	if (ret != 0) {
		pr_err("%s, Failed to open offload channel\n", __func__);
		return -EIO;
	}

	ret = aud_ipc_ch_open(AMSG_CH_MP3_OFFLOAD_DRAIN);
	if (ret != 0) {
		aud_ipc_ch_close(AMSG_CH_MP3_OFFLOAD);
		pr_err("%s, Failed to open offload drain channel\n", __func__);
		return -EIO;
	}

	return 0;
}

static int compr_ch_close(uint8_t dst)
{
	int ret = 0;

	if (dst >= AUD_IPC_NR) {
		pr_info("%s, invalid dst:%d\n", __func__, dst);
		return -EINVAL;
	}
	ret = aud_ipc_ch_close(AMSG_CH_MP3_OFFLOAD);
	if (ret != 0) {
		pr_err("%s, Failed to close offload channel\n", __func__);
		return -EIO;
	}

	ret = aud_ipc_ch_close(AMSG_CH_MP3_OFFLOAD_DRAIN);
	if (ret != 0) {
		pr_err("%s, Failed to close offload drain channel\n", __func__);
		return -EIO;
	}

	return 0;
}

static int compr_send_cmd(u32 cmd, void *para, int32_t para_size)
{
	int ret = 0;

	ret = aud_send_cmd(AMSG_CH_MP3_OFFLOAD, -1, -1, cmd,
		para, para_size, COMPR_WAIT_FOREVER);

	if (ret < 0) {
		pr_err("%s, Failed to send cmd(0x%x), ret(%d)\n",
			__func__, cmd, ret);
		return -EIO;
	}

	return 0;
}

static int compr_send_cmd_no_wait(uint16_t channel, u32 cmd,
	u32 total_size, u32 reserve)
{
	struct aud_smsg msend = {0};

	aud_smsg_set(&msend, channel, cmd, total_size, reserve, 0, 0);

	return aud_smsg_send(AUD_IPC_AGDSP, &msend);
}

static int compr_receive_cmd(uint16_t channel, u32 cmd, int *value)
{
	struct aud_smsg mrecv = { 0 };
	int ret = 0;

	aud_smsg_set(&mrecv, channel, cmd, 0, 0, 0, 0);
	ret = aud_smsg_recv(AUD_IPC_AGDSP, &mrecv, COMPR_WAIT_FOREVER);
	if (ret >= 0)
		*value = mrecv.parameter3;

	return ret;
}
