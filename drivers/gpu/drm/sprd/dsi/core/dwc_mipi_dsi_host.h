#ifndef _DWC_MIPI_DSI_HOST_H_
#define _DWC_MIPI_DSI_HOST_H_

#include <asm/types.h>

struct dsi_reg {
	union _0x00 {
		u32 val;
		struct _VERSION {
		u32 version: 32;
		} bits;
	} VERSION;

	union _0x04 {
		u32 val;
		struct _PWR_UP {
		/*
		 * This bit configures the core either to work normal or to
		 * reset. It's default value is 0. After the core configur-
		 * ation, to enable the mipi_dsi_host, set this register to 1.
		 * 1: power up     0: reset core
		 */
		u32 shutdownz: 1;

		u32 reserved: 31;
		} bits;
	} PWR_UP;

	union _0x08 {
		u32 val;
		struct _CLKMGR_CFG {
		/*
		 * This field indicates the division factor for the TX Escape
		 * clock source (lanebyteclk). The values 0 and 1 stop the
		 * TX_ESC clock generation.
		 */
		u32 tx_esc_clk_division: 8;

		/*
		 * This field indicates the division factor for the Time Out
		 * clock used as the timing unit in the configuration of HS to
		 * LP and LP to HS transition error.
		 */
		u32 to_clk_division: 8;

		u32 reserved: 16;

		} bits;
	} CLKMGR_CFG;

	union _0x0C {
		u32 val;
		struct _DPI_VCID {
		/* This field configures the DPI virtual channel id that
		 * is indexed to the VIDEO mode packets
		 */
		u32 dpi_vcid: 2;

		u32 reserved: 30;

		} bits;
	} DPI_VCID;

	union _0x10 {
		u32 val;
		struct _DPI_COLOR_CODING {
		/*
		 * This field configures the DPI color coding as follows:
		 * 0000: 16-bit configuration 1
		 * 0001: 16-bit configuration 2
		 * 0010: 16-bit configuration 3
		 * 0011: 18-bit configuration 1
		 * 0100: 18-bit configuration 2
		 * 0101: 24-bit
		 * 0110: 20-bit YCbCr 4:2:2 loosely packed
		 * 0111: 24-bit YCbCr 4:2:2
		 * 1000: 16-bit YCbCr 4:2:2
		 * 1001: 30-bit
		 * 1010: 36-bit
		 * 1011: 12-bit YCbCr 4:2:0
		 * 1100: Compression Display Stream
		 * 1101-1111: 12-bit YCbCr 4:2:0
		 */
		u32 dpi_color_coding: 4;

		u32 reserved0: 4;

		/* When set to 1, this bit activates loosely packed
		 * variant to 18-bit configurations
		 */
		u32 loosely18_en: 1;

		u32 reserved1: 23;

		} bits;
	} DPI_COLOR_CODING;

	union _0x14 {
		u32 val;
		struct _DPI_CFG_POL {
		/* When set to 1, this bit configures the data enable
		 * pin (dpidataen) asactive low.
		 */
		u32 dataen_active_low: 1;

		/* When set to 1, this bit configures the vertical
		 * synchronism pin (dpivsync) as active low.
		 */
		u32 vsync_active_low: 1;

		/* When set to 1, this bit configures the horizontal
		 * synchronism pin (dpihsync) as active low.
		 */
		u32 hsync_active_low: 1;

		/* When set to 1, this bit configures the shutdown pin
		 * (dpishutdn) as active low.
		 */
		u32 shutd_active_low: 1;

		/* When set to 1, this bit configures the color mode pin
		 * (dpicolorm) as active low.
		 */
		u32 colorm_active_low: 1;

		u32 reserved: 27;

		} bits;
	} DPI_CFG_POL;

	union _0x18 {
		u32 val;
		struct _DPI_LP_CMD_TIM {
		/*
		 * This field is used for the transmission of commands in
		 * low-power mode. It defines the size, in bytes, of the
		 * largest packet that can fit in a line during the VACT
		 * region.
		 */
		u32 invact_lpcmd_time: 8;

		u32 reserved0: 8;

		/*
		 * This field is used for the transmission of commands in
		 * low-power mode. It defines the size, in bytes, of the
		 * largest packet that can fit in a line during the VSA, VBP,
		 * and VFP regions.
		 */
		u32 outvact_lpcmd_time: 8;

		u32 reserved1: 8;

		} bits;
	} DPI_LP_CMD_TIM;

	union _0x1C {
		u32 val;
		struct _DBI_VCID {
		u32 val;
		} bits;
	} DBI_VCID;

	union _0x20 {
		u32 val;
		struct _DBI_CFG {
		u32 val;
		} bits;
	} DBI_CFG;

	union _0x24 {
		u32 val;
		struct _DBI_PARTITIONING_EN {
		u32 val;
		} bits;
	} DBI_PARTITIONING_EN;

	union _0x28 {
		u32 val;
		struct _DBI_CMDSIZE {
		u32 val;
		} bits;
	} DBI_CMDSIZE;

	union _0x2C {
		u32 val;
		struct _PCKHDL_CFG {
		/* When set to 1, this bit enables the EoTp transmission */
		u32 eotp_tx_en: 1;

		/* When set to 1, this bit enables the EoTp reception. */
		u32 eotp_rx_en: 1;

		/* When set to 1, this bit enables the Bus Turn-Around (BTA)
		 * request.
		 */
		u32 bta_en: 1;

		/* When set to 1, this bit enables the ECC reception, error
		 * correction, and reporting.
		 */
		u32 ecc_rx_en: 1;

		/* When set to 1, this bit enables the CRC reception and error
		 * reporting.
		 */
		u32 crc_rx_en: 1;

		u32 reserved: 27;

		} bits;
	} PCKHDL_CFG;

	union _0x30 {
		u32 val;
		struct _GEN_VCID {
		/* This field indicates the Generic interface read-back
		 * virtual channel identification
		 */
		u32 gen_vcid_rx: 2;

		u32 reserved: 30;

		} bits;
	} GEN_VCID;

	union _0x34 {
		u32 val;
		struct _MODE_CFG {
		/* This bit configures the operation mode
		 * 0: Video mode ;   1: Command mode
		 */
		u32 cmd_video_mode: 1;

		u32 reserved: 31;

		} bits;
	} MODE_CFG;

