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

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>

#include "dwc_mipi_dsi_host.h"
#include "sprd_dsi.h"

#define read32(c)	readl((void __force __iomem *)(c))
#define write32(v, c)	writel(v, (void __force __iomem *)(c))

/**
 * Get DSI Host core version
 * @param instance pointer to structure holding the DSI Host core information
 * @return ascii number of the version
 */
static bool dsi_check_version(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	/* DWC_mipi_dsi_host_1.31a */
	if (read32(&reg->VERSION) == 0x3133312a)
		return true;

	/* DWC_mipi_dsi_host_1.21a */
	if (read32(&reg->VERSION) == 0x3132302a)
		return true;

	return false;
}
/**
 * Modify power status of DSI Host core
 * @param instance pointer to structure holding the DSI Host core information
 * @param on (1) or off (0)
 */
static void dsi_power_enable(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	write32(enable, &reg->PWR_UP);
}
/**
 * Enable/disable DPI video mode
 * @param instance pointer to structure holding the DSI Host core information
 */
static void dsi_video_mode(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	write32(0, &reg->MODE_CFG);
}
/**
 * Enable command mode (Generic interface)
 * @param instance pointer to structure holding the DSI Host core information
 */
static void dsi_cmd_mode(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	write32(1, &reg->MODE_CFG);
}

static bool dsi_is_cmd_mode(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	return read32(&reg->MODE_CFG);
}
/**
 * Configure the read back virtual channel for the generic interface
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc to listen to on the line
 */
static void dsi_rx_vcid(struct dsi_context *ctx, u8 vc)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	write32(vc, &reg->GEN_VCID);
}
/**
 * Write the DPI video virtual channel destination
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc virtual channel
 */
static void dsi_video_vcid(struct dsi_context *ctx, u8 vc)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	write32(vc, &reg->DPI_VCID);
}
/**
 * Set DPI video mode type (burst/non-burst - with sync pulses or events)
 * @param instance pointer to structure holding the DSI Host core information
 * @param type
 * @return error code
 */
static void dsi_dpi_video_burst_mode(struct dsi_context *ctx, int mode)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = read32(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.vid_mode_type = mode;

	write32(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}
/**
 * Set DPI video color coding
 * @param instance pointer to structure holding the DSI Host core information
 * @param color_coding enum (configuration and color depth)
 * @return error code
 */
static void dsi_dpi_color_coding(struct dsi_context *ctx, int coding)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x10 dpi_color_coding;

	dpi_color_coding.val = read32(&reg->DPI_COLOR_CODING);
	dpi_color_coding.bits.dpi_color_coding = coding;

	write32(dpi_color_coding.val, &reg->DPI_COLOR_CODING);
}
/**
 * Set DPI loosely packetisation video (used only when color depth = 18
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_dpi_18_loosely_packet_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x10 dpi_color_coding;

	dpi_color_coding.val = read32(&reg->DPI_COLOR_CODING);
	dpi_color_coding.bits.loosely18_en = enable;

	write32(dpi_color_coding.val, &reg->DPI_COLOR_CODING);
}
/*
 * Set DPI color mode pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void dsi_dpi_color_mode_pol(struct dsi_context *ctx, int active_low)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x14 dpi_cfg_pol;

	dpi_cfg_pol.val = read32(&reg->DPI_CFG_POL);
	dpi_cfg_pol.bits.colorm_active_low = active_low;

	write32(dpi_cfg_pol.val, &reg->DPI_CFG_POL);
}
/*
 * Set DPI shut down pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void dsi_dpi_shut_down_pol(struct dsi_context *ctx, int active_low)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x14 dpi_cfg_pol;

	dpi_cfg_pol.val = read32(&reg->DPI_CFG_POL);
	dpi_cfg_pol.bits.shutd_active_low = active_low;

	write32(dpi_cfg_pol.val, &reg->DPI_CFG_POL);
}
/*
 * Set DPI horizontal sync pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void dsi_dpi_hsync_pol(struct dsi_context *ctx, int active_low)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x14 dpi_cfg_pol;

	dpi_cfg_pol.val = read32(&reg->DPI_CFG_POL);
	dpi_cfg_pol.bits.hsync_active_low = active_low;

	write32(dpi_cfg_pol.val, &reg->DPI_CFG_POL);
}
/*
 * Set DPI vertical sync pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void dsi_dpi_vsync_pol(struct dsi_context *ctx, int active_low)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x14 dpi_cfg_pol;

	dpi_cfg_pol.val = read32(&reg->DPI_CFG_POL);
	dpi_cfg_pol.bits.vsync_active_low = active_low;

	write32(dpi_cfg_pol.val, &reg->DPI_CFG_POL);
}
/*
 * Set DPI data enable pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void dsi_dpi_data_en_pol(struct dsi_context *ctx, int active_low)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x14 dpi_cfg_pol;

	dpi_cfg_pol.val = read32(&reg->DPI_CFG_POL);
	dpi_cfg_pol.bits.dataen_active_low = active_low;

	write32(dpi_cfg_pol.val, &reg->DPI_CFG_POL);
}
/*
 * Configure the Horizontal Line time
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle taken to transmit the total of the horizontal line
 */
