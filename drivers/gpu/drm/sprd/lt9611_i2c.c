/*
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

#include "lt9611_i2c.h"

struct video_timing *video;

u8 pcr_m;

bool tx_hpd;

enum videoformat video_format;

static int lt9611_mipi_port = 1; //0 daul port, 1 single_port_mipi
static int lt_slave_addr = (0x76 >> 1);

static struct lt9611_i2c lt9611_i2c;

struct lontium_ic_mode lt9611 = {
	SINGLE_PORT_MIPI, //mipi_port_cnt; //single_port_mipi or dual_port_mipi
	//DUAL_PORT_MIPI, //mipi_port_cnt; //single_port_mipi or dual_port_mipi
	LANE_CNT_4,       //mipi_lane_cnt; //1 or 2 or 4
	DSI,              //mipi_mode;     //dsi or csi
	NON_BURST_MODE_WITH_SYNC_EVENTS,
	AUDIO_I2S,       //audio_out      //audio_i2s or audio_spdif
	//DC_MODE,       //hdmi_coupling_mode;//ac_mode or dc_mode
	AC_MODE,         //hdmi_coupling_mode;//ac_mode or dc_mode
	HDCP_ENABLE      //hdcp_encryption //hdcp_enable or hdcp_diabled
};
// hfp, hs, hbp,hact,htotal,vfp, vs, vbp,vact,vtotal, pclk_khz
struct video_timing video_1920x1080_60Hz  = {
	32, 4, 32, 1920,  1988,  176,  10,
	16, 1080, 1282, 1, 1, 16, 2, 153600};
struct video_timing video_1920x1080_50Hz  = {
	528, 44, 148, 1920,  2640,  4,  5,
	36, 1080, 1125, 1, 1, 31, 2, 148500};
struct video_timing video_1920x1080_30Hz  = {
	88, 44, 148, 1920,  2200,  4,  5,
	36, 1080, 1125, 1, 1, 34, 2, 128000};
struct video_timing video_1920x1080_25Hz  = {
	528, 44, 148, 1920,  2640,  4,  5,
	36, 1080, 1125, 1, 1, 33, 2, 74250};
struct video_timing video_1920x1080_24Hz  = {
	638, 44, 148, 1920,  2750,  4,  5,
	36, 1080, 1125, 1, 1, 32, 2, 74250};

static int lt9611_i2c_read(u8 reg, u8 *data, u16 length)
{
	int err;
	struct i2c_client *i2c = lt9611_i2c.client;
	struct i2c_msg msgs[2];

	msgs[0].flags = 0;
	msgs[0].buf = &reg;
	msgs[0].addr = i2c->addr;
	msgs[0].len = 1;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = data;
	msgs[1].addr = i2c->addr;
	msgs[1].len = length;

	err = i2c_transfer(i2c->adapter, msgs, 2);
	if (err < 0) {
		LT_ERR("lt9611 i2c read error:%d\n", err);
		LT_ERR("msg:%d-%d:%d, %d-%d:%d\n",
			msgs[0].addr, msgs[0].len, msgs[0].buf[0],
			msgs[1].addr, msgs[1].len, msgs[1].buf[1]);
		return err;
	}

	return length;
}

static int lt9611_i2c_write(u8 reg, u8 *data, u16 length)
{
	int err;
	u8 buf[length + 1];
	struct i2c_client *i2c = lt9611_i2c.client;
	struct i2c_msg msg;

	buf[0] = reg;
	memcpy(&buf[1], data, length * sizeof(u8));
	msg.flags = 0;
	msg.buf = buf;
	msg.addr = i2c->addr;
	msg.len = length + 1;

	err = i2c_transfer(i2c->adapter, &msg, 1);
	if (err < 0) {
		LT_ERR("lt9611 i2c write error:%d\n", err);
		LT_ERR("msg:%d-%d:%d\n", msg.addr, msg.len, msg.buf[0]);
		return err;
	}

	return length;
}

static void hdmi_writei2c_byte(u8 reg, u8 value)
{
	lt9611_i2c_write(reg, &value, 1);
}

static u8 hdmi_readi2c_byte(u8 reg)
{
	u8 value = 0;

	lt9611_i2c_read(reg, &value, 1);
	return value;
}

void lt9611_chip_id(void)
{
	u8 chipid[4] = {0};

	hdmi_writei2c_byte(0xFF, 0x80);
	hdmi_writei2c_byte(0xee, 0x01);
	chipid[0] = hdmi_readi2c_byte(0x00);
	chipid[1] = hdmi_readi2c_byte(0x01);
	chipid[2] = hdmi_readi2c_byte(0x02);
	lt9611_i2c.chipid = chipid[0] << 16 | chipid[1] << 8 | chipid[2];
	LT_INFO("lt9611 ring Chip ID = %x\n", lt9611_i2c.chipid);

	hdmi_writei2c_byte(0xFF, 0x81);
	hdmi_writei2c_byte(0x01, 0x18); //sel xtal clock
	hdmi_writei2c_byte(0xFF, 0x80);
}

void lt9611_rst_pd_init(void)
{
	/* power consumption for standby */
	hdmi_writei2c_byte(0xFF, 0x81);
	hdmi_writei2c_byte(0x02, 0x48);
	hdmi_writei2c_byte(0x23, 0x80);
	hdmi_writei2c_byte(0x30, 0x00);
	hdmi_writei2c_byte(0x01, 0x00); /* i2s stop work */
}

