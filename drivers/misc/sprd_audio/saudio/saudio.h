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
#ifndef __SAUDIO_H
#define  __SAUDIO_H

struct saudio_init_data {
	char	*name;
	uint8_t	dst;
	uint8_t	ctrl_channel;
	uint8_t	playback_channel[2];
	uint8_t	capture_channel;
	uint8_t	monitor_channel;
	uint8_t	device_num;
	uint32_t	ap_addr_to_cp;
};

#endif