static void dsi_dpi_hline_time(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x50 vid_hline_time;

	vid_hline_time.val = read32(&reg->VID_HLINE_TIME);
	vid_hline_time.bits.vid_hline_time = byte_cycle;

	write32(vid_hline_time.val, &reg->VID_HLINE_TIME);
}
/**
 * Configure the Horizontal back porch time
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle taken to transmit the horizontal back porch
 */
static void dsi_dpi_hbp_time(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x4C vid_hbp_time;

	vid_hbp_time.val = read32(&reg->VID_HBP_TIME);
	vid_hbp_time.bits.vid_hbp_time = byte_cycle;

	write32(vid_hbp_time.val, &reg->VID_HBP_TIME);
}
/**
 * Configure the Horizontal sync time
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle taken to transmit the horizontal sync
 */
static void dsi_dpi_hsync_time(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x48 vid_hsa_time;

	vid_hsa_time.val = read32(&reg->VID_HSA_TIME);
	vid_hsa_time.bits.vid_hsa_time = byte_cycle;

	write32(vid_hsa_time.val, &reg->VID_HSA_TIME);
}
/**
 * Configure the vertical active lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void dsi_dpi_vact(struct dsi_context *ctx, u16 lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x60 vid_vactive_lines;

	vid_vactive_lines.val = read32(&reg->VID_VACTIVE_LINES);
	vid_vactive_lines.bits.v_active_lines = lines;

	write32(vid_vactive_lines.val, &reg->VID_VACTIVE_LINES);
}
/**
 * Configure the vertical front porch lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void dsi_dpi_vfp(struct dsi_context *ctx, u16 lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x5C vid_vfp_lines;

	vid_vfp_lines.val = read32(&reg->VID_VFP_LINES);
	vid_vfp_lines.bits.vfp_lines = lines;

	write32(vid_vfp_lines.val, &reg->VID_VFP_LINES);
}
/**
 * Configure the vertical back porch lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void dsi_dpi_vbp(struct dsi_context *ctx, u16 lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x58 vid_vbp_lines;

	vid_vbp_lines.val = read32(&reg->VID_VBP_LINES);
	vid_vbp_lines.bits.vbp_lines = lines;

	write32(vid_vbp_lines.val, &reg->VID_VBP_LINES);
}
/**
 * Configure the vertical sync lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void dsi_dpi_vsync(struct dsi_context *ctx, u16 lines)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x54 vid_vsa_lines;

	vid_vsa_lines.val = read32(&reg->VID_VSA_LINES);
	vid_vsa_lines.bits.vsa_lines = lines;

	write32(vid_vsa_lines.val, &reg->VID_VSA_LINES);
}
/**
 * Enable return to low power mode inside horizontal front porch periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_dpi_hporch_lp_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = read32(&reg->VID_MODE_CFG);

	vid_mode_cfg.bits.lp_vact_en = enable;
	vid_mode_cfg.bits.lp_hfp_en = enable;
	vid_mode_cfg.bits.lp_hbp_en = enable;

	write32(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}
/**
 * Enable return to low power mode inside vertical active lines periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_dpi_vporch_lp_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = read32(&reg->VID_MODE_CFG);

	vid_mode_cfg.bits.lp_vfp_en = enable;
	vid_mode_cfg.bits.lp_vbp_en = enable;
	vid_mode_cfg.bits.lp_vsa_en = enable;

	write32(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}
/**
 * Enable FRAME BTA ACK
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_dpi_frame_ack_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = read32(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.frame_bta_ack_en = enable;

	write32(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}
/*
 * Write no of chunks to core - taken into consideration only when multi packet
 * is enabled
 * @param instance pointer to structure holding the DSI Host core information
 * @param no of chunks
 */