void lt9611_system_init(void)
{
	//hdmi_writei2c_byte(0xFF, 0x81);
	//hdmi_writei2c_byte(0x01, 0x18); //sel xtal clock
	//GPIO init
	//hdmi_writei2c_byte(0x46, 0x8b); //select IRQ from multifunction

	hdmi_writei2c_byte(0xFF, 0x82);
	hdmi_writei2c_byte(0x51, 0x11);
	//Timer for Frequency meter
	hdmi_writei2c_byte(0xFF, 0x82);
	hdmi_writei2c_byte(0x1b, 0x69); //Timer 2
	hdmi_writei2c_byte(0x1c, 0x78);
	hdmi_writei2c_byte(0xcb, 0x69); //Timer 1
	hdmi_writei2c_byte(0xcc, 0x78);

	/*power consumption for work*/
	hdmi_writei2c_byte(0xff, 0x80);
	hdmi_writei2c_byte(0x04, 0xf0);
	hdmi_writei2c_byte(0x06, 0xf0);
	hdmi_writei2c_byte(0x0a, 0x80);
	hdmi_writei2c_byte(0x0b, 0x46); //csc clk
	hdmi_writei2c_byte(0x0d, 0xef);
	hdmi_writei2c_byte(0x11, 0xfa);

}

void lt9611_mipi_input_analog(void)
{
	//mipi mode
	hdmi_writei2c_byte(0xff, 0x81);
	hdmi_writei2c_byte(0x06, 0x20); //port A rx current
	hdmi_writei2c_byte(0x07, 0x3f); //eq
	hdmi_writei2c_byte(0x08, 0x3f); //eq
	hdmi_writei2c_byte(0x0a, 0xfe); //port A ldo voltage set
	hdmi_writei2c_byte(0x0b, 0xbf); //enable port A lprx
	hdmi_writei2c_byte(0x11, 0x20); //port B rx current
	hdmi_writei2c_byte(0x12, 0x3f); //eq
	hdmi_writei2c_byte(0x13, 0x3f); //eq
	hdmi_writei2c_byte(0x15, 0xfe); //port B ldo voltage set
	hdmi_writei2c_byte(0x16, 0xbf); //enable port B lprx

	hdmi_writei2c_byte(0x1c, 0x03); //PortA clk lane no-LP mode.
	hdmi_writei2c_byte(0x20, 0x03); //PortB clk lane no-LP mode.
}

void lt9611_mipi_input_digtal(void)
{
	u8 lanes;

	lanes = lt9611.mipi_lane_cnt;
	LT_INFO("lt9611 set mipi lanes = %d\n", lanes);

	hdmi_writei2c_byte(0xff, 0x82);
	hdmi_writei2c_byte(0x4f, 0x80);    //[7] = Select ad_txpll_d_clk.
	hdmi_writei2c_byte(0x50, 0x10);

	hdmi_writei2c_byte(0xff, 0x83);
	hdmi_writei2c_byte(0x00, lanes);
	hdmi_writei2c_byte(0x02, 0x0a); //settle
	hdmi_writei2c_byte(0x06, 0x0a); //settle

	if (lt9611_mipi_port) {  //single_port_mipi
		hdmi_writei2c_byte(0x0a, 0x00); //1=dual_lr, 0=dual_en
		LT_INFO("lt9611 set mipi ports = 1\n");
	} else {   //dual_port_mipi
		hdmi_writei2c_byte(0x0a, 0x03); //1=dual_lr, 0=dual_en
		LT_INFO("lt9611 set mipi port = 2\n");
	}

	if (lt9611.mipi_mode == CSI) {
		LT_INFO("lt9611.mipi_mode = csi\n");
		hdmi_writei2c_byte(0xff, 0x83);
		hdmi_writei2c_byte(0x08, 0x10); //csi_en
		hdmi_writei2c_byte(0x2c, 0x40); //csi_sel
	} else
		LT_INFO("lt9611.mipi_mode = dsi\n");
}

