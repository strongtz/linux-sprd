/*
 * include/sound/audio-sipc.h
 *
 * SPRD SoC VBC -- SpreadTrum SOC SIPC for audio Common function.
 *
 * Copyright (C) 2015 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY ork FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __AUDIO_SIPC_H
#define __AUDIO_SIPC_H

#include "audio-smsg.h"

#define AUDIO_SIPC_WAIT_FOREVER	(-1)

enum SND_VBC_PROFILE_USE_E {
	SND_VBC_PROFILE_START = 0,
	SND_VBC_PROFILE_AUDIO_STRUCTURE = SND_VBC_PROFILE_START,
	/*vbc eq*/
	SND_VBC_PROFILE_DSP,
	SND_VBC_PROFILE_NXP,
	SND_VBC_PROFILE_IVS_SMARTPA,
	SND_VBC_PROFILE_MAX
};

enum SND_VBC_SHM_USE_E {
	SND_VBC_SHM_START = SND_VBC_PROFILE_MAX,
	SND_VBC_SHM_VBC_REG = SND_VBC_SHM_START,
	SND_VBC_SHM_MAX
};

int aud_ipc_ch_open(uint16_t channel);
int aud_ipc_ch_close(uint16_t channel);
int aud_send_cmd(uint16_t channel, int id, int stream, u32 cmd,
		 void *para, size_t n, int32_t timeout);
int aud_send_cmd_result(u16 channel, int id, int stream, u32 cmd,
			void *para, size_t n, void *result, int32_t timeout);
int aud_send_block_param(uint16_t channel, int id, int stream, u32 cmd,
	int type, void *buf, size_t n, int32_t timeout);

int aud_recv_block_param(uint16_t channel, int id, int stream, u32 cmd,
	int type, void *buf, u32 size, int32_t timeout);

int aud_recv_cmd(u16 channel, u32 cmd,
		 struct aud_smsg *result, int32_t timeout);
int aud_send_use_noreplychan(u32 cmd, u32 value0, u32 value1,
			     u32 value2, int32_t value3);
int aud_send_cmd_no_wait(uint16_t channel, u32 cmd, u32 value0,
			 u32 value1, u32 value2, int32_t value3);
int aud_send_cmd_no_param(uint16_t channel, u32 cmd,
			  u32 value0, u32 value1,
			  u32 value2, int32_t value3, int32_t timeout);
u32 aud_ipc_dump(void *buf, u32 buf_bytes);
int aud_get_aud_ipc_smsg_addr(unsigned long *phy, unsigned long *virt,
			      u32 *size);
int aud_get_aud_ipc_smsg_para_addr(unsigned long *phy,
				   unsigned long *virt, u32 *size);

#endif /* __AUDIO_SIPC_H */