static void dsi_dpi_chunk_num(struct dsi_context *ctx, u16 num)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x40 vid_num_chunks;

	vid_num_chunks.val = read32(&reg->VID_NUM_CHUNKS);
	vid_num_chunks.bits.vid_num_chunks = num;

	write32(vid_num_chunks.val, &reg->VID_NUM_CHUNKS);
}
/**
 * Write the null packet size - will only be taken into account when null
 * packets are enabled.
 * @param instance pointer to structure holding the DSI Host core information
 * @param size of null packet
 * @return error code
 */
static void dsi_dpi_null_packet_size(struct dsi_context *ctx, u16 size)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x44 vid_null_size;

	vid_null_size.val = read32(&reg->VID_NULL_SIZE);
	vid_null_size.bits.vid_null_size = size;

	write32(vid_null_size.val, &reg->VID_NULL_SIZE);
}
/**
 * Write video packet size. obligatory for sending video
 * @param instance pointer to structure holding the DSI Host core information
 * @param size of video packet - containing information
 * @return error code
 */
static void dsi_dpi_video_packet_size(struct dsi_context *ctx, u16 size)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x3C vid_pkt_size;

	vid_pkt_size.val = read32(&reg->VID_PKT_SIZE);
	vid_pkt_size.bits.vid_pkt_size = size;

	write32(vid_pkt_size.val, &reg->VID_PKT_SIZE);
}
/**
 * Specifiy the size of the packet memory write start/continue
 * @param instance pointer to structure holding the DSI Host core information
 * @ size of the packet
 * @note when different than zero (0) eDPI is enabled
 */
static void dsi_edpi_max_pkt_size(struct dsi_context *ctx, u16 size)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x64 edpi_cmd_size;

	edpi_cmd_size.val = read32(&reg->EDPI_CMD_SIZE);
	edpi_cmd_size.bits.edpi_allowed_cmd_size = size;

	write32(edpi_cmd_size.val, &reg->EDPI_CMD_SIZE);
}
/**
 * Enable tear effect acknowledge
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_tear_effect_ack_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x68 cmd_mode_cfg;

	cmd_mode_cfg.val = read32(&reg->CMD_MODE_CFG);
	cmd_mode_cfg.bits.tear_fx_en = enable;

	write32(cmd_mode_cfg.val, &reg->CMD_MODE_CFG);
}
/**
 * Enable packets acknowledge request after each packet transmission
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void dsi_cmd_ack_request_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x68 cmd_mode_cfg;

	cmd_mode_cfg.val = read32(&reg->CMD_MODE_CFG);
	cmd_mode_cfg.bits.ack_rqst_en = enable;

	write32(cmd_mode_cfg.val, &reg->CMD_MODE_CFG);
}
/**
 * Set DCS command packet transmission to transmission type
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_param of command
 * @param lp transmit in low power
 * @return error code
 */
static void dsi_cmd_mode_lp_cmd_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x68 cmd_mode_cfg;

	cmd_mode_cfg.val = read32(&reg->CMD_MODE_CFG);

	cmd_mode_cfg.bits.gen_sw_0p_tx = enable;
	cmd_mode_cfg.bits.gen_sw_1p_tx = enable;
	cmd_mode_cfg.bits.gen_sw_2p_tx = enable;
	cmd_mode_cfg.bits.gen_lw_tx = enable;
	cmd_mode_cfg.bits.dcs_sw_0p_tx = enable;
	cmd_mode_cfg.bits.dcs_sw_1p_tx = enable;
	cmd_mode_cfg.bits.dcs_lw_tx = enable;
	cmd_mode_cfg.bits.max_rd_pkt_size = enable;

	cmd_mode_cfg.bits.gen_sr_0p_tx = enable;
	cmd_mode_cfg.bits.gen_sr_1p_tx = enable;
	cmd_mode_cfg.bits.gen_sr_2p_tx = enable;
	cmd_mode_cfg.bits.dcs_sr_0p_tx = enable;

	write32(cmd_mode_cfg.val, &reg->CMD_MODE_CFG);
}
/**
 * Set DCS read command packet transmission to transmission type
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_param of command
 * @param lp transmit in low power
 * @return error code
 */