void lt9611_video_check(void)
{
	u8 mipi_video_format = 0x00;
	u16 h_act, h_act_a, h_act_b, v_act, v_tal;
	u16 h_total_sysclk;

	hdmi_writei2c_byte(0xff, 0x82); // top video check module
	h_total_sysclk = hdmi_readi2c_byte(0x86);
	h_total_sysclk = (h_total_sysclk << 8) + hdmi_readi2c_byte(0x87);

	v_act = hdmi_readi2c_byte(0x82);
	v_act = (v_act << 8) + hdmi_readi2c_byte(0x83);
	v_tal = hdmi_readi2c_byte(0x6c);
	v_tal = (v_tal << 8) + hdmi_readi2c_byte(0x6d);

	hdmi_writei2c_byte(0xff, 0x83);
	h_act_a = hdmi_readi2c_byte(0x82);
	h_act_a = (h_act_a << 8) + hdmi_readi2c_byte(0x83);

	h_act_b = hdmi_readi2c_byte(0x86);
	h_act_b = (h_act_b << 8) + hdmi_readi2c_byte(0x87);

	if (lt9611.mipi_mode == CSI) {
		LT_INFO("lt9611.mipi_mode = csi\n");
		h_act_a /= 2;
		h_act_b /= 2;
	} else {
		LT_INFO("lt9611.mipi_mode = dsi\n");
		h_act_a /= 3;
		h_act_b /= 3;
	}

	mipi_video_format = hdmi_readi2c_byte(0x88);

	LT_INFO("h_act_a = %d, h_act_b = %d, v_act = %d, v_tal = %d:\n",
		h_act_a, h_act_b, v_act, v_tal);

	LT_INFO("mipi_video_format: %x\n", mipi_video_format);

	if (lt9611_mipi_port == 0) //dual port.
		h_act = h_act_a + h_act_b;
	else
		h_act = h_act_a;

	if ((h_act == video_1920x1080_60Hz.hact) &&
	    (v_act == video_1920x1080_60Hz.vact)) { //1080P
		if (h_total_sysclk < 430) {
			LT_INFO("Video_Check = video_1920x1080_60Hz\n");
			video_format = VIDEO_1920x1080_60HZ_VIC16;
			video = &video_1920x1080_60Hz;
		} else if (h_total_sysclk < 510) {
			LT_INFO("Video_Check = video_1920x1080_50Hz\n");
			video_format = VIDEO_1920x1080_50HZ_VIC;
			video = &video_1920x1080_50Hz;
		} else if (h_total_sysclk < 830) {
			LT_INFO("Video_Check = video_1920x1080_30Hz\n");
			video_format = VIDEO_1920x1080_30HZ_VIC;
			video = &video_1920x1080_30Hz;
		} else if (h_total_sysclk < 980) {
			LT_INFO("Video_Check = video_1920x1080_25Hz\n");
			video_format = VIDEO_1920x1080_25HZ_VIC;
			video = &video_1920x1080_25Hz;
		} else if (h_total_sysclk < 1030) {
			LT_INFO("Video_Check = video_1920x1080_24Hz\n");
			video_format = VIDEO_1920x1080_24HZ_VIC;
			video = &video_1920x1080_24Hz;
		}
	} else {
		video_format = VIDEO_NONE;
		LT_INFO("Video_Check = unknown video format\n");
	}
}

void show_timing(struct video_timing *video_timing)
{
	LT_INFO("video_format:%dX%d, pclk_khz:%d\n\t",
		video_timing->hact, video_timing->vact,
		video_timing->pclk_khz);
	LT_INFO("hfp:%d, hbp:%d, hs:%d, htotal:%d\n\t",
		video_timing->hfp, video_timing->hbp,
		video_timing->hs, video_timing->htotal);
	LT_INFO("vfp:%d, vbp:%d, vs:%d, htotel:%d\n\t",
		video_timing->vfp, video_timing->vbp,
		video_timing->vs, video_timing->vtotal);
}

void lt9611_mipi_video_timing(struct video_timing *video_timing)
{
	show_timing(video_timing);
	hdmi_writei2c_byte(0xff, 0x83);
	hdmi_writei2c_byte(0x0d, (u8)(video_timing->vtotal / 256));
	hdmi_writei2c_byte(0x0e, (u8)(video_timing->vtotal % 256));//vtotal
	hdmi_writei2c_byte(0x0f, (u8)(video_timing->vact / 256));
	hdmi_writei2c_byte(0x10, (u8)(video_timing->vact % 256));  //vactive
	hdmi_writei2c_byte(0x11, (u8)(video_timing->htotal / 256));
	hdmi_writei2c_byte(0x12, (u8)(video_timing->htotal % 256));//htotal
	hdmi_writei2c_byte(0x13, (u8)(video_timing->hact / 256));
	hdmi_writei2c_byte(0x14, (u8)(video_timing->hact % 256)); //hactive
	hdmi_writei2c_byte(0x15, (u8)(video_timing->vs % 256));   //vsa
	hdmi_writei2c_byte(0x16, (u8)(video_timing->hs % 256));   //hsa
	hdmi_writei2c_byte(0x17, (u8)(video_timing->vfp % 256));  //vfp
	hdmi_writei2c_byte(0x18, (u8)((video_timing->vs +
				video_timing->vbp) % 256));  //vss
	hdmi_writei2c_byte(0x19, (u8)(video_timing->hfp % 256));  //hfp
	hdmi_writei2c_byte(0x1a, (u8)(((video_timing->hfp / 256) << 4) +
			(video_timing->hs + video_timing->hbp) / 256));
	hdmi_writei2c_byte(0x1b, (u8)((video_timing->hs +
				video_timing->hbp) % 256));  //hss
}

