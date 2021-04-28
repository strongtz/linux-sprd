/*:
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#ifndef _LT9611_I2C_H_
#define _LT9611_I2C_H_

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/stat.h>

#define TAG "[LT9611]"

#define LT_ERR(fmt, ...) \
		pr_err(TAG " %s() " fmt "\n", __func__, ##__VA_ARGS__)
#define LT_WARN(fmt, ...) \
		pr_warn(TAG " %s() " fmt "\n", __func__, ##__VA_ARGS__)
#define LT_INFO(fmt, ...) \
		pr_info(TAG " %s() " fmt "\n", __func__, ##__VA_ARGS__)

#define SINGLE_PORT_MIPI  1
#define DUAL_PORT_MIPI    2

//Do not support 3lane
#define LANE_CNT_1   1
#define LANE_CNT_2   2
#define LANE_CNT_4   0

#define AUDIO_I2S     0
#define AUDIO_SPDIF   1

#define DSI           0
#define CSI           1

#define NON_BURST_MODE_WITH_SYNC_EVENTS 0x00
#define NON_BURST_MODE_WITH_SYNC_PULSES 0x01
#define BURST_MODE                      0x02

#define AC_MODE     0
#define DC_MODE     1

#define HDCP_DISABLED 0
#define HDCP_ENABLE   1


struct lontium_ic_mode {
	u8 mipi_port_cnt; //1 or 2
	u8 mipi_lane_cnt; //1 or 2 or 4
	bool mipi_mode;   //dsi or csi
	u8 video_mode;    //non-burst mode with sync pulses; non-burst mode with sync events
	bool audio_out;   //i2s or spdif
	bool hdmi_coupling_mode;//ac_mode or dc_mode
	bool hdcp_encryption; //hdcp_enable or hdcp_disable
};

struct video_timing {
	u16 hfp;
	u16 hs;
	u16 hbp;
	u16 hact;
	u16 htotal;
	u16 vfp;
	u16 vs;
	u16 vbp;
	u16 vact;
	u16 vtotal;
	bool h_polarity;
	bool v_polarity;
	u16 vic;
	u8 aspact_ratio;  // 0=no data, 1=4:3, 2=16:9, 3=no data.
	u32 pclk_khz;
};

enum videoformat {
	VIDEO_640x480_60HZ_VIC1,       //vic 1
	VIDEO_720x480_60HZ_VIC3,       //vic 2
	VIDEO_1280x720_60HZ_VIC4,      //vic 3
	VIDEO_1920x1080_60HZ_VIC16,    //vic 4

	VIDEO_1920x1080I_60HZ_169 = 5,  //vic 5
	VIDEO_720x480I_60HZ_43 = 6,     //vic 6
	VIDEO_720x480I_60HZ_169 = 7,    //vic 7
	VIDEO_720x240P_60HZ_43 = 8,     //vic 8
	VIDEO_720x240P_60HZ_169 = 9,    //vic 9

	VIDEO_1280x720_50HZ_VIC,
	VIDEO_1280x720_30HZ_VIC,

	VIDEO_3840x2160_30HZ_VIC,
	VIDEO_3840x2160_25HZ_VIC,
	VIDEO_3840x2160_24HZ_VIC,

	VIDEO_3840x1080_60HZ_VIC,
	VIDEO_1024x600_60HZ_VIC,
	VIDEO_1080x1920_60HZ_VIC,
	VIDEO_720x1280_60HZ_VIC,
	VIDEO_1280x800_60HZ_VIC,
	VIDEO_540x960_60HZ_VIC,
	VIDEO_1366x768_60HZ_VIC,

	VIDEO_2560x1600_60HZ_VIC,
	VIDEO_2560x1440_60HZ_VIC,
	VIDEO_2560x1080_60HZ_VIC,

	VIDEO_1920x1080_50HZ_VIC,
	VIDEO_1920x1080_30HZ_VIC,
	VIDEO_1920x1080_25HZ_VIC,
	VIDEO_1920x1080_24HZ_VIC,

	VIDEO_OTHER,
	VIDEO_NONE
};

struct lt9611_i2c {
	struct i2c_client *client;
	struct platform_device *mp_dev;
	u32 chipid;
};


#endif