static void dsi_video_mode_lp_cmd_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x38 vid_mode_cfg;

	vid_mode_cfg.val = read32(&reg->VID_MODE_CFG);
	vid_mode_cfg.bits.lp_cmd_en = enable;

	write32(vid_mode_cfg.val, &reg->VID_MODE_CFG);
}

/**
 * Write command header in the generic interface
 * (which also sends DCS commands) as a subset
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc of destination
 * @param packet_type (or type of DCS command)
 * @param ls_byte (if DCS, it is the DCS command)
 * @param ms_byte (only parameter of short DCS packet)
 * @return error code
 */
static void dsi_set_packet_header(struct dsi_context *ctx,
				   u8 vc,
				   u8 type,
				   u8 wc_lsb,
				   u8 wc_msb)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x6C gen_hdr;

	gen_hdr.bits.gen_dt = type;
	gen_hdr.bits.gen_vc = vc;
	gen_hdr.bits.gen_wc_lsbyte = wc_lsb;
	gen_hdr.bits.gen_wc_msbyte = wc_msb;

	write32(gen_hdr.val, &reg->GEN_HDR);
}
/**
 * Write the payload of the long packet commands
 * @param instance pointer to structure holding the DSI Host core information
 * @param payload array of bytes of payload
 * @return error code
 */
static void dsi_set_packet_payload(struct dsi_context *ctx, u32 payload)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	write32(payload, &reg->GEN_PLD_DATA);
}
/**
 * Write the payload of the long packet commands
 * @param instance pointer to structure holding the DSI Host core information
 * @param payload pointer to 32-bit array to hold read information
 * @return error code
 */
static u32 dsi_get_rx_payload(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	return read32(&reg->GEN_PLD_DATA);
}

/**
 * Enable Bus Turn-around request
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_bta_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C pckhdl_cfg;

	pckhdl_cfg.val = read32(&reg->PCKHDL_CFG);
	pckhdl_cfg.bits.bta_en = enable;

	write32(pckhdl_cfg.val, &reg->PCKHDL_CFG);
}
/**
 * Enable EOTp reception
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_eotp_rx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C pckhdl_cfg;

	pckhdl_cfg.val = read32(&reg->PCKHDL_CFG);
	pckhdl_cfg.bits.eotp_rx_en = enable;

	write32(pckhdl_cfg.val, &reg->PCKHDL_CFG);
}
/**
 * Enable EOTp transmission
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_eotp_tx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C pckhdl_cfg;

	pckhdl_cfg.val = read32(&reg->PCKHDL_CFG);
	pckhdl_cfg.bits.eotp_tx_en = enable;

	write32(pckhdl_cfg.val, &reg->PCKHDL_CFG);
}
/**
 * Enable ECC reception, error correction and reporting
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_ecc_rx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C pckhdl_cfg;

	pckhdl_cfg.val = read32(&reg->PCKHDL_CFG);
	pckhdl_cfg.bits.ecc_rx_en = enable;

	write32(pckhdl_cfg.val, &reg->PCKHDL_CFG);
}
/**
 * Enable CRC reception, error reporting
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void dsi_crc_rx_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x2C pckhdl_cfg;

	pckhdl_cfg.val = read32(&reg->PCKHDL_CFG);
	pckhdl_cfg.bits.crc_rx_en = enable;

	write32(pckhdl_cfg.val, &reg->PCKHDL_CFG);
}
/**
 * Get status of read command
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if busy
 */
static bool dsi_is_bta_returned(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = read32(&reg->CMD_PKT_STATUS);

	return !cmd_pkt_status.bits.gen_cmd_rdcmd_ongoing;
}
/**
 * Get the FULL status of generic read payload fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo full
 */