void lt9611_mipi_pcr(struct video_timing *video_timing)
{
	u8 POL;

	POL = (video_timing->h_polarity) * 0x02 + (video_timing->v_polarity);
	POL = ~POL;
	POL &= 0x03;

	LT_INFO(" POL = %x, %x\n", POL, POL);

	hdmi_writei2c_byte(0xff, 0x83);
	hdmi_writei2c_byte(0x0b, 0x01); //vsync read delay(reference value)
	hdmi_writei2c_byte(0x0c, 0x10); //

	hdmi_writei2c_byte(0x48, 0x00); //de mode delay
	hdmi_writei2c_byte(0x49, 0x81); //=1/4 hact

	/* stage 1 */
	hdmi_writei2c_byte(0x21, 0x4a); //bit[3:0] step[11:8]
	//hdmi_writei2c_byte(0x22, 0x40);//step[7:0]

	hdmi_writei2c_byte(0x24, 0x71); //bit[7:4]v/h/de mode; line for clk stb[11:8]
	hdmi_writei2c_byte(0x25, 0x30); //line for clk stb[7:0]

	hdmi_writei2c_byte(0x2a, 0x01); //clk stable in

	/* stage 2 */
	hdmi_writei2c_byte(0x4a, 0x40); //offset //0x10
	hdmi_writei2c_byte(0x1d, (0x10 | POL)); //PCR de mode step setting.

	/* MK limit */
	switch (video_format) {
	case VIDEO_3840x1080_60HZ_VIC:
	case VIDEO_3840x2160_30HZ_VIC:
	case VIDEO_3840x2160_25HZ_VIC:
	case VIDEO_3840x2160_24HZ_VIC:
	case VIDEO_2560x1600_60HZ_VIC:
	case VIDEO_2560x1440_60HZ_VIC:
	case VIDEO_2560x1080_60HZ_VIC:
		if (lt9611_mipi_port == 0) { //dual port.
			hdmi_writei2c_byte(0x0b, 0x03); //vsync mode
			hdmi_writei2c_byte(0x0c, 0xd0); //=1/4 hact

			hdmi_writei2c_byte(0x48, 0x03); //de mode delay
			hdmi_writei2c_byte(0x49, 0xb0); //

			//step1
			hdmi_writei2c_byte(0x24, 0x72);
			hdmi_writei2c_byte(0x25, 0x00);
			hdmi_writei2c_byte(0x2a, 0x01);  //clk stable in

			//step2
			hdmi_writei2c_byte(0x4a, 0x10);
			hdmi_writei2c_byte(0x1d, 0x10);
			//hdmi_writei2c_byte(0x23, 0x30);
		}
		break;
	case VIDEO_1920x1080_60HZ_VIC16:
	case VIDEO_1920x1080_30HZ_VIC:
	case VIDEO_1280x720_60HZ_VIC4:
	case VIDEO_1280x720_30HZ_VIC:
	case VIDEO_720x480_60HZ_VIC3:
	case VIDEO_640x480_60HZ_VIC1:
		break;
	case VIDEO_540x960_60HZ_VIC:
	case VIDEO_1024x600_60HZ_VIC:
		hdmi_writei2c_byte(0x24, 0x70); //bit[7:4]v/h/de mode; line for clk stb[11:8]
		hdmi_writei2c_byte(0x25, 0x80); //line for clk stb[7:0]
		hdmi_writei2c_byte(0x2a, 0x10); //clk stable in
		/* stage 2 */
		//hdmi_writei2c_byte(0x23, 0x04); //pcr h mode step
		//hdmi_writei2c_byte(0x4a, 0x10); //offset //0x10
		hdmi_writei2c_byte(0x1d, 0xf0); //PCR de mode step setting.
		break;

	default:
		break;
	}
	lt9611_mipi_video_timing(video);

	hdmi_writei2c_byte(0xff, 0x83);
	hdmi_writei2c_byte(0x26, pcr_m);
	hdmi_writei2c_byte(0x27, 0x10);

	hdmi_writei2c_byte(0xff, 0x80);
	hdmi_writei2c_byte(0x11, 0x5a); //Pcr reset
	hdmi_writei2c_byte(0x11, 0xfa);
}