	union _0x38 {
		u32 val;
		struct _VID_MODE_CFG {
		/*
		 * This field indicates the video mode transmission type as
		 * follows:
		 * 00: Non-burst with sync pulses
		 * 01: Non-burst with sync events
		 * 10 and 11: Burst mode
		 */
		u32 vid_mode_type: 2;

		u32 reserved_0: 6;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VSA period when timing allows.
		 */
		u32 lp_vsa_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VBP period when timing allows.
		 */
		u32 lp_vbp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VFP period when timing allows.
		 */
		u32 lp_vfp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VACT period when timing allows.
		 */
		u32 lp_vact_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the HBP period when timing allows.
		 */
		u32 lp_hbp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the HFP period when timing allows.
		 */
		u32 lp_hfp_en: 1;

		/* When set to 1, this bit enables the request for an ack-
		 * nowledge response at the end of a frame.
		 */
		u32 frame_bta_ack_en: 1;

		/* When set to 1, this bit enables the command transmission
		 * only in low-power mode.
		 */
		u32 lp_cmd_en: 1;

		/* When set to 1, this bit enables the video mode pattern
		 * generator
		 */
		u32 vpg_en: 1;

		u32 reserved_1: 3;

		/* This field is to select the pattern:
		 * 0: Color bar (horizontal or vertical)
		 * 1: BER pattern (vertical only)
		 */
		u32 vpg_mode: 1;

		u32 reserved_2: 3;

		/* This field indicates the color bar orientation as follows:
		 * 0: Vertical mode
		 * 1: Horizontal mode
		 */
		u32 vpg_orientation: 1;

		u32 reserved_3: 7;

		} bits;
	} VID_MODE_CFG;

	union _0x3C {
		u32 val;
		struct _VID_PKT_SIZE {
		/*
		 * This field configures the number of pixels in a single
		 * video packet. For 18-bit not loosely packed data types,
		 * this number must be a multiple of 4. For YCbCr data
		 * types, it must be a multiple of 2, as described in the
		 * DSI specification.
		 */
		u32 vid_pkt_size: 14;

		u32 reserved: 18;

		} bits;
	} VID_PKT_SIZE;

	union _0x40 {
		u32 val;
		struct _VID_NUM_CHUNKS {
		/*
		 * This register configures the number of chunks to be
		 * transmitted during a Line period (a chunk consists of
		 * a video packet and a null packet). If set to 0 or 1,
		 * the video line is transmitted in a single packet. If
		 * set to 1, the packet is part of a chunk, so a null packet
		 * follows it if vid_null_size > 0. Otherwise, multiple chunks
		 * are used to transmit each video line.
		 */
		u32 vid_num_chunks: 13;

		u32 reserved: 19;

		} bits;
	} VID_NUM_CHUNKS;

	union _0x44 {
		u32 val;
		struct _VID_NULL_SIZE {
		/*
		 * This register configures the number of bytes inside a null
		 * packet. Setting it to 0 disables the null packets.
		 */
		u32 vid_null_size: 13;

		u32 reserved: 19;

		} bits;
	} VID_NULL_SIZE;

	union _0x48 {
		u32 val;
		struct _VID_HSA_TIME {
		/* This field configures the Horizontal Synchronism Active
		 * period in lane byte clock cycles
		 */
		u32 vid_hsa_time: 12;

		u32 reserved: 20;

		} bits;
	} VID_HSA_TIME;

	union _0x4C {
		u32 val;
		struct _VID_HBP_TIME {
		/* This field configures the Horizontal Back Porch period
		 * in lane byte clock cycles
		 */
		u32 vid_hbp_time: 12;

		u32 reserved: 20;

		} bits;
	} VID_HBP_TIME;

	union _0x50 {
		u32 val;
		struct _VID_HLINE_TIME {
		/* This field configures the size of the total line time
		 * (HSA+HBP+HACT+HFP) counted in lane byte clock cycles
		 */
		u32 vid_hline_time: 15;

		u32 reserved: 17;

		} bits;
	} VID_HLINE_TIME;

	union _0x54 {
		u32 val;
		struct _VID_VSA_LINES {
		/* This field configures the Vertical Synchronism Active
		 * period measured in number of horizontal lines
		 */
		u32 vsa_lines: 10;

		u32 reserved: 22;

		} bits;
	} VID_VSA_LINES;

	union _0x58 {
		u32 val;
		struct _VID_VBP_LINES {
		/* This field configures the Vertical Back Porch period
		 * measured in number of horizontal lines
		 */
		u32 vbp_lines: 10;

		u32 reserved: 22;

		} bits;
	} VID_VBP_LINES;

	union _0x5C {
		u32 val;
		struct _VID_VFP_LINES {
		/* This field configures the Vertical Front Porch period
		 * measured in number of horizontal lines
		 */
		u32 vfp_lines: 10;

		u32 reserved: 22;

		} bits;
	} VID_VFP_LINES;

	union _0x60 {
		u32 val;
		struct _VID_VACTIVE_LINES {
		/* This field configures the Vertical Active period measured
		 * in number of horizontal lines
		 */
		u32 v_active_lines: 14;

		u32 reserved: 18;

		} bits;
	} VID_VACTIVE_LINES;

	union _0x64 {
		u32 val;
		struct _EDPI_CMD_SIZE {
		/*
		 * This field configures the maximum allowed size for an eDPI
		 * write memory command, measured in pixels. Automatic parti-
		 * tioning of data obtained from eDPI is permanently enabled.
		 */
		u32 edpi_allowed_cmd_size: 16;

		u32 reserved: 16;
		} bits;
	} EDPI_CMD_SIZE;

	union _0x68 {
		u32 val;
		struct _CMD_MODE_CFG {
		/*
		 * When set to 1, this bit enables the tearing effect
		 * acknowledge request.
		 */
		u32 tear_fx_en: 1;

		/*
		 * When set to 1, this bit enables the acknowledge request
		 * after each packet transmission.
		 */
		u32 ack_rqst_en: 1;

		u32 reserved_0: 6;