static bool dsi_is_rx_payload_fifo_full(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = read32(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_cmd_rdata_fifo_full;
}
/**
 * Get the EMPTY status of generic read payload fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo empty
 */
static bool dsi_is_rx_payload_fifo_empty(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = read32(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_cmd_rdata_fifo_empty;
}
/**
 * Get the FULL status of generic write payload fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo full
 */
static bool dsi_is_tx_payload_fifo_full(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = read32(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_cmd_wdata_fifo_full;
}
/**
 * Get the EMPTY status of generic write payload fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo empty
 */
static bool dsi_is_tx_payload_fifo_empty(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = read32(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_cmd_wdata_fifo_empty;
}
/**
 * Get the FULL status of generic command fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo full
 */
static bool dsi_is_tx_cmd_fifo_full(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = read32(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_cmd_cmd_fifo_full;
}
/**
 * Get the EMPTY status of generic command fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo empty
 */
static bool dsi_is_tx_cmd_fifo_empty(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x74 cmd_pkt_status;

	cmd_pkt_status.val = read32(&reg->CMD_PKT_STATUS);

	return cmd_pkt_status.bits.gen_cmd_cmd_fifo_empty;
}

/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch data lane from high speed to low power
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle
 * @return error code
 */
static void dsi_datalane_hs2lp_config(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x9C phy_tmr_cfg;

	phy_tmr_cfg.val = read32(&reg->PHY_TMR_CFG);
	phy_tmr_cfg.bits.phy_hs2lp_time = byte_cycle;

	write32(phy_tmr_cfg.val, &reg->PHY_TMR_CFG);
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch the data lane from to low power high speed
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle
 * @return error code
 */
static void dsi_datalane_lp2hs_config(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x9C phy_tmr_cfg;

	phy_tmr_cfg.val = read32(&reg->PHY_TMR_CFG);
	phy_tmr_cfg.bits.phy_lp2hs_time = byte_cycle;

	write32(phy_tmr_cfg.val, &reg->PHY_TMR_CFG);
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch clock lane from high speed to low power
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle
 * @return error code
 */
static void dsi_clklane_hs2lp_config(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x98 phy_tmr_lpclk_cfg;

	phy_tmr_lpclk_cfg.val = read32(&reg->PHY_TMR_LPCLK_CFG);
	phy_tmr_lpclk_cfg.bits.phy_clkhs2lp_time = byte_cycle;

	write32(phy_tmr_lpclk_cfg.val, &reg->PHY_TMR_LPCLK_CFG);
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch clock lane from to low power high speed
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle
 * @return error code
 */
static void dsi_clklane_lp2hs_config(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x98 phy_tmr_lpclk_cfg;

	phy_tmr_lpclk_cfg.val = read32(&reg->PHY_TMR_LPCLK_CFG);
	phy_tmr_lpclk_cfg.bits.phy_clklp2hs_time = byte_cycle;

	write32(phy_tmr_lpclk_cfg.val, &reg->PHY_TMR_LPCLK_CFG);
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to turn the bus around to start receiving
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle
 * @return error code
 */
static void dsi_max_read_time(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xF4 phy_tmr_rd_cfg;

	phy_tmr_rd_cfg.val = read32(&reg->PHY_TMR_RD_CFG);
	phy_tmr_rd_cfg.bits.max_rd_time = byte_cycle;

	write32(phy_tmr_rd_cfg.val, &reg->PHY_TMR_RD_CFG);
}
/**
 * Enable the automatic mechanism to stop providing clock in the clock
 * lane when time allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 * @return error code
 */
static void dsi_nc_clk_en(struct dsi_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x94 lpclk_ctrl;

	lpclk_ctrl.val = read32(&reg->LPCLK_CTRL);
	lpclk_ctrl.bits.auto_clklane_ctrl = enable;

	write32(lpclk_ctrl.val, &reg->LPCLK_CTRL);
}
/**
 * Write transmission escape timeout
 * a safe guard so that the state machine would reset if transmission
 * takes too long
 * @param instance pointer to structure holding the DSI Host core information
 * @param div
 */
static void dsi_tx_escape_division(struct dsi_context *ctx, u8 div)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x08 clkmgr_cfg;

	clkmgr_cfg.val = read32(&reg->CLKMGR_CFG);
	clkmgr_cfg.bits.tx_esc_clk_division = div;

	write32(clkmgr_cfg.val, &reg->CLKMGR_CFG);
}
/* PRESP Time outs */
/**
 * configure timeout divisions (so they would have more clock ticks)
 * @param instance pointer to structure holding the DSI Host core information
 * @param div no of hs cycles before transiting back to LP in
 *  (lane_clk / div)
 */
static void dsi_timeout_clock_division(struct dsi_context *ctx, u8 div)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x08 clkmgr_cfg;

	clkmgr_cfg.val = read32(&reg->CLKMGR_CFG);
	clkmgr_cfg.bits.to_clk_division = div;

	write32(clkmgr_cfg.val, &reg->CLKMGR_CFG);
}
/**
 * Configure the Low power receive time out
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle (of byte cycles)
 */
static void dsi_lp_rx_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x78 to_cnt_cfg;

	to_cnt_cfg.val = read32(&reg->TO_CNT_CFG);
	to_cnt_cfg.bits.lprx_to_cnt = byte_cycle;

	write32(to_cnt_cfg.val, &reg->TO_CNT_CFG);
}
/**
 * Configure a high speed transmission time out
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_cycle (byte cycles)
 */
static void dsi_hs_tx_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x78 to_cnt_cfg;

	to_cnt_cfg.val = read32(&reg->TO_CNT_CFG);
	to_cnt_cfg.bits.hstx_to_cnt = byte_cycle;

	write32(to_cnt_cfg.val, &reg->TO_CNT_CFG);
}
/**
 * Timeout for peripheral (for controller to stay still) after bus turn around
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a BTA operation. This period is
 * measured in cycles of lanebyteclk
 */
static void dsi_bta_presp_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	write32(byte_cycle, &reg->BTA_TO_CNT);
}
/**
 * Timeout for peripheral (for controller to stay still) after LP data
 * transmission write requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a low power write operation. This period is
 * measured in cycles of lanebyteclk
 */
static void dsi_lp_write_presp_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	write32(byte_cycle, &reg->LP_WR_TO_CNT);
}
/**
 * Timeout for peripheral (for controller to stay still) after LP data
 * transmission read requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a low power read operation. This period is
 * measured in cycles of lanebyteclk
 */
static void dsi_lp_read_presp_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	write32(byte_cycle, &reg->LP_RD_TO_CNT);
}
/**
 * Timeout for peripheral (for controller to stay still) after HS data
 * transmission write requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a high-speed write operation. This period is
 * measured in cycles of lanebyteclk
 */
static void dsi_hs_write_presp_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0x84 hs_wr_to_cnt;

	hs_wr_to_cnt.val = read32(&reg->HS_WR_TO_CNT);
	hs_wr_to_cnt.bits.hs_wr_to_cnt = byte_cycle;

	write32(hs_wr_to_cnt.val, &reg->HS_WR_TO_CNT);
}
/**
 * Timeout for peripheral between HS data transmission read requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a high-speed read operation. This period is
 * measured in cycles of lanebyteclk
 */
static void dsi_hs_read_presp_timeout(struct dsi_context *ctx, u16 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	write32(byte_cycle, &reg->HS_RD_TO_CNT);
}
/**
 * Get the error 0 interrupt register status
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask the mask to be read from the register
 * @return error status 0 value
 */
static u32 dsi_int0_status(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xBC int_sts0;

	int_sts0.val = read32(&reg->INT_STS0);

	if (int_sts0.bits.dphy_errors_0)
		pr_err("dphy_err: escape entry error\n");

	if (int_sts0.bits.dphy_errors_1)
		pr_err("dphy_err: lp data transmission sync error\n");

	if (int_sts0.bits.dphy_errors_2)
		pr_err("dphy_err: control error\n");

	if (int_sts0.bits.dphy_errors_3)
		pr_err("dphy_err: LP0 contention error\n");

	if (int_sts0.bits.dphy_errors_4)
		pr_err("dphy_err: LP1 contention error\n");

	if (int_sts0.bits.ack_with_err_0)
		pr_err("ack_err: SoT error\n");

	if (int_sts0.bits.ack_with_err_1)
		pr_err("ack_err: SoT Sync error\n");

	if (int_sts0.bits.ack_with_err_2)
		pr_err("ack_err: EoT Sync error\n");

	if (int_sts0.bits.ack_with_err_3)
		pr_err("ack_err: Escape Mode Entry Command error\n");

	if (int_sts0.bits.ack_with_err_4)
		pr_err("ack_err: LP Transmit Sync error\n");

	if (int_sts0.bits.ack_with_err_5)
		pr_err("ack_err: Peripheral Timeout error\n");

	if (int_sts0.bits.ack_with_err_6)
		pr_err("ack_err: False Control error\n");

	if (int_sts0.bits.ack_with_err_7)
		pr_err("ack_err: reserved (specific to device)\n");

	if (int_sts0.bits.ack_with_err_8)
		pr_err("ack_err: ECC error, single-bit (corrected)\n");

	if (int_sts0.bits.ack_with_err_9)
		pr_err("ack_err: ECC error, multi-bit (not corrected)\n");

	if (int_sts0.bits.ack_with_err_10)
		pr_err("ack_err: checksum error (long packet only)\n");

	if (int_sts0.bits.ack_with_err_11)
		pr_err("ack_err: not recognized DSI data type\n");

	if (int_sts0.bits.ack_with_err_12)
		pr_err("ack_err: DSI VC ID Invalid\n");

	if (int_sts0.bits.ack_with_err_13)
		pr_err("ack_err: invalid transmission length\n");

	if (int_sts0.bits.ack_with_err_14)
		pr_err("ack_err: reserved (specific to device)\n");

	if (int_sts0.bits.ack_with_err_15)
		pr_err("ack_err: DSI protocol violation\n");

	return 0;
}
/**
 * Get the error 1 interrupt register status
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask the mask to be read from the register
 * @return error status 1 value
 */
static u32 dsi_int1_status(struct dsi_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;
	union _0xC0 int_sts1;
	u32 status = 0;

	int_sts1.val = read32(&reg->INT_STS1);

	if (int_sts1.bits.to_hs_tx)
		pr_err("high-speed transmission timeout\n");

	if (int_sts1.bits.to_lp_rx)
		pr_err("low-power reception timeout\n");

	if (int_sts1.bits.ecc_single_err)
		pr_err("ECC single error in a received packet\n");

	if (int_sts1.bits.ecc_multi_err)
		pr_err("ECC multiple error in a received packet\n");

	if (int_sts1.bits.crc_err)
		pr_err("CRC error in the received packet payload\n");

	if (int_sts1.bits.pkt_size_err)
		pr_err("receive packet size error\n");

	if (int_sts1.bits.eotp_err)
		pr_err("EoTp packet is not received\n");

	if (int_sts1.bits.dpi_pld_wr_err) {
		pr_err("DPI pixel-fifo is full\n");
		status |= DSI_INT_STS_NEED_SOFT_RESET;
	}

	if (int_sts1.bits.gen_cmd_wr_err)
		pr_err("cmd header-fifo is full\n");

	if (int_sts1.bits.gen_pld_wr_err)
		pr_err("cmd write-payload-fifo is full\n");

	if (int_sts1.bits.gen_pld_send_err)
		pr_err("cmd write-payload-fifo is empty\n");

	if (int_sts1.bits.gen_pld_rd_err)
		pr_err("cmd read-payload-fifo is empty\n");

	if (int_sts1.bits.gen_pld_recev_err)
		pr_err("cmd read-payload-fifo is full\n");

	return status;
}
/**
 * Configure MASK (hiding) of interrupts coming from error 0 source
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask to be written to the register
 */
static void dsi_int0_mask(struct dsi_context *ctx, u32 mask)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	/* for DWC_mipi_dsi_host_1.31a, set 1 to un-mask interrupt */
	if (reg->VERSION.val == 0x3133312a)
		write32(~mask, &reg->INT_MASK0);
	else
		write32(mask, &reg->INT_MASK0);
}
/**
 * Configure MASK (hiding) of interrupts coming from error 1 source
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask the mask to be written to the register
 */