int lt9611_pll(struct video_timing *video_timing)
{
	u32 pclk;
	u8 pll_lock_flag;
	u8 hdmi_post_div;
	u8 i;

	if (video_timing == NULL) {
		LT_ERR("video_timing is null\n");
		return -EINVAL;
	}

	pclk = video_timing->pclk_khz;
	LT_INFO("set rx pll = %d\n", pclk); //Dec

	hdmi_writei2c_byte(0xff, 0x81);
	hdmi_writei2c_byte(0x23, 0x40);
	hdmi_writei2c_byte(0x24, 0x62); //0x62, LG25UM58 issue, 20180824
	hdmi_writei2c_byte(0x25, 0x80); //pre-divider
	hdmi_writei2c_byte(0x26, 0x55);
	hdmi_writei2c_byte(0x2c, 0x37);
	//hdmi_writei2c_byte(0x2d, 0x99); //txpll_divx_set&da_txpll_freq_set
	//hdmi_writei2c_byte(0x2e, 0x01);
	hdmi_writei2c_byte(0x2f, 0x01);
	hdmi_writei2c_byte(0x26, 0x55);
	hdmi_writei2c_byte(0x27, 0x66);
	hdmi_writei2c_byte(0x28, 0x88);

	if (pclk > 150000) {
		hdmi_writei2c_byte(0x2d, 0x88);
		hdmi_post_div = 0x01;
	} else if (pclk > 70000) {
		hdmi_writei2c_byte(0x2d, 0x99);
		hdmi_post_div = 0x02;
	} else {
		hdmi_writei2c_byte(0x2d, 0xaa);
		hdmi_post_div = 0x04;
	}

	pcr_m = (u8)((pclk * 5 * hdmi_post_div) / 27000);
	LT_INFO("pcr_m = %x, hdmi_post_div = %d\n", pcr_m, hdmi_post_div);

	hdmi_writei2c_byte(0xff, 0x83);
	hdmi_writei2c_byte(0x2d, 0x40); //M up limit
	hdmi_writei2c_byte(0x31, 0x08); //M down limit
	hdmi_writei2c_byte(0x26, 0x80 | pcr_m); /* fixed M is to let pll locked*/

	pclk = pclk / 2;
	hdmi_writei2c_byte(0xff, 0x82);     //13.5M
	hdmi_writei2c_byte(0xe3, pclk / 65536);
	pclk = pclk % 65536;
	hdmi_writei2c_byte(0xe4, pclk / 256);
	hdmi_writei2c_byte(0xe5, pclk % 256);

	hdmi_writei2c_byte(0xde, 0x20);
	hdmi_writei2c_byte(0xde, 0xe0);

	hdmi_writei2c_byte(0xff, 0x80);
//	hdmi_writei2c_byte(0x11, 0x5a); /* Pcr clk reset */
//	hdmi_writei2c_byte(0x11, 0xfa);
//	hdmi_writei2c_byte(0x18, 0xdc); /* pll analog reset */
//	hdmi_writei2c_byte(0x18, 0xfc);
	hdmi_writei2c_byte(0x16, 0xf1);
	hdmi_writei2c_byte(0x16, 0xf3);

	/* pll lock status */
	for (i = 0; i < 6 ; i++) {
		hdmi_writei2c_byte(0xff, 0x80);
		hdmi_writei2c_byte(0x16, 0xe3); /* pll lock logic reset */
		hdmi_writei2c_byte(0x16, 0xf3);
		hdmi_writei2c_byte(0xff, 0x82);
		pll_lock_flag = hdmi_readi2c_byte(0x15);
		if (pll_lock_flag & 0x80) {
			LT_INFO("hdmi pll locked\n");
			break;
		} else {
			hdmi_writei2c_byte(0xff, 0x80);
			hdmi_writei2c_byte(0x11, 0x5a); /* Pcr clk reset */
			hdmi_writei2c_byte(0x11, 0xfa);
			hdmi_writei2c_byte(0x18, 0xdc); /* pll analog reset */
			hdmi_writei2c_byte(0x18, 0xfc);
			hdmi_writei2c_byte(0x16, 0xf1); /* pll cal reset*/
			hdmi_writei2c_byte(0x16, 0xf3);
			LT_INFO("hdmi pll unlocked, reset pll\n");
		}
	}

	return 0;
}

void lt9611_hdmi_tx_phy(void)
{
	hdmi_writei2c_byte(0xff, 0x81);
	hdmi_writei2c_byte(0x30, 0x6a);
	if (lt9611.hdmi_coupling_mode == AC_MODE) {
		hdmi_writei2c_byte(0x31, 0x73); //DC: 0x44, AC:0x73
	} else { //lt9611.hdmi_coupling_mode == dc_mode
		hdmi_writei2c_byte(0x31, 0x44);
	}
	hdmi_writei2c_byte(0x32, 0x4a);
	hdmi_writei2c_byte(0x33, 0x0b);
	hdmi_writei2c_byte(0x34, 0x00);
	hdmi_writei2c_byte(0x35, 0x00);
	hdmi_writei2c_byte(0x36, 0x00);
	hdmi_writei2c_byte(0x37, 0x44);
	hdmi_writei2c_byte(0x3f, 0x0f);
	hdmi_writei2c_byte(0x40, 0xa0);
	hdmi_writei2c_byte(0x41, 0xa0);
	hdmi_writei2c_byte(0x42, 0xa0);
	hdmi_writei2c_byte(0x43, 0xa0);
	hdmi_writei2c_byte(0x44, 0x0a);
}

void lt9611_hdmi_out_enable(void)
{
	hdmi_writei2c_byte(0xff, 0x81);
	hdmi_writei2c_byte(0x23, 0x40);

	hdmi_writei2c_byte(0xff, 0x82);
	hdmi_writei2c_byte(0xde, 0x20);
	hdmi_writei2c_byte(0xde, 0xe0);

	hdmi_writei2c_byte(0xff, 0x80);
	hdmi_writei2c_byte(0x18, 0xdc); /* txpll sw rst */
	hdmi_writei2c_byte(0x18, 0xfc);
	hdmi_writei2c_byte(0x16, 0xf1); /* txpll calibration rest */
	hdmi_writei2c_byte(0x16, 0xf3);

	hdmi_writei2c_byte(0x11, 0x5a); //Pcr reset
	hdmi_writei2c_byte(0x11, 0xfa);

	hdmi_writei2c_byte(0xff, 0x81);
	hdmi_writei2c_byte(0x30, 0xea);
}

void lt9611_hdmi_out_disable(void)
{
	hdmi_writei2c_byte(0xff, 0x81);
	hdmi_writei2c_byte(0x30, 0x00); /* Txphy PD */
	hdmi_writei2c_byte(0x23, 0x80); /* Txpll PD */
}