		/*
		 * This bit configures the Generic short write packet with
		 * zero parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sw_0p_tx: 1;

		/*
		 * This bit configures the Generic short write packet with
		 * one parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sw_1p_tx: 1;

		/*
		 * This bit configures the Generic short write packet with
		 * two parameters command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sw_2p_tx: 1;

		/*
		 * This bit configures the Generic short read packet with
		 * zero parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sr_0p_tx: 1;

		/*
		 * This bit configures the Generic short read packet with
		 * one parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sr_1p_tx: 1;

		/*
		 * This bit configures the Generic short read packet with
		 * two parameters command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_sr_2p_tx: 1;

		/*
		 * This bit configures the Generic long write packet command
		 * transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 gen_lw_tx: 1;

		u32 reserved_1: 1;

		/*
		 * This bit configures the DCS short write packet with zero
		 * parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 dcs_sw_0p_tx: 1;

		/*
		 * This bit configures the DCS short write packet with one
		 * parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 dcs_sw_1p_tx: 1;

		/*
		 * This bit configures the DCS short read packet with zero
		 * parameter command transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 dcs_sr_0p_tx: 1;

		/*
		 * This bit configures the DCS long write packet command
		 * transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 dcs_lw_tx: 1;

		u32 reserved_2: 4;

		/*
		 * This bit configures the maximum read packet size command
		 * transmission type:
		 * 0: High-speed 1: Low-power
		 */
		u32 max_rd_pkt_size: 1;

		u32 reserved_3: 7;

		} bits;
	} CMD_MODE_CFG;

	union _0x6C {
		u32 val;
		struct _GEN_HDR {
		/*
		 * This field configures the packet data type of the header
		 * packet.
		 */
		u32 gen_dt: 6;

		/*
		 * This field configures the virtual channel id of the header
		 * packet.
		 */
		u32 gen_vc: 2;

		/*
		 * This field configures the least significant byte of the
		 * header packet's Word count for long packets or data 0 for
		 * short packets.
		 */
		u32 gen_wc_lsbyte: 8;

		/*
		 * This field configures the most significant byte of the
		 * header packet's word count for long packets or data 1 for
		 * short packets.
		 */
		u32 gen_wc_msbyte: 8;

		u32 reserved: 8;

		} bits;
	} GEN_HDR;

	union _0x70 {
		u32 val;
		struct _GEN_PLD_DATA {
		/* This field indicates byte 1 of the packet payload. */
		u32 gen_pld_b1: 8;

		/* This field indicates byte 2 of the packet payload. */
		u32 gen_pld_b2: 8;

		/* This field indicates byte 3 of the packet payload. */
		u32 gen_pld_b3: 8;

		/* This field indicates byte 4 of the packet payload. */
		u32 gen_pld_b4: 8;

		} bits;
	} GEN_PLD_DATA;

	union _0x74 {
		u32 val;
		struct _CMD_PKT_STATUS {
		/*
		 * This bit indicates the empty status of the generic
		 * command FIFO.
		 * Value after reset: 0x1
		 */
		u32 gen_cmd_cmd_fifo_empty: 1;

		/*
		 * This bit indicates the full status of the generic
		 * command FIFO.
		 * Value after reset: 0x0
		 */
		u32 gen_cmd_cmd_fifo_full: 1;

		/*
		 * This bit indicates the empty status of the generic write
		 * payload FIFO.
		 * Value after reset: 0x1
		 */
		u32 gen_cmd_wdata_fifo_empty: 1;

		/*
		 * This bit indicates the full status of the generic write
		 * payload FIFO.
		 * Value after reset: 0x0
		 */
		u32 gen_cmd_wdata_fifo_full: 1;

		/*
		 * This bit indicates the empty status of the generic read
		 * payload FIFO.
		 * Value after reset: 0x1
		 */
		u32 gen_cmd_rdata_fifo_empty: 1;

		/*
		 * This bit indicates the full status of the generic read
		 * payload FIFO.
		 * Value after reset: 0x0
		 */
		u32 gen_cmd_rdata_fifo_full: 1;

		/*
		 * This bit is set when a read command is issued and cleared
		 * when the entire response is stored in the FIFO.
		 * Value after reset: 0x0
		 */
		u32 gen_cmd_rdcmd_ongoing: 1;

		u32 reserved0 : 1;

		u32 dbi_cmd_empty: 1;
		u32 dbi_cmd_full: 1;
		u32 dbi_pld_w_empty: 1;
		u32 dbi_pld_w_full: 1;
		u32 dbi_pld_r_empty: 1;
		u32 dbi_pld_r_full: 1;
		u32 dbi_rd_cmd_busy: 1;

		u32 reserved : 17;

		} bits;
	} CMD_PKT_STATUS;

	union _0x78 {
		u32 val;
		struct _TO_CNT_CFG {
		/*
		 * This field configures the timeout counter that triggers
		 * a low-power reception timeout contention detection (measured
		 * in TO_CLK_DIVISION cycles).
		 */
		u32 lprx_to_cnt: 16;

		/*
		 * This field configures the timeout counter that triggers
		 * a high speed transmission timeout contention detection
		 * (measured in TO_CLK_DIVISION cycles).
		 *
		 * If using the non-burst mode and there is no sufficient
		 * time to switch from HS to LP and back in the period which
		 * is from one line data finishing to the next line sync
		 * start, the DSI link returns the LP state once per frame,
		 * then you should configure the TO_CLK_DIVISION and
		 * hstx_to_cnt to be in accordance with:
		 * hstx_to_cnt * lanebyteclkperiod * TO_CLK_DIVISION >= the
		 * time of one FRAME data transmission * (1 + 10%)
		 *
		 * In burst mode, RGB pixel packets are time-compressed,
		 * leaving more time during a scan line. Therefore, if in
		 * burst mode and there is sufficient time to switch from HS
		 * to LP and back in the period of time from one line data
		 * finishing to the next line sync start, the DSI link can
		 * return LP mode and back in this time interval to save power.
		 * For this, configure the TO_CLK_DIVISION and hstx_to_cnt
		 * to be in accordance with:
		 * hstx_to_cnt * lanebyteclkperiod * TO_CLK_DIVISION >= the
		 * time of one LINE data transmission * (1 + 10%)
		 */
		u32 hstx_to_cnt: 16;

		} bits;
	} TO_CNT_CFG;

	union _0x7C {
		u32 val;
		struct _HS_RD_TO_CNT {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after sending a high-speed read oper-
		 * ation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		u32 hs_rd_to_cnt: 16;