static void dsi_int1_mask(struct dsi_context *ctx, u32 mask)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->base;

	/* for DWC_mipi_dsi_host_1.31a, set 1 to un-mask interrupt */
	if (reg->VERSION.val == 0x3133312a)
		write32(~mask, &reg->INT_MASK1);
	else
		write32(mask, &reg->INT_MASK1);
}

static struct dsi_core_ops dwc_mipi_dsi_host_ops = {
	.check_version                  = dsi_check_version,
	.power_en                       = dsi_power_enable,
	.video_mode                     = dsi_video_mode,
	.cmd_mode                       = dsi_cmd_mode,
	.is_cmd_mode                    = dsi_is_cmd_mode,
	.rx_vcid                        = dsi_rx_vcid,
	.video_vcid                     = dsi_video_vcid,
	.dpi_video_burst_mode           = dsi_dpi_video_burst_mode,
	.dpi_color_coding               = dsi_dpi_color_coding,
	.dpi_18_loosely_packet_en       = dsi_dpi_18_loosely_packet_en,
	.dpi_color_mode_pol             = dsi_dpi_color_mode_pol,
	.dpi_shut_down_pol              = dsi_dpi_shut_down_pol,
	.dpi_hsync_pol                  = dsi_dpi_hsync_pol,
	.dpi_vsync_pol                  = dsi_dpi_vsync_pol,
	.dpi_data_en_pol                = dsi_dpi_data_en_pol,
	.dpi_hline_time                 = dsi_dpi_hline_time,
	.dpi_hsync_time                 = dsi_dpi_hsync_time,
	.dpi_hbp_time                   = dsi_dpi_hbp_time,
	.dpi_vact                       = dsi_dpi_vact,
	.dpi_vfp                        = dsi_dpi_vfp,
	.dpi_vbp                        = dsi_dpi_vbp,
	.dpi_vsync                      = dsi_dpi_vsync,
	.dpi_hporch_lp_en               = dsi_dpi_hporch_lp_en,
	.dpi_vporch_lp_en               = dsi_dpi_vporch_lp_en,
	.dpi_frame_ack_en               = dsi_dpi_frame_ack_en,
	.dpi_chunk_num                  = dsi_dpi_chunk_num,
	.dpi_null_packet_size           = dsi_dpi_null_packet_size,
	.dpi_video_packet_size          = dsi_dpi_video_packet_size,
	.edpi_max_pkt_size              = dsi_edpi_max_pkt_size,
	.tear_effect_ack_en             = dsi_tear_effect_ack_en,
	.cmd_ack_request_en             = dsi_cmd_ack_request_en,
	.cmd_mode_lp_cmd_en             = dsi_cmd_mode_lp_cmd_en,
	.video_mode_lp_cmd_en           = dsi_video_mode_lp_cmd_en,
	.set_packet_header              = dsi_set_packet_header,
	.set_packet_payload             = dsi_set_packet_payload,
	.get_rx_payload                 = dsi_get_rx_payload,
	.bta_en                         = dsi_bta_en,
	.eotp_rx_en                     = dsi_eotp_rx_en,
	.eotp_tx_en                     = dsi_eotp_tx_en,
	.ecc_rx_en                      = dsi_ecc_rx_en,
	.crc_rx_en                      = dsi_crc_rx_en,
	.is_bta_returned                = dsi_is_bta_returned,
	.is_rx_payload_fifo_full        = dsi_is_rx_payload_fifo_full,
	.is_rx_payload_fifo_empty       = dsi_is_rx_payload_fifo_empty,
	.is_tx_payload_fifo_full        = dsi_is_tx_payload_fifo_full,
	.is_tx_payload_fifo_empty       = dsi_is_tx_payload_fifo_empty,
	.is_tx_cmd_fifo_full            = dsi_is_tx_cmd_fifo_full,
	.is_tx_cmd_fifo_empty           = dsi_is_tx_cmd_fifo_empty,
	.datalane_hs2lp_config          = dsi_datalane_hs2lp_config,
	.datalane_lp2hs_config          = dsi_datalane_lp2hs_config,
	.clklane_hs2lp_config           = dsi_clklane_hs2lp_config,
	.clklane_lp2hs_config           = dsi_clklane_lp2hs_config,
	.max_read_time                  = dsi_max_read_time,
	.nc_clk_en                      = dsi_nc_clk_en,
	.tx_escape_division             = dsi_tx_escape_division,
	.timeout_clock_division         = dsi_timeout_clock_division,
	.lp_rx_timeout                  = dsi_lp_rx_timeout,
	.hs_tx_timeout                  = dsi_hs_tx_timeout,
	.bta_presp_timeout              = dsi_bta_presp_timeout,
	.lp_write_presp_timeout         = dsi_lp_write_presp_timeout,
	.lp_read_presp_timeout          = dsi_lp_read_presp_timeout,
	.hs_write_presp_timeout         = dsi_hs_write_presp_timeout,
	.hs_read_presp_timeout          = dsi_hs_read_presp_timeout,
	.int0_status                    = dsi_int0_status,
	.int1_status                    = dsi_int1_status,
	.int0_mask                      = dsi_int0_mask,
	.int1_mask                      = dsi_int1_mask,
};

static struct ops_entry entry = {
	.ver = "synps,dwc-mipi-dsi-host",
	.ops = &dwc_mipi_dsi_host_ops,
};

static int __init dsi_core_register(void)
{
	return dsi_core_ops_register(&entry);
}

subsys_initcall(dsi_core_register);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("leon.he@unisoc.com");
MODULE_DESCRIPTION("DSI Low-level registers operation for SYNOPSYS");