void lt9611_hdmi_tx_digital(struct video_timing *video_timing)
{
	u8 HDMI_VIC = video_timing->vic;
	u8 AR = video_timing->aspact_ratio;
	u8 pb0, pb2, pb4;

	pb2 =  (AR << 4) + 0x08;
	pb4 =  HDMI_VIC;

	if ((pb2 + pb4) < 0x5f)
		pb0 = 0x5f - pb2 - pb4;
	else
		pb0 = 0x15f - pb2 - pb4;

	//AVI
	hdmi_writei2c_byte(0xff, 0x84);
	hdmi_writei2c_byte(0x43, pb0);   //AVI_PB0

	//hdmi_writei2c_byte(0x44, 0x10);                    //AVI_PB1
	hdmi_writei2c_byte(0x45, pb2);            //AVI_PB2
	hdmi_writei2c_byte(0x47, pb4);                //AVI_PB4

	hdmi_writei2c_byte(0xff, 0x84);
	hdmi_writei2c_byte(0x10, 0x02); //data iland
	hdmi_writei2c_byte(0x12, 0x64); //act_h_blank
}

void lt9611_csc(void)
{
	if (lt9611.mipi_mode == CSI) {
		hdmi_writei2c_byte(0xff, 0x82);
		hdmi_writei2c_byte(0xb9, 0x18);
		LT_INFO("Ypbpr 422 to RGB888\n");
	}
}


void lt9611_audio_init(void)
{
	if (lt9611.audio_out == AUDIO_I2S) {
		LT_INFO("Audio inut = I2S 2ch\n");
		hdmi_writei2c_byte(0xff, 0x82);
		hdmi_writei2c_byte(0xd6, 0x8e); //0x8e
		hdmi_writei2c_byte(0xd7, 0x04); //sync polarity

		hdmi_writei2c_byte(0xff, 0x84);
		hdmi_writei2c_byte(0x06, 0x08);
		hdmi_writei2c_byte(0x07, 0x10);

		hdmi_writei2c_byte(0x0f, 0x29); //0x29: 48K, 20bit
		hdmi_writei2c_byte(0x34, 0xd4); //CTS_N
	}

	if (lt9611.audio_out == AUDIO_SPDIF) {
		LT_INFO("Audio inut = SPDIF\n");
		hdmi_writei2c_byte(0xff, 0x82);
		hdmi_writei2c_byte(0xd6, 0x8e);
		hdmi_writei2c_byte(0xd7, 0x04); //sync polarity

		hdmi_writei2c_byte(0xff, 0x84);
		hdmi_writei2c_byte(0x06, 0x0c);
		hdmi_writei2c_byte(0x07, 0x10);

		hdmi_writei2c_byte(0x34, 0xd4); //CTS_N
	}
}

void lt9611_irq_init(void)
{
	hdmi_writei2c_byte(0xff, 0x82);
	//hdmi_writei2c_byte(0x10, 0x00); //Output low level active;
	hdmi_writei2c_byte(0x58, 0x0a); //Det HPD
	hdmi_writei2c_byte(0x59, 0x80); //HPD debounce width

	//hdmi_writei2c_byte(0x9e, 0xff); //vid chk clk
	hdmi_writei2c_byte(0x9e, 0xf7);

	hdmi_writei2c_byte(0x00, 0xfe);   //mask0 vid_chk_IRQ
	//hdmi_writei2c_byte(0x01, 0xff); //mask1
	//hdmi_writei2c_byte(0x02, 0xff); //mask2

	hdmi_writei2c_byte(0x03, 0x3f); //mask3  //Tx_det
	hdmi_writei2c_byte(0x04, 0xff); //clear0
	hdmi_writei2c_byte(0x04, 0xfe); //clear0
	//hdmi_writei2c_byte(0x05, 0xff); //clear1
	//hdmi_writei2c_byte(0x06, 0xff); //clear2
	hdmi_writei2c_byte(0x07, 0xff); //clear3
	hdmi_writei2c_byte(0x07, 0x3f); //clear3
	//flag0 = (hdmi_readi2c_byte(0x0c);
	//flag1 = (hdmi_readi2c_byte(0x0d);
	//flag2 = (hdmi_readi2c_byte(0x0e);
	//flag3 = (hdmi_readi2c_byte(0x0f);
}

void lt9611_read_edid(void)
{
#ifdef _enable_read_edid_
	u8 i, j;

	hdmi_writei2c_byte(0xff, 0x85);
	//hdmi_writei2c_byte(0x02, 0x0a);
	hdmi_writei2c_byte(0x03, 0xc9);
	hdmi_writei2c_byte(0x04, 0xa0);
	hdmi_writei2c_byte(0x05, 0x00);
	hdmi_writei2c_byte(0x06, 0x20);
	hdmi_writei2c_byte(0x14, 0x7f);
	for (i = 0; i < 8; i++) {
		hdmi_writei2c_byte(0x05, i * 32);
		hdmi_writei2c_byte(0x07, 0x36);
		msleep(5);
		hdmi_writei2c_byte(0x07, 0x31);
		hdmi_writei2c_byte(0x07, 0x37);
		msleep(20);
		if (hdmi_readi2c_byte(0x40) & 0x02) {
			if (hdmi_readi2c_byte(0x40) & 0x50) {
				LT_INFO("read edid failed: no ack\n");
				goto end;
			} else {
				for (j = 0; j < 32; j++) {
					Sink_EDID[i * 32 + j] = hdmi_readi2c_byte(0x83);
				}
			}
		} else {
			LT_INFO("read edid failed: accs not done\n");
			goto end;
		}
	}
	LT_INFO("read edid succeeded, checksum = %ld\n", Sink_EDID[255]);

end:
hdmi_writei2c_byte(0x03, 0xc2);
hdmi_writei2c_byte(0x07, 0x1f);
#endif
}