		u32 reserved: 16;

		} bits;
	} HS_RD_TO_CNT;

	union _0x80 {
		u32 val;
		struct _LP_RD_TO_CNT {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after sending a low-power read oper-
		 * ation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		u32 lp_rd_to_cnt: 16;

		u32 reserved: 16;

		} bits;
	} LP_RD_TO_CNT;

	union _0x84 {
		u32 val;
		struct _HS_WR_TO_CNT {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link inactive after sending a high-speed write
		 * operation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		u32 hs_wr_to_cnt: 16;

		u32 reserved_0: 8;

		/*
		 * When set to 1, this bit ensures that the peripheral response
		 * timeout caused by hs_wr_to_cnt is used only once per eDPI
		 * frame, when both the following conditions are met:
		 * dpivsync_edpiwms has risen and fallen.
		 * Packets originated from eDPI have been transmitted and its
		 * FIFO is empty again In this scenario no non-eDPI requests
		 * are sent to the D-PHY, even if there is traffic from generic
		 * or DBI ready to be sent, making it return to stop state.
		 * When it does so, PRESP_TO counter is activated and only when
		 * it finishes does the controller send any other traffic that
		 * is ready.
		 */
		u32 presp_to_mode: 1;

		u32 reserved_1: 7;

		} bits;
	} HS_WR_TO_CNT;

	union _0x88 {
		u32 val;
		struct _LP_WR_TO_CNT {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after sending a low-power write oper-
		 * ation. This period is measured in cycles of lanebyteclk.
		 * The counting starts when the D-PHY enters the Stop state
		 * and causes no interrupts.
		 */
		u32 lp_wr_to_cnt: 16;

		u32 reserved: 16;

		} bits;
	} LP_WR_TO_CNT;

	union _0x8C {
		u32 val;
		struct _BTA_TO_CNT {
		/*
		 * This field sets a period for which the DWC_mipi_dsi_host
		 * keeps the link still, after completing a Bus Turn-Around.
		 * This period is measured in cycles of lanebyteclk. The
		 * counting starts when the D-PHY enters the Stop state and
		 * causes no interrupts.
		 */
		u32 bta_to_cnt: 16;

		u32 reserved: 16;

		} bits;
	} BTA_TO_CNT;

	union _0x90 {
		u32 val;
		struct _SDF_3D {
		/*
		 * This field defines the 3D mode on/off & display orientation:
		 * 00: 3D mode off (2D mode on)
		 * 01: 3D mode on, portrait orientation
		 * 10: 3D mode on, landscape orientation
		 * 11: Reserved
		 */
		u32 mode_3d: 2;

		/*
		 * This field defines the 3D image format:
		 * 00: Line (alternating lines of left and right data)
		 * 01: Frame (alternating frames of left and right data)
		 * 10: Pixel (alternating pixels of left and right data)
		 * 11: Reserved
		 */
		u32 format_3d: 2;

		/*
		 * This field defines whether there is a second VSYNC pulse
		 * between Left and Right Images, when 3D Image Format is
		 * Frame-based:
		 * 0: No sync pulses between left and right data
		 * 1: Sync pulse (HSYNC, VSYNC, blanking) between left and
		 *    right data
		 */
		u32 second_vsync: 1;

		/*
		 * This bit defines the left or right order:
		 * 0: Left eye data is sent first, and then the right eye data
		 *    is sent.
		 * 1: Right eye data is sent first, and then the left eye data
		 *    is sent.
		 */
		u32 right_first: 1;

		u32 reserved_0: 10;

		/*
		 * When set, causes the next VSS packet to include 3D control
		 * payload in every VSS packet.
		 */
		u32 send_3d_cfg: 1;

		u32 reserved_1: 15;

		} bits;
	} SDF_3D;

	union _0x94 {
		u32 val;
		struct _LPCLK_CTRL {
		/* This bit controls the D-PHY PPI txrequestclkhs signal */
		u32 phy_txrequestclkhs: 1;

		/* This bit enables the automatic mechanism to stop providing
		 * clock in the clock lane when time allows.
		 */
		u32 auto_clklane_ctrl: 1;

		u32 reserved: 30;

		} bits;
	} LPCLK_CTRL;

	union _0x98 {
		u32 val;
		struct _PHY_TMR_LPCLK_CFG {
		/*
		 * This field configures the maximum time that the D-PHY
		 * clock lane takes to go from low-power to high-speed
		 * transmission measured in lane byte clock cycles.
		 */
		u32 phy_clklp2hs_time: 10;

		u32 reserved0: 6;

		/*
		 * This field configures the maximum time that the D-PHY
		 * clock lane takes to go from high-speed to low-power
		 * transmission measured in lane byte clock cycles.
		 */
		u32 phy_clkhs2lp_time: 10;

		u32 reserved1: 6;

		} bits;
	} PHY_TMR_LPCLK_CFG;

	union _0x9C {
		u32 val;
		struct _PHY_TMR_CFG {
		/*
		 * This field configures the maximum time that the D-PHY data
		 * lanes take to go from low-power to high-speed transmission
		 * measured in lane byte clock cycles.
		 */
		u32 phy_lp2hs_time: 10;

		u32 reserved0: 6;

		/*
		 * This field configures the maximum time that the D-PHY data
		 * lanes take to go from high-speed to low-power transmission
		 * measured in lane byte clock cycles.
		 */
		u32 phy_hs2lp_time: 10;

		u32 reserved1: 6;

		} bits;
	} PHY_TMR_CFG;

	union _0xA0 {
		u32 val;
		struct _PHY_RSTZ {
		/* When set to 0, this bit places the D-PHY macro in power-
		 * down state.
		 */
		u32 phy_shutdownz: 1;

		/* When set to 0, this bit places the digital section of the
		 * D-PHY in the reset state.
		 */
		u32 phy_rstz: 1;

		/* When set to 1, this bit enables the D-PHY Clock Lane
		 * module.
		 */
		u32 phy_enableclk: 1;

		/* When the D-PHY is in ULPS, this bit enables the D-PHY PLL. */
		u32 phy_forcepll: 1;

		u32 reserved: 28;

		} bits;
	} PHY_RSTZ;