void lt9611_hpd_status(void)
{
	hdmi_writei2c_byte(0xff, 0x82);
	if (hdmi_readi2c_byte(0x5e) & 0x04)
		tx_hpd = 1;
	else
		tx_hpd = 0;
}

void lt9611_hdcp_init(void)
{
	hdmi_writei2c_byte(0xff, 0x85);
	hdmi_writei2c_byte(0x07, 0x1f);
	hdmi_writei2c_byte(0x13, 0xfe);
	hdmi_writei2c_byte(0x17, 0x0f);
	hdmi_writei2c_byte(0x15, 0x05);
	//hdmi_writei2c_byte(0x15, 0x65);
}

void lt9611_hdcp_enable(void)
{
	hdmi_writei2c_byte(0xff, 0x80);
	hdmi_writei2c_byte(0x14, 0x7f);
	hdmi_writei2c_byte(0x14, 0xff);
	hdmi_writei2c_byte(0xff, 0x85);
	hdmi_writei2c_byte(0x15, 0x01); //disable HDCP
	hdmi_writei2c_byte(0x15, 0x71); //enable HDCP
	hdmi_writei2c_byte(0x15, 0x65); //enable HDCP
}

void lt9611_hdcp_disable(void)
{
	hdmi_writei2c_byte(0xff, 0x85);
	hdmi_writei2c_byte(0x15, 0x45); //enable HDCP
}

void lt9611_hdmi_cec_on(bool enable)
{
	if (enable) {
		/* cec init */
		hdmi_writei2c_byte(0xff, 0x80);
		hdmi_writei2c_byte(0x0d, 0xff);
		hdmi_writei2c_byte(0x15, 0xf9);
		hdmi_writei2c_byte(0xff, 0x86);
		hdmi_writei2c_byte(0xfa, 0x00);
		hdmi_writei2c_byte(0xfe, 0xa5);

		/* cec irq init */
		hdmi_writei2c_byte(0xff, 0x82);
		hdmi_writei2c_byte(0x01, 0x7f); //mask bit[7]
		hdmi_writei2c_byte(0x05, 0xff); //clr bit[7]
		hdmi_writei2c_byte(0x05, 0x7f);
	} else {
		hdmi_writei2c_byte(0xff, 0x80);
		hdmi_writei2c_byte(0x15, 0xf1);
	}
}

void lt9611_frequency_meter_byte_clk(void)
{
	u8 temp;
	u32 reg = 0x00;

	/* port A byte clk meter */
	hdmi_writei2c_byte(0xff, 0x82);
	hdmi_writei2c_byte(0xc7, 0x03); //PortA
	msleep(50);
	temp = hdmi_readi2c_byte(0xcd);
	if ((temp & 0x60) == 0x60) { /* clk stable */
		reg = (u32)(temp & 0x0f) * 65536;
		temp = hdmi_readi2c_byte(0xce);
		reg = reg + (u16)temp * 256;
		temp = hdmi_readi2c_byte(0xcf);
		reg = reg + temp;
		LT_INFO("port A byte clk = %d\n", reg);
	} else /* clk unstable */
		LT_INFO("port A byte clk unstable\n");

	/* port B byte clk meter */
	hdmi_writei2c_byte(0xff, 0x82);
	hdmi_writei2c_byte(0xc7, 0x04);
	msleep(50);
	temp = hdmi_readi2c_byte(0xcd);
	if ((temp & 0x60) == 0x60) { /* clk stable */
		reg = (u32)(temp & 0x0f) * 65536;
		temp = hdmi_readi2c_byte(0xce);
		reg = reg + (u16)temp * 256;
		temp = hdmi_readi2c_byte(0xcf);
		reg = reg + temp;
		LT_INFO("port B byte clk =%d\n", reg); //Dec
	} else /* clk unstable */
		LT_INFO("port B byte clk unstable\n");
}

void lt9611_htotal_sysclk(void)
{
#ifdef _htotal_stable_check_
	u16 reg;
	u8 loopx;

	for (loopx = 0; loopx < 10; loopx++) {
		hdmi_writei2c_byte(0xff, 0x82);
		reg = hdmi_readi2c_byte(0x86);
		reg = reg * 256 + hdmi_readi2c_byte(0x87);
		LT_INFO("Htotal_Sysclk = %d\n", reg);
	}
#endif
}

void lt9611_pcr_mk_lt_info(void)
{
#ifdef _pcr_mk_printf_
	u8 loopx;

	for (loopx = 0; loopx < 8; loopx++) {
		hdmi_writei2c_byte(0xff, 0x83);
		LT_INFO("PCR stable 0x8397[4] = %x, SYNC_INTG_M[6:0] = %x, "
			hdmi_readi2c_byte(0x97), hdmi_readi2c_byte(0xb4));

		LT_INFO("SYNC_FRAC_K[23:16] = %x, SYNC_FRAC_K[15:8] = %x "
			hdmi_readi2c_byte(0xb5), hdmi_readi2c_byte(0xb6));

		LT_INFO("SYNC_FRAC_K[7:0] = %x\n",
			hdmi_readi2c_byte(0xb7));
		msleep(1000);
	}
#endif
}