	union _0xA4 {
		u32 val;
		struct _PHY_IF_CFG {
		/*
		 * This field configures the number of active data lanes:
		 * 00: One data lane (lane 0)
		 * 01: Two data lanes (lanes 0 and 1)
		 * 10: Three data lanes (lanes 0, 1, and 2)
		 * 11: Four data lanes (lanes 0, 1, 2, and 3)
		 */
		u32 n_lanes: 2;

		u32 reserved0: 6;

		/* This field configures the minimum wait period to request
		 * a high-speed transmission after the Stop state.
		 */
		u32 phy_stop_wait_time: 8;

		u32 reserved1: 16;

		} bits;
	} PHY_IF_CFG;

	union _0xA8 {
		u32 val;
		struct _PHY_ULPS_CTRL {
		/* ULPS mode Request on clock lane */
		u32 phy_txrequlpsclk: 1;

		/* ULPS mode Exit on clock lane */
		u32 phy_txexitulpsclk: 1;

		/* ULPS mode Request on all active data lanes */
		u32 phy_txrequlpslan: 1;

		/* ULPS mode Exit on all active data lanes */
		u32 phy_txexitulpslan: 1;

		u32 reserved: 28;
		} bits;
	} PHY_ULPS_CTRL;

	union _0xAC {
		u32 val;
		struct _PHY_TX_TRIGGERS {
		/* This field controls the trigger transmissions. */
		u32 phy_tx_triggers: 4;

		u32 reserved: 28;
		} bits;
	} PHY_TX_TRIGGERS;

	union _0xB0 {
		u32 val;
		struct _PHY_STATUS {
		/* the status of phylock D-PHY signal */
		u32 phy_lock: 1;

		/* the status of phydirection D-PHY signal */
		u32 phy_direction: 1;

		/* the status of phystopstateclklane D-PHY signal */
		u32 phy_stopstateclklane: 1;

		/* the status of phyulpsactivenotclk D-PHY signal */
		u32 phy_ulpsactivenotclk: 1;

		/* the status of phystopstate0lane D-PHY signal */
		u32 phy_stopstate0lane: 1;

		/* the status of ulpsactivenot0lane D-PHY signal */
		u32 phy_ulpsactivenot0lane: 1;

		/* the status of rxulpsesc0lane D-PHY signal */
		u32 phy_rxulpsesc0lane: 1;

		/* the status of phystopstate1lane D-PHY signal */
		u32 phy_stopstate1lane: 1;

		/* the status of ulpsactivenot1lane D-PHY signal */
		u32 phy_ulpsactivenot1lane: 1;

		/* the status of phystopstate2lane D-PHY signal */
		u32 phy_stopstate2lane: 1;

		/* the status of ulpsactivenot2lane D-PHY signal */
		u32 phy_ulpsactivenot2lane: 1;

		/* the status of phystopstate3lane D-PHY signal */
		u32 phy_stopstate3lane: 1;

		/* the status of ulpsactivenot3lane D-PHY signal */
		u32 phy_ulpsactivenot3lane: 1;

		u32 reserved: 19;

		} bits;
	} PHY_STATUS;

	union _0xB4 {
		u32 val;
		struct _PHY_TST_CTRL0 {
		/* PHY test interface clear (active high) */
		u32 phy_testclr: 1;

		/* This bit is used to clock the TESTDIN bus into the D-PHY */
		u32 phy_testclk: 1;

		u32 reserved: 30;
		} bits;
	} PHY_TST_CTRL0;

	union _0xB8 {
		u32 val;
		struct _PHY_TST_CTRL1 {
		/* PHY test interface input 8-bit data bus for internal
		 * register programming and test functionalities access.
		 */
		u32 phy_testdin: 8;

		/* PHY output 8-bit data bus for read-back and internal
		 * probing functionalities.
		 */
		u32 phy_testdout: 8;

		/*
		 * PHY test interface operation selector:
		 * 1: The address write operation is set on the falling edge
		 *    of the testclk signal.
		 * 0: The data write operation is set on the rising edge of
		 *    the testclk signal.
		 */
		u32 phy_testen: 1;

		u32 reserved: 15;
		} bits;
	} PHY_TST_CTRL1;

	union _0xBC {
		u32 val;
		struct _INT_STS0 {
		/* SoT error from the Acknowledge error report */
		u32 ack_with_err_0: 1;

		/* SoT Sync error from the Acknowledge error report */
		u32 ack_with_err_1: 1;

		/* EoT Sync error from the Acknowledge error report */
		u32 ack_with_err_2: 1;

		/* Escape Mode Entry Command error from the Acknowledge
		 * error report
		 */
		u32 ack_with_err_3: 1;

		/* LP Transmit Sync error from the Acknowledge error report */
		u32 ack_with_err_4: 1;

		/* Peripheral Timeout error from the Acknowledge error report */
		u32 ack_with_err_5: 1;

		/* False Control error from the Acknowledge error report */
		u32 ack_with_err_6: 1;

		/* reserved (specific to device) from the Acknowledge error
		 * report
		 */
		u32 ack_with_err_7: 1;

		/* ECC error, single-bit (detected and corrected) from the
		 * Acknowledge error report
		 */
		u32 ack_with_err_8: 1;

		/* ECC error, multi-bit (detected, not corrected) from the
		 * Acknowledge error report
		 */
		u32 ack_with_err_9: 1;

		/* checksum error (long packet only) from the Acknowledge
		 * error report
		 */
		u32 ack_with_err_10: 1;

		/* not recognized DSI data type from the Acknowledge error
		 * report
		 */
		u32 ack_with_err_11: 1;

		/* DSI VC ID Invalid from the Acknowledge error report */
		u32 ack_with_err_12: 1;

		/* invalid transmission length from the Acknowledge error
		 * report
		 */
		u32 ack_with_err_13: 1;

		/* reserved (specific to device) from the Acknowledge error
		 * report
		 */
		u32 ack_with_err_14: 1;

		/* DSI protocol violation from the Acknowledge error report */
		u32 ack_with_err_15: 1;

		/* ErrEsc escape entry error from Lane 0 */
		u32 dphy_errors_0: 1;

		/* ErrSyncEsc low-power data transmission synchronization
		 * error from Lane 0
		 */
		u32 dphy_errors_1: 1;

		/* ErrControl error from Lane 0 */
		u32 dphy_errors_2: 1;

		/* ErrContentionLP0 LP0 contention error from Lane 0 */
		u32 dphy_errors_3: 1;

		/* ErrContentionLP1 LP1 contention error from Lane 0 */
		u32 dphy_errors_4: 1;

		u32 reserved: 11;

		} bits;
	} INT_STS0;

	union _0xC0 {
		u32 val;
		struct _INT_STS1 {
		/* This bit indicates that the high-speed transmission timeout
		 * counter reached the end and contention is detected.
		 */
		u32 to_hs_tx: 1;

		/* This bit indicates that the low-power reception timeout
		 * counter reached the end and contention is detected.
		 */
		u32 to_lp_rx: 1;

		/* This bit indicates that the ECC single error is detected
		 * and corrected in a received packet.
		 */
		u32 ecc_single_err: 1;

		/* This bit indicates that the ECC multiple error is detected
		 * in a received packet.
		 */
		u32 ecc_multi_err: 1;

		/* This bit indicates that the CRC error is detected in the
		 * received packet payload.
		 */
		u32 crc_err: 1;

		/* This bit indicates that the packet size error is detected
		 * during the packet reception.
		 */
		u32 pkt_size_err: 1;

		/* This bit indicates that the EoTp packet is not received at
		 * the end of the incoming peripheral transmission
		 */
		u32 eotp_err: 1;

		/* This bit indicates that during a DPI pixel line storage,
		 * the payload FIFO becomes full and the data stored is
		 * corrupted.
		 */
		u32 dpi_pld_wr_err: 1;

		/* This bit indicates that the system tried to write a command
		 * through the Generic interface and the FIFO is full. There-
		 * fore, the command is not written.
		 */
		u32 gen_cmd_wr_err: 1;

		/* This bit indicates that the system tried to write a payload
		 * data through the Generic interface and the FIFO is full.
		 * Therefore, the payload is not written.
		 */
		u32 gen_pld_wr_err: 1;

		/* This bit indicates that during a Generic interface packet
		 * build, the payload FIFO becomes empty and corrupt data is
		 * sent.
		 */
		u32 gen_pld_send_err: 1;

		/* This bit indicates that during a DCS read data, the payload
		 * FIFO becomes	empty and the data sent to the interface is
		 * corrupted.
		 */
		u32 gen_pld_rd_err: 1;

		/* This bit indicates that during a generic interface packet
		 * read back, the payload FIFO becomes full and the received
		 * data is corrupted.
		 */
		u32 gen_pld_recev_err: 1;

		u32 dbi_cmd_wr_err: 1;
		u32 dbi_pld_wr_err: 1;
		u32 dbi_pld_rd_err: 1;
		u32 dbi_pld_recv_err: 1;
		u32 dbi_ilegal_comm_err: 1;

		u32 reserved: 14;

		} bits;
	} INT_STS1;

	union _0xC4 {
		u32 val;
		struct _INT_MASK0 {
		u32 mask_ack_with_err_0: 1;
		u32 mask_ack_with_err_1: 1;
		u32 mask_ack_with_err_2: 1;
		u32 mask_ack_with_err_3: 1;
		u32 mask_ack_with_err_4: 1;
		u32 mask_ack_with_err_5: 1;
		u32 mask_ack_with_err_6: 1;
		u32 mask_ack_with_err_7: 1;
		u32 mask_ack_with_err_8: 1;
		u32 mask_ack_with_err_9: 1;
		u32 mask_ack_with_err_10: 1;
		u32 mask_ack_with_err_11: 1;
		u32 mask_ack_with_err_12: 1;
		u32 mask_ack_with_err_13: 1;
		u32 mask_ack_with_err_14: 1;
		u32 mask_ack_with_err_15: 1;
		u32 mask_dphy_errors_0: 1;
		u32 mask_dphy_errors_1: 1;
		u32 mask_dphy_errors_2: 1;
		u32 mask_dphy_errors_3: 1;
		u32 mask_dphy_errors_4: 1;
		u32 reserved: 11;
		} bits;
	} INT_MASK0;

	union _0xC8 {
		u32 val;
		struct _INT_MASK1 {
		u32 mask_to_hs_tx: 1;
		u32 mask_to_lp_rx: 1;
		u32 mask_ecc_single_err: 1;
		u32 mask_ecc_multi_err: 1;
		u32 mask_crc_err: 1;
		u32 mask_pkt_size_err: 1;
		u32 mask_eotp_err: 1;
		u32 mask_dpi_pld_wr_err: 1;
		u32 mask_gen_cmd_wr_err: 1;
		u32 mask_gen_pld_wr_err: 1;
		u32 mask_gen_pld_send_err: 1;
		u32 mask_gen_pld_rd_err: 1;
		u32 mask_gen_pld_recev_err: 1;
		u32 mask_dbi_cmd_wr_err: 1;
		u32 mask_dbi_pld_wr_err: 1;
		u32 mask_dbi_pld_rd_err: 1;
		u32 mask_dbi_pld_recv_err: 1;
		u32 mask_dbi_ilegal_comm_err: 1;
		u32 reserved: 14;
		} bits;
	} INT_MASK1;

	union _0xCC {
		u32 val;
		struct _PHY_CAL {

		u32 txskewcalhs: 1;

		u32 reserved: 31;

		} bits;
	} PHY_CAL;

	u32 reservedD0_D4[2];

	union _0xD8 {
		u32 val;
		struct _INT_FORCE0 {
		u32 force_ack_with_err_0: 1;
		u32 force_ack_with_err_1: 1;
		u32 force_ack_with_err_2: 1;
		u32 force_ack_with_err_3: 1;
		u32 force_ack_with_err_4: 1;
		u32 force_ack_with_err_5: 1;
		u32 force_ack_with_err_6: 1;
		u32 force_ack_with_err_7: 1;
		u32 force_ack_with_err_8: 1;
		u32 force_ack_with_err_9: 1;
		u32 force_ack_with_err_10: 1;
		u32 force_ack_with_err_11: 1;
		u32 force_ack_with_err_12: 1;
		u32 force_ack_with_err_13: 1;
		u32 force_ack_with_err_14: 1;
		u32 force_ack_with_err_15: 1;
		u32 force_dphy_errors_0: 1;
		u32 force_dphy_errors_1: 1;
		u32 force_dphy_errors_2: 1;
		u32 force_dphy_errors_3: 1;
		u32 force_dphy_errors_4: 1;
		u32 reserved: 11;
		} bits;
	} INT_FORCE0;