void lt9611_dphy_debug(void)
{
#ifdef _mipi_dphy_debug_
	u8 temp;

	hdmi_writei2c_byte(0xff, 0x83);
	temp = hdmi_readi2c_byte(0xbc);
	if (temp == 0x55)
		LT_INFO("port A lane PN is right\n");
	else
		LT_INFO("port A lane PN error 0x83bc = %x\n", temp);

	temp = hdmi_readi2c_byte(0x99);
	if (temp == 0xb8)
		LT_INFO("port A lane 0 sot right\n");
	else
		LT_INFO("port A lane 0 sot error = %x\n", temp);

	temp = hdmi_readi2c_byte(0x9b);
	if (temp == 0xb8)
		LT_INFO("port A lane 1 sot right\n");
	else
		LT_INFO("port A lane 1 sot error = %x\n", temp);

	temp = hdmi_readi2c_byte(0x9d);
	if (temp == 0xb8)
		LT_INFO("port A lane 2 sot right\n");
	else
		LT_INFO("port A lane 2 sot error = %x\n", temp);

	temp = hdmi_readi2c_byte(0x9f);
	if (temp == 0xb8)
		LT_INFO("port A lane 3 sot right\n");
	else
		LT_INFO("port A lane 3 sot error = %x\n", temp);

	LT_INFO("port A lane 0 settle = %x\n", hdmi_readi2c_byte(0x98));
	LT_INFO("port A lane 1 settle = %x\n", hdmi_readi2c_byte(0x9a));
	LT_INFO("port A lane 2 settle = %x\n", hdmi_readi2c_byte(0x9c));
	LT_INFO("port A lane 3 settle = %x\n", hdmi_readi2c_byte(0x9e));
#endif
}

int lt9611_init(void)
{
	int ret = 0;

	//reset_lt9611();
	lt9611_system_init();
	//lt9611_rst_rd_init();
	lt9611_mipi_input_analog();
	lt9611_mipi_input_digtal();

	msleep(1000);
	lt9611_video_check();

	ret = lt9611_pll(video);
	if (ret) {
		LT_ERR("lt9611 init failed\n");
		return ret;
	}

	lt9611_mipi_pcr(video); //pcr setup

	lt9611_audio_init();
	lt9611_csc();
	lt9611_hdcp_init();
	lt9611_hdmi_tx_digital(video);
	lt9611_hdmi_tx_phy();

	lt9611_irq_init();
	//lt9611_hdcp_enable();
	//lt9611_read_edid();

	lt9611_hdmi_cec_on(1);

	LT_INFO("############lt9611 initial End##################\n");

	lt9611_hpd_status();
	if (tx_hpd) {
		LT_INFO("Detect hpd High\n");
		lt9611_read_edid();
		lt9611_hdmi_out_enable();
		lt9611_hdcp_enable();
	} else {
		LT_INFO("Detect hpd Low\n");
		lt9611_hdcp_disable();
		lt9611_hdmi_out_disable();
	}

	lt9611_frequency_meter_byte_clk();
	lt9611_dphy_debug();
	lt9611_htotal_sysclk();
	lt9611_pcr_mk_lt_info();

	return 0;
}

static int lt9611_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		LT_ERR("I2C check functionality fail!");
		return -ENODEV;
	}

	LT_ERR("1024\n");
	LT_INFO("lt_slave_addr:0x%X,client->addr:%X\n",
		lt_slave_addr, client->addr);
	lt9611_i2c.client = client;
	lt9611_chip_id();

	ret = lt9611_init();
	if (ret) {
		LT_ERR("I2C device probe failed\n");
		return ret;
	}

	LT_INFO("I2C device probe OK\n");

	return 0;
}

static int lt9611_i2c_remove(struct i2c_client *client)
{
	i2c_set_clientdata(client, NULL);
	lt9611_i2c.client = NULL;

	return 0;
}

static const struct i2c_device_id lt9611_i2c_ids[] = {
	{ "lt9611_i2c_drv", 0 },
	{ }
};

static const struct of_device_id lt9611_i2c_matchs[] = {
	{ .compatible = "lt9611,i2c", },
	{ }
};

static struct i2c_driver lt9611_i2c_driver = {
	.probe		= lt9611_i2c_probe,
	.remove		= lt9611_i2c_remove,
	.id_table	= lt9611_i2c_ids,
	.driver	= {
		.name	= "lt9611_i2c_drv",
		.owner	= THIS_MODULE,
		.bus = &i2c_bus_type,
		.of_match_table = lt9611_i2c_matchs,
	},
};

static int __init lt9611_i2c_init(void)
{
	return i2c_add_driver(&lt9611_i2c_driver);
}

static void lt9611_i2c_exit(void)
{
	LT_INFO("lt9611 i2c exit\n");
	i2c_del_driver(&lt9611_i2c_driver);
}

module_init(lt9611_i2c_init);
module_exit(lt9611_i2c_exit);

MODULE_AUTHOR("ashley.chen@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum MIPI Switch HDMI Drivers");
MODULE_LICENSE("GPL");