	union _0xDC {
		u32 val;
		struct _INT_FORCE1 {
		u32 force_to_hs_tx: 1;
		u32 force_to_lp_rx: 1;
		u32 force_ecc_single_err: 1;
		u32 force_ecc_multi_err: 1;
		u32 force_crc_err: 1;
		u32 force_pkt_size_err: 1;
		u32 force_eotp_err: 1;
		u32 force_dpi_pld_wr_err: 1;
		u32 force_gen_cmd_wr_err: 1;
		u32 force_gen_pld_wr_err: 1;
		u32 force_gen_pld_send_err: 1;
		u32 force_gen_pld_rd_err: 1;
		u32 force_gen_pld_recev_err: 1;
		u32 force_dbi_cmd_wr_err: 1;
		u32 force_dbi_pld_wr_err: 1;
		u32 force_dbi_pld_rd_err: 1;
		u32 force_dbi_pld_recv_err: 1;
		u32 force_dbi_ilegal_comm_err: 1;
		u32 reserved: 14;
		} bits;
	} INT_FORCE1;

	u32 reservedE0_EC[4];

	union _0xF0 {
		u32 val;
		struct _DSC_PARAMETER {
		/* When set to 1, this bit enables the compression mode. */
		u32 compression_mode: 1;

		u32 reserved0: 7;

		/* This field indicates the algorithm identifier:
		 * 00 = VESA DSC Standard 1.1
		 * 11 = vendor-specific algorithm
		 * 01 and 10 = reserved, not used
		 */
		u32 compress_algo: 2;

		u32 reserved1: 6;

		/* This field indicates the PPS selector:
		 * 00 = PPS Table 1
		 * 01 = PPS Table 2
		 * 10 = PPS Table 3
		 * 11 = PPS Table 4
		 */
		u32 pps_sel: 2;

		u32 reserved2: 14;

		} bits;
	} DSC_PARAMETER;

	union _0xF4 {
		u32 val;
		struct _PHY_TMR_RD_CFG {
		/*
		 * This field configures the maximum time required to perform
		 * a read command in lane byte clock cycles. This register can
		 * only be modified when no read command is in progress.
		 */
		u32 max_rd_time: 15;

		u32 reserved: 17;

		} bits;
	} PHY_TMR_RD_CFG;

	u32 reservedF8_FC[2];

	union _0x100 {
		u32 val;
		struct _VID_SHADOW_CTRL {
		/*
		 * When set to 1, DPI receives the active configuration
		 * from the auxiliary registers. When the feature is set
		 * at the same time than vid_shadow_req the auxiliary
		 * registers are automatically updated.
		 */
		u32 vid_shadow_en: 1;

		u32 reserved0: 7;

		/*
		 * When set to 1, this bit request that the dpi registers
		 * from regbank are copied to the auxiliary registers. When
		 * the request is completed this bit is a auto clear.
		 */
		u32 vid_shadow_req: 1;

		u32 reserved1: 7;

		/*
		 * When set to 1, the video request is done by external pin.
		 * In this mode vid_shadow_req is ignored.
		 */
		u32 vid_shadow_pin_req: 1;

		u32 reserved2: 15;

		} bits;
	} VID_SHADOW_CTRL;

	u32 reserved104_108[2];

	union _0x10C {
		u32 val;
		struct _DPI_VCID_ACT {
		/* This field configures the DPI virtual channel id that
		 * is indexed to the VIDEO mode packets
		 */
		u32 dpi_vcid: 2;

		u32 reserved: 30;

		} bits;
	} DPI_VCID_ACT;

	union _0x110 {
		u32 val;
		struct _DPI_COLOR_CODING_ACT {
		/*
		 * This field configures the DPI color coding as follows:
		 * 0000: 16-bit configuration 1
		 * 0001: 16-bit configuration 2
		 * 0010: 16-bit configuration 3
		 * 0011: 18-bit configuration 1
		 * 0100: 18-bit configuration 2
		 * 0101: 24-bit
		 * 0110: 20-bit YCbCr 4:2:2 loosely packed
		 * 0111: 24-bit YCbCr 4:2:2
		 * 1000: 16-bit YCbCr 4:2:2
		 * 1001: 30-bit
		 * 1010: 36-bit
		 * 1011: 12-bit YCbCr 4:2:0
		 * 1100: Compression Display Stream
		 * 1101-1111: 12-bit YCbCr 4:2:0
		 */
		u32 dpi_color_coding: 4;

		u32 reserved0: 4;

		/* When set to 1, this bit activates loosely packed
		 * variant to 18-bit configurations
		 */
		u32 loosely18_en: 1;

		u32 reserved1: 23;

		} bits;
	} DPI_COLOR_CODING_ACT;

	u32 reserved144;

	union _0x118 {
		u32 val;
		struct _DPI_LP_CMD_TIM_ACT {
		/*
		 * This field is used for the transmission of commands in
		 * low-power mode. It defines the size, in bytes, of the
		 * largest packet that can fit in a line during the VACT
		 * region.
		 */
		u32 invact_lpcmd_time: 8;

		u32 reserved0: 8;

		/*
		 * This field is used for the transmission of commands in
		 * low-power mode. It defines the size, in bytes, of the
		 * largest packet that can fit in a line during the VSA, VBP,
		 * and VFP regions.
		 */
		u32 outvact_lpcmd_time: 8;

		u32 reserved1: 8;

		} bits;
	} DPI_LP_CMD_TIM_ACT;

	u32 reserved11C_134[7];

	union _0x138 {
		u32 val;
		struct _VID_MODE_CFG_ACT {
		/*
		 * This field indicates the video mode transmission type as
		 * follows:
		 * 00: Non-burst with sync pulses
		 * 01: Non-burst with sync events
		 * 10 and 11: Burst mode
		 */
		u32 vid_mode_type: 2;

		u32 reserved_0: 6;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VSA period when timing allows.
		 */
		u32 lp_vsa_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VBP period when timing allows.
		 */
		u32 lp_vbp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VFP period when timing allows.
		 */
		u32 lp_vfp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the VACT period when timing allows.
		 */
		u32 lp_vact_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the HBP period when timing allows.
		 */
		u32 lp_hbp_en: 1;

		/* When set to 1, this bit enables the return to low-power
		 * inside the HFP period when timing allows.
		 */
		u32 lp_hfp_en: 1;

		/* When set to 1, this bit enables the request for an ack-
		 * nowledge response at the end of a frame.
		 */
		u32 frame_bta_ack_en: 1;

		/* When set to 1, this bit enables the command transmission
		 * only in low-power mode.
		 */
		u32 lp_cmd_en: 1;

		/* When set to 1, this bit enables the video mode pattern
		 * generator
		 */
		u32 vpg_en: 1;

		u32 reserved_1: 3;

		/* This field is to select the pattern:
		 * 0: Color bar (horizontal or vertical)
		 * 1: BER pattern (vertical only)
		 */
		u32 vpg_mode: 1;

		u32 reserved_2: 3;

		/* This field indicates the color bar orientation as follows:
		 * 0: Vertical mode
		 * 1: Horizontal mode
		 */
		u32 vpg_orientation: 1;

		u32 reserved_3: 7;

		} bits;
	} VID_MODE_CFG_ACT;

	union _0x13C {
		u32 val;
		struct _VID_PKT_SIZE_ACT {
		/*
		 * This field configures the number of pixels in a single
		 * video packet. For 18-bit not loosely packed data types,
		 * this number must be a multiple of 4. For YCbCr data
		 * types, it must be a multiple of 2, as described in the
		 * DSI specification.
		 */
		u32 vid_pkt_size: 14;

		u32 reserved: 18;

		} bits;
	} VID_PKT_SIZE_ACT;

	union _0x140 {
		u32 val;
		struct _VID_NUM_CHUNKS_ACT {
		/*
		 * This register configures the number of chunks to be
		 * transmitted during a Line period (a chunk consists of
		 * a video packet and a null packet). If set to 0 or 1,
		 * the video line is transmitted in a single packet. If
		 * set to 1, the packet is part of a chunk, so a null packet
		 * follows it if vid_null_size > 0. Otherwise, multiple chunks
		 * are used to transmit each video line.
		 */
		u32 vid_num_chunks: 13;

		u32 reserved: 19;

		} bits;
	} VID_NUM_CHUNKS_ACT;

	union _0x144 {
		u32 val;
		struct _VID_NULL_SIZE_ACT {
		/*
		 * This register configures the number of bytes inside a null
		 * packet. Setting it to 0 disables the null packets.
		 */
		u32 vid_null_size: 13;

		u32 reserved: 19;

		} bits;
	} VID_NULL_SIZE_ACT;

	union _0x148 {
		u32 val;
		struct _VID_HSA_TIME_ACT {
		/* This field configures the Horizontal Synchronism Active
		 * period in lane byte clock cycles
		 */
		u32 vid_hsa_time: 12;

		u32 reserved: 20;

		} bits;
	} VID_HSA_TIME_ACT;

	union _0x14C {
		u32 val;
		struct _VID_HBP_TIME_ACT {
		/* This field configures the Horizontal Back Porch period
		 * in lane byte clock cycles
		 */
		u32 vid_hbp_time: 12;

		u32 reserved: 20;

		} bits;
	} VID_HBP_TIME_ACT;

	union _0x150 {
		u32 val;
		struct _VID_HLINE_TIME_ACT {
		/* This field configures the size of the total line time
		 * (HSA+HBP+HACT+HFP) counted in lane byte clock cycles
		 */
		u32 vid_hline_time: 15;

		u32 reserved: 17;

		} bits;
	} VID_HLINE_TIME_ACT;

	union _0x154 {
		u32 val;
		struct _VID_VSA_LINES_ACT {
		/* This field configures the Vertical Synchronism Active
		 * period measured in number of horizontal lines
		 */
		u32 vsa_lines: 10;

		u32 reserved: 22;

		} bits;
	} VID_VSA_LINES_ACT;

	union _0x158 {
		u32 val;
		struct _VID_VBP_LINES_ACT {
		/* This field configures the Vertical Back Porch period
		 * measured in number of horizontal lines
		 */
		u32 vbp_lines: 10;

		u32 reserved: 22;

		} bits;
	} VID_VBP_LINES_ACT;

	union _0x15C {
		u32 val;
		struct _VID_VFP_LINES_ACT {
		/* This field configures the Vertical Front Porch period
		 * measured in number of horizontal lines
		 */
		u32 vfp_lines: 10;

		u32 reserved: 22;

		} bits;
	} VID_VFP_LINES_ACT;

	union _0x160 {
		u32 val;
		struct _VID_VACTIVE_LINES_ACT {
		/* This field configures the Vertical Active period measured
		 * in number of horizontal lines
		 */
		u32 v_active_lines: 14;

		u32 reserved: 18;

		} bits;
	} VID_VACTIVE_LINES_ACT;

	u32 reserved164_18C[11];

	union _0x190 {
		u32 val;
		struct _SDF_3D_ACT {
		/*
		 * This field defines the 3D mode on/off & display orientation:
		 * 00: 3D mode off (2D mode on)
		 * 01: 3D mode on, portrait orientation
		 * 10: 3D mode on, landscape orientation
		 * 11: Reserved
		 */
		u32 mode_3d: 2;

		/*
		 * This field defines the 3D image format:
		 * 00: Line (alternating lines of left and right data)
		 * 01: Frame (alternating frames of left and right data)
		 * 10: Pixel (alternating pixels of left and right data)
		 * 11: Reserved
		 */
		u32 format_3d: 2;

		/*
		 * This field defines whether there is a second VSYNC pulse
		 * between Left and Right Images, when 3D Image Format is
		 * Frame-based:
		 * 0: No sync pulses between left and right data
		 * 1: Sync pulse (HSYNC, VSYNC, blanking) between left and
		 *    right data
		 */
		u32 second_vsync: 1;

		/*
		 * This bit defines the left or right order:
		 * 0: Left eye data is sent first, and then the right eye data
		 *    is sent.
		 * 1: Right eye data is sent first, and then the left eye data
		 *    is sent.
		 */
		u32 right_first: 1;

		u32 reserved_0: 10;

		/*
		 * When set, causes the next VSS packet to include 3D control
		 * payload in every VSS packet.
		 */
		u32 send_3d_cfg: 1;

		u32 reserved_1: 15;

		} bits;
	} SDF_3D_ACT;

};

#endif
