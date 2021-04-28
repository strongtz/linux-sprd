/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#ifndef _ISP_DRV_KERNEL_H_
#define _ISP_DRV_KERNEL_H_

#define BUF_ALIGN(X) (((((X) + 512) + 1024 - 1) >> 10) << 10)

#define ISP_AEM_STATIS_BUF_SIZE		BUF_ALIGN(0x2000)
#define ISP_AEM_STATIS_BUF_NUM		4
#define ISP_AFM_STATIS_BUF_SIZE		BUF_ALIGN(0x78)
#define ISP_AFM_STATIS_BUF_NUM		4
#define ISP_AFL_STATIS_BUF_SIZE		BUF_ALIGN(0x25260)
#define ISP_AFL_STATIS_BUF_NUM		4
#define ISP_PDAF_STATIS_BUF_SIZE	BUF_ALIGN(0x43600)
#define ISP_PDAF_STATIS_BUF_NUM		4
#define ISP_BINNING_STATIS_BUF_SIZE	BUF_ALIGN(0x64000)
#define ISP_BINNING_STATIS_BUF_NUM	4


#define ISP_PINGPANG_FRGB_GAMC_NODE		   129
#define ISP_PINGPANG_FRGB_GAMC_NUM		   257
#define NR3_BUF_NUM               8
#define POSTERIZE_NUM             8
#define ISP_PINGPANG_YUV_YGAMMA_NUM		   129
#define ISP_CCE_MATRIX_TAB_MAX             9
#define ISP_CCE_UVD_NUM                    7
#define ISP_CCE_UVC0_NUM                   2
#define ISP_CCE_UVC1_NUM                   3
#define ISP_AWBM_ITEM                      1024
#define ISP_RAW_AEM_ITEM                   1024
#define ISP_AFM_WIN_NUM			10
#define ISP_VST_IVST_NUM 1024
#define PDAF_PPI_NUM 64
#define PDAF_CORRECT_GAIN_NUM    128
#define v_counter_interval 524288

enum isp_img_output_id {
	ISP_IMG_PREVIEW = 0,
	ISP_IMG_VIDEO,
	ISP_IMG_CAPTURE,
	ISP_IMG_STATISTICS,
	ISP_OUTPUT_IMG_TOTAL,
};

enum isp_rtn {
	ISP_IMG_TX_DONE = 0x20,
	ISP_IMG_NO_MEM,
	ISP_IMG_TX_ERR,
	ISP_IMG_SYS_BUSY,
	ISP_IMG_TIMEOUT,
	ISP_IMG_TX_STOP,
};

enum isp_irq_type {
	ISP_IRQ_AEM_DONE = 0x20,
	ISP_IRQ_STATIS,
	ISP_IRQ_IMG,
	ISP_IRQ_CFG_BUF,
};

enum isp_block {
	ISP_BLOCK_1D_LSC,
	ISP_BLOCK_2D_LSC,
	ISP_BLOCK_ANTI_FLICKER,
	ISP_BLOCK_ANTI_FLICKER_NEW,
	ISP_BLOCK_ARBITER,
	ISP_BLOCK_BRIGHTNESS,
	ISP_BLOCK_BLC,
	ISP_BLOCK_BPC,
	ISP_BLOCK_CCE,
	ISP_BLOCK_YUV_CDN,
	ISP_BLOCK_CFA,
	ISP_BLOCK_CMC,
	ISP_BLOCK_COMMON,
	ISP_BLOCK_CONTRAST,
	ISP_BLOCK_CSA,
	ISP_BLOCK_DISPATCH,
	ISP_BLOCK_DISPATCH_YUV,
	ISP_BLOCK_EDGE,
	ISP_BLOCK_FETCH,
	ISP_BLOCK_YUV_FETCH,
	ISP_BLOCK_GRGB,
	ISP_BLOCK_HIST,
	ISP_BLOCK_HIST2,
	ISP_BLOCK_HSV,
	ISP_BLOCK_HUE,
	ISP_BLOCK_IIRCNR,
	ISP_BLOCK_NLC,
	ISP_BLOCK_NLM,
	ISP_BLOCK_NOISE_FILTER,
	ISP_BLOCK_PDAF,
	ISP_BLOCK_PDAF_CORRECT,
	ISP_BLOCK_PGG,
	ISP_BLOCK_POST_BLC,
	ISP_BLOCK_POST_CDN,
	ISP_BLOCK_PRE_CDN,
	ISP_BLOCK_PSTRZ,
	ISP_BLOCK_RLSC,
	ISP_BLOCK_RGB2Y,
	ISP_BLOCK_RGBG,
	ISP_BLOCK_RGBG_DITHER,
	ISP_BLOCK_STORE,
	ISP_BLOCK_STOREA,
	ISP_BLOCK_STORE1,
	ISP_BLOCK_STORE2,
	ISP_BLOCK_STORE3,
	ISP_BLOCK_STORE4,
	ISP_BLOCK_UVD,
	ISP_BLOCK_YDELAY,
	ISP_BLOCK_GAMMA,
	ISP_BLOCK_YGAMMA,
	ISP_BLOCK_YNR,
	ISP_BLOCK_AWBC,
	ISP_BLOCK_BINNING,
	ISP_BLOCK_RAW_AEM,
	ISP_BLOCK_RAW_AFM,
	ISP_BLOCK_3DNR,
};

enum isp_blc_property {
	ISP_PRO_BLC_BLOCK,
};

enum isp_2d_lsc_property {
	ISP_PRO_2D_LSC_BLOCK,
	ISP_PRO_2D_LSC_SLICE_SIZE,
	ISP_PRO_2D_LSC_TRANSADDR,
};

enum isp_1d_lsc_property {
	ISP_PRO_1D_LSC_BLOCK,
	ISP_PRO_1D_LSC_SLICE_SIZE,
	ISP_PRO_1D_LSC_POS,
};

enum isp_awb_property {
	ISP_PRO_AWB_BLOCK,
	ISP_PRO_AWBC_GAIN,
};

enum isp_bpc_property {
	ISP_PRO_BPC_BLOCK,
};

enum isp_anti_flicker_property {
	ISP_PRO_ANTI_FLICKER_BLOCK,
	ISP_PRO_ANTI_FLICKER_BYPASS,
};

enum isp_anti_flicker_new_property {
	ISP_PRO_ANTI_FLICKER_NEW_BLOCK,
	ISP_PRO_ANTI_FLICKER_NEW_BYPASS,
};

enum isp_arbiter_property {
	ISP_PRO_ARBITER_BLOCK,
};

enum isp_binning_property {
	ISP_PRO_BINNING_BLOCK,
};

enum isp_brightness_property {
	ISP_PRO_BRIGHT_BLOCK,
};

enum isp_cce_property {
	ISP_PRO_CCE_BLOCK_MATRIX,
};

enum isp_cdn_property {
	ISP_PRO_YUV_CDN_BLOCK,
};

enum isp_cfa_property {
	ISP_PRO_CFA_BLOCK,
};

enum isp_cmc_property {
	ISP_PRO_CMC_BLOCK,
};

enum isp_common_property {
	ISP_PRO_COMMON_BLOCK,
	ISP_PRO_COMMON_COMM_SHADOW,
	ISP_PRO_COMMON_3A_SINGLE_FRAME_CTRL,
};

enum isp_contrast_property {
	ISP_PRO_CONTRAST_BLOCK,
};

enum isp_csa_property {
	ISP_PRO_CSA_BLOCK,
};


/*isp sub block: dispatch*/
enum isp_dispatch_property {
	ISP_PRO_DISPATCH_BLOCK,
	ISP_PRO_DISPATCH_CH0_SIZE,
};

/*isp sub block: dispatch_yuv*/
enum isp_dispatch_yuv_property {
	ISP_PRO_DISPATCH_YUV_BLOCK,
};

/*isp sub block: dispatch_fetch*/
enum isp_fetch_property {
	ISP_PRO_FETCH_RAW_BLOCK,
	ISP_PRO_FETCH_START,
	ISP_PRO_FETCH_SLICE_SIZE,
	ISP_PRO_FETCH_TRANSADDR,
};

/*isp sub block: edge*/
enum isp_edge_property {
	ISP_PRO_EDGE_BLOCK,
};

/*isp sub block: gamma*/
enum isp_gamma_property {
	ISP_PRO_GAMMA_BLOCK,
};

/*isp sub block: grgb*/
enum isp_grgb_property {
	ISP_PRO_GRGB_BLOCK,
};

/*isp sub block: hist*/
enum isp_hist_property {
	ISP_PRO_HIST_BLOCK,
};

/*isp sub block: hist2*/
enum isp_hist2_property {
	ISP_PRO_HIST2_BLOCK,
};

/*isp sub block: hsv*/
enum isp_hsv_property {
	ISP_PRO_HSV_BLOCK,
};

/*isp sub block: hue*/
enum isp_hua_property {
	ISP_PRO_HUE_BLOCK,
};

/*isp sub block: iircnr*/
enum isp_iircnr_property {
	ISP_PRO_IIRCNR_BLOCK,
	ISP_PRO_YRANDOM_BLOCK,
};

/*isp sub block: nlc*/
enum isp_nlc_property {
	ISP_PRO_NLC_BLOCK,
};

/*isp sub block: nlm*/
enum isp_nlm_property {
	ISP_PRO_NLM_BLOCK,
};

/*isp sub block: noise_filter*/
enum isp_noise_filter_property {
	ISP_PRO_NOISE_FILTER_BLOCK,
};

/*isp sub block: pgg*/
enum isp_pre_glb_gain_property {
	ISP_PRO_PRE_GLB_GAIN_BLOCK,
};

/*isp sub block: ynr*/
enum isp_ynr_property {
	ISP_PRO_YNR_BLOCK,
};

/*isp sub block: pdaf*/
enum isp_pdaf_property {
	ISP_PRO_PDAF_BLOCK,
	ISP_PRO_PDAF_BYPASS,
	ISP_PRO_PDAF_SET_MODE,
	ISP_PRO_PDAF_SET_SKIP_NUM,
	ISP_PRO_PDAF_SET_ROI,
	ISP_PRO_PDAF_SET_PPI_INFO,
	ISP_PRO_PDAF_SET_EXTRACTOR_BYPASS,
};

enum isp_pdaf_correct_property {
	ISP_PRO_PDAF_SET_CORRECT_PARAM,
};

/*isp sub block: post_blc*/
enum isp_post_blc_property {
	ISP_PRO_POST_BLC_BLOCK,
};

/*isp sub block: post_cdn*/
enum isp_post_cdn_property {
	ISP_PRO_POST_CDN_BLOCK,
};

/*isp sub block: pstrz*/
enum isp_pstrz_property {
	ISP_PRO_POSTERIZE_BLOCK,
};

/*isp sub block: rgb_gain*/
enum isp_rgb_gain_property {
	ISP_PRO_RGB_GAIN_BLOCK,
	ISP_PRO_RGB_GAIN_BYPASS,
	ISP_PRO_RGB_GAIN_GLOBAL_GAIN,
	ISP_PRO_RGB_GAIN_RGB_GAIN,
	ISP_PRO_RGB_GAIN_RANDOM_PARAM,
	ISP_PRO_RGB_GAIN_RANDOM_INIT,
};

/*isp sub block: rgb_dither*/
enum isp_rgb_edither_property {
	ISP_PRO_RGB_EDITHER_RANDOM_BLOCK,
	ISP_PRO_RGB_EDITHER_RANDOM_INIT,
};

/*isp sub block: rgb2y*/
enum isp_rgb2y_property {
	ISP_PRO_RGB2Y_BLOCK,
};

/*isp sub block: store*/
enum isp_store_property {
	ISP_PRO_STORE_BLOCK,
	ISP_PRO_STORE_SLICE_SIZE,
};

/*isp sub block: uvd*/
enum isp_uvd_property {
	ISP_PRO_UVD_BLOCK,
};

/*isp sub block: y_delay*/
enum isp_y_delay_property {
	ISP_PRO_Y_DELAY_BLOCK,
};

/*isp sub block: y_gamma*/
enum isp_ygamma_property {
	ISP_PRO_YGAMMA_BLOCK,
};

/*isp sub block: pre_cdn*/
enum isp_yuv_precdn_property {
	ISP_PRO_PRE_CDN_BLOCK,
};

enum isp_raw_aem_property {
	ISP_PRO_RAW_AEM_BLOCK,
	ISP_PRO_RAW_AEM_BYPASS,
	ISP_PRO_RAW_AEM_MODE,
	ISP_PRO_RAW_AEM_SKIP_NUM,
	ISP_PRO_RAW_AEM_SHIFT,
	ISP_PRO_RAW_AEM_OFFSET,
	ISP_PRO_RAW_AEM_BLK_SIZE,
	ISP_PRO_RAW_AEM_SLICE_SIZE,
};

enum isp_raw_af_property {
	ISP_PRO_RGB_AFM_BYPASS,
	ISP_PRO_RGB_AFM_BLOCK,
	ISP_PRO_RGB_AFM_FRAME_SIZE,
	ISP_PRO_RGB_AFM_IIR_NR_CFG,
	ISP_PRO_RGB_AFM_MODULE_CFG,
	ISP_PRO_RGB_AFM_WIN,
	ISP_PRO_RGB_AFM_WIN_NUM,
	ISP_PRO_RGB_AFM_MODE,
	ISP_PRO_RGB_AFM_SKIP_NUM,
	ISP_PRO_RGB_AFM_SKIP_NUM_CLR,
};

enum isp_dev_capability {
	ISP_CAPABILITY_CONTINE_SIZE,
	ISP_CAPABILITY_TIME,
};

enum isp_3dnr_property {
	ISP_PRO_3DNR_UPDATE_PRE_PARAM,
	ISP_PRO_3DNR_UPDATE_CAP_PARAM
};

struct isp_io_param {
	unsigned int isp_id;
	unsigned int sub_block;
	unsigned int property;
	void  __user  *property_param;
};

struct isp_addr {
	unsigned long	chn0;
	unsigned long	chn1;
	unsigned long	chn2;
};

struct isp_img_size {
	unsigned int width;
	unsigned int height;
};

struct isp_lsc_addr {
	unsigned int phys_addr;
	unsigned int virt_addr;
	unsigned int buf_len;
};

struct isp_img_offset {
	unsigned int x;
	unsigned int y;
};

struct isp_coord {
	unsigned int  start_x;
	unsigned int  start_y;
	unsigned int  end_x;
	unsigned int  end_y;
};
struct img_offset {
	unsigned int x;
	unsigned int y;
};

struct awbc_param {
	unsigned int r;
	unsigned int b;
	unsigned int gr;
	unsigned int gb;
};

struct awbc_rgb {
	unsigned int r;
	unsigned int g;
	unsigned int b;
};

struct cmc_matrix {
	unsigned short val[9];
	unsigned short reserved;
};

struct isp_common_lbuf_param {
	unsigned int comm_lbuf_offset;
	unsigned int ydly_lbuf_offset;
};

struct isp_common_sdw_ctrl {
	unsigned int comm_cfg_rdy;
	unsigned int shadow_mctrl;
	unsigned int sdw_ctrl;
};

struct isp_dev_raw_aem_info {
	unsigned int bypass;
	unsigned int skip_num;
	struct img_offset offset;
	unsigned int aem_avgshf;
	struct isp_img_size blk_size;
	struct isp_img_size slice_size;
	unsigned ddr_wr_num;
};

struct yuv_param {
	unsigned int y;
	unsigned int u;
	unsigned int v;
};

struct uv_param {
	unsigned int u;
	unsigned int v;
};

struct isp_img_addr {
	unsigned long y_phy_addr;
	unsigned long uv_phy_addr;
};

struct isp_dev_1d_lsc_info {
	unsigned int bypass;
	unsigned int buf_sel;
	unsigned int radius_step;
	unsigned int center_r0c0_row_y;
	unsigned int center_r0c0_col_x;
	unsigned int center_r0c1_row_y;
	unsigned int center_r0c1_col_x;
	unsigned int center_r1c0_row_y;
	unsigned int center_r1c0_col_x;
	unsigned int center_r1c1_row_y;
	unsigned int center_r1c1_col_x;
	unsigned int init_r_r0c1;
	unsigned int init_r_r0c0;
	unsigned int init_r_r1c1;
	unsigned int init_r_r1c0;
	unsigned int init_r2_r0c0;
	unsigned int init_r2_r0c1;
	unsigned int init_r2_r1c0;
	unsigned int init_r2_r1c1;
	unsigned int init_dr2_r0c0;
	unsigned int init_dr2_r0c1;
	unsigned int init_dr2_r1c0;
	unsigned int init_dr2_r1c1;
	unsigned int data_ptr[2]; /*compatible with 64bit cpu*/
};

struct lnc_1d_slice_param {
	uint32_t start_col;
	uint32_t start_row;
	uint32_t r[4];
	uint32_t r2[4];
	uint32_t dr2[4];
};

struct isp_dev_2d_lsc_info {
	unsigned int bypass;
	unsigned int offset_x;
	unsigned int offset_y;
	unsigned int loader_enable;
	unsigned int grid_pitch;
	unsigned int grid_width;
	unsigned int grid_x_num;
	unsigned int grid_y_num;
	unsigned int grid_num_t;
	unsigned int load_chn_sel;
	unsigned int endian;
	struct isp_img_size slice_size;
	unsigned int relative_x;
	unsigned int relative_y;
	unsigned int q_value[2][5];
	unsigned int buf_addr[2];
	unsigned int buf_len;
	unsigned int weight_num;
	unsigned int data_ptr[2];
};

/*anti	flicker */
struct isp_dev_anti_flicker_new_info {
	unsigned int  bypass;
	unsigned int  mode;
	unsigned int  skip_frame_num;
	unsigned int  afl_stepx;
	unsigned int  afl_stepy;
	unsigned int  frame_num;
	unsigned int  start_col;
	unsigned int  end_col;
	unsigned int  mem_init_addr;
	unsigned int  step_x_region;
	unsigned int  step_y_region;
	unsigned int  step_x_start_region;
	unsigned int  step_x_end_region;
	unsigned int  mem_init_addr_region;
	struct isp_img_size img_size;
};

struct isp_dev_anti_flicker_info {
	unsigned int  bypass;
	unsigned int  mode;
	unsigned int  skip_frame_num;
	unsigned int  line_step;
	unsigned int  frame_num;
	unsigned int  vheight;
	unsigned int  start_col;
	unsigned int  end_col;
	unsigned int  afl_total_num;
	struct isp_img_size img_size;
};

struct isp_dev_arbiter_info {
	unsigned int fetch_raw_endian;
	unsigned int fetch_bit_reorder;
	unsigned int fetch_raw_word_change;
	unsigned int fetch_yuv_endian;
	unsigned int fetch_yuv_word_change;
};

struct isp_dev_awb_info {
	unsigned int awbc_bypass;
	struct awbc_param gain;
	struct awbc_rgb thrd;
	struct awbc_param gain_offset;
};

struct isp_dev_binning4awb_info {
	unsigned int bypass;
	unsigned int bin_mode_sel;
	unsigned int vx;
	unsigned int mem_fifo_clr;
	unsigned int hx;
	unsigned int bin_skip_num;
	unsigned int addr_ddr_init;
	unsigned int ddr_wr_num;
	unsigned int skip_num_clr;
	struct isp_img_size img_size;
};

struct isp_dev_blc_info {
	unsigned int bypass;
	unsigned int mode;
	unsigned int r;
	unsigned int b;
	unsigned int gr;
	unsigned int gb;
};

struct isp_dev_bpc_info {
	unsigned int bypass;
	unsigned int bpc_mode;
	unsigned int edge_hv_mode;
	unsigned int edge_rd_mode;
	unsigned int bad_pixel_pos_out_en;
	unsigned int rd_retain_num;
	unsigned int rd_max_len_sel;
	unsigned int wr_max_len_sel;
	unsigned int double_badpixel_th[4];
	unsigned int three_badpixel_th[4];
	unsigned int four_badpixel_th[4];
	unsigned int texture_th;
	unsigned int flat_th;
	unsigned int shift[3];
	unsigned int edge_ratio_hv;
	unsigned int edge_ratio_rd;
	unsigned int high_offset;
	unsigned int low_offset;
	unsigned int high_coeff;
	unsigned int low_coeff;
	unsigned int min_coeff;
	unsigned int max_coeff;
	unsigned short intercept_b[8];
	unsigned short slope_k[8];
	unsigned short lut_level[8];
	unsigned int map_addr;
	unsigned int bad_map_hw_fifo_clr_en;
	unsigned int bad_pixel_num;
	unsigned int bad_pixel_pos_out_addr;
	unsigned int bad_pixel_pos_fifo_clr;
	unsigned int bpc_map_fifo_clr;
};

struct new_bpc_slice_param {
	unsigned int bad_pixel_num;
	unsigned int bpc_map_addr;
};

struct isp_dev_brightness_info {
	unsigned int bypass;
	unsigned int factor;
};

struct isp_dev_cce_info {
	unsigned int bypass;
	unsigned short matrix[9];
	unsigned short reserved0;
	unsigned short y_offset;
	unsigned short u_offset;
	unsigned short v_offset;
	unsigned short reserved1;
};

struct isp_dev_yuv_cdn_info {
	unsigned int bypass;
	unsigned int filter_bypass;
	unsigned int median_writeback_en;
	unsigned int median_mode;
	unsigned int gaussian_mode;
	unsigned int median_thr;
	unsigned int median_thru0;
	unsigned int median_thru1;
	unsigned int median_thrv0;
	unsigned int median_thrv1;
	unsigned int rangewu[31];
	unsigned int rangewv[31];
	unsigned int level;
};

struct isp_dev_cfa_info {
	unsigned int bypass;
	unsigned int css_bypass;
	unsigned int grid_thr;
	unsigned int min_grid_new;
	unsigned int grid_gain_new;
	unsigned int strong_edge_thr;
	unsigned int uni_dir_intplt_thr_new;
	unsigned int weight_control_bypass;
	unsigned int cdcr_adj_factor;
	unsigned int smooth_area_thr;
	unsigned int readblue_high_sat_thr;
	unsigned int grid_dir_weight_t1;
	unsigned int grid_dir_weight_t2;
	unsigned int round_diff_03_thr;
	unsigned int low_lux_03_thr;
	unsigned int round_diff_12_thr;
	unsigned int low_lux_12_thr;
	unsigned int css_weak_edge_thr;
	unsigned int css_edge_thr;
	unsigned int css_texture1_thr;
	unsigned int css_texture2_thr;
	unsigned int css_uv_val_thr;
	unsigned int css_uv_diff_thr;
	unsigned int css_gray_thr;
	unsigned int css_pix_similar_thr;
	unsigned int css_green_edge_thr;
	unsigned int css_green_weak_edge_thr;
	unsigned int css_green_tex1_thr;
	unsigned int css_green_tex2_thr;
	unsigned int css_green_flat_thr;
	unsigned int css_edge_corr_ratio_r;
	unsigned int css_edge_corr_ratio_b;
	unsigned int css_text1_corr_ratio_r;
	unsigned int css_text1_corr_ratio_b;
	unsigned int css_text2_corr_ratio_r;
	unsigned int css_text2_corr_ratio_b;
	unsigned int css_flat_corr_ratio_r;
	unsigned int css_flat_corr_ratio_b;
	unsigned int css_wedge_corr_ratio_r;
	unsigned int css_wedge_corr_ratio_b;
	unsigned int css_alpha_for_tex2;
	unsigned int css_skin_u_top[2];
	unsigned int css_skin_u_down[2];
	unsigned int css_skin_v_top[2];
	unsigned int css_skin_v_down[2];
	unsigned int gbuf_addr_max;
};

struct isp_dev_cmc10_info {
	unsigned int bypass;
	struct cmc_matrix matrix;
};

struct isp_dev_common_info {
	unsigned int fetch_sel_0;
	unsigned int store_sel_0;
	unsigned int fetch_sel_1;
	unsigned int store_sel_1;
	unsigned int fetch_color_space_sel;
	unsigned int store_color_space_sel;
	unsigned int ch0_path_ctrl;
	unsigned int bin_pos_sel;
	unsigned int ram_mask;
	unsigned int gclk_ctrl_rrgb;
	unsigned int gclk_ctrl_yiq_frgb;
	unsigned int gclk_ctrl_yuv;
	unsigned int gclk_ctrl_scaler_3dnr;
	struct isp_common_lbuf_param lbuf_off;
	struct isp_common_sdw_ctrl shadow_ctrl_ch0;
	struct isp_common_sdw_ctrl shadow_ctrl_ch1;
	unsigned int afl_version_sel_ch0;
	unsigned int res[3];
	unsigned int yuv_disp_path_sel_sdw_en;
	unsigned int store_cce_path_sel;
	unsigned int store_cce_en;
	unsigned int yuv_disp_path_sel_ch0;
	unsigned int yuv_disp_path_sel_ch1;
	unsigned int scl_pre_path_sel;
	unsigned int scl_vid_path_sel;
	unsigned int scl_cap_path_sel;
	unsigned int store_out_path_sel;
	unsigned int jpg_frame_done_en;
	unsigned int jpg_frame_done_clr;
	unsigned int jpg_line_done_en;
	unsigned int isp_soft_rst;
	unsigned int isp_cfg_sof_rst;
	unsigned int fetch_color_format;
	unsigned int store_color_format;
};

struct isp_dev_contrast_info {
	unsigned int bypass;
	unsigned int factor;
};

struct csa_factor {
	unsigned int bypass;
};

struct isp_dev_csa_info {
	unsigned int bypass;
	unsigned int factor_u;
	unsigned int factor_v;
	struct csa_factor factor;
};

/*isp sub block: dispatch*/
struct isp_dev_dispatch_info {
	unsigned int  bayer_ch0;
	struct isp_img_size ch0_size;
	struct isp_img_size ch1_size;
	unsigned short  width_dly_num_ch0;
	unsigned short  height_dly_num_ch0;
	unsigned int  bayer_ch1;
	unsigned short  nready_cfg_ch0;
	unsigned short  nready_width_ch0;
	unsigned short  pipe_dly_num;
};

struct isp_dispatch_dly_num_info {
	unsigned short  width_dly_num_ch0;
	unsigned short  height_dly_num_ch0;
};

struct isp_dispatch_hw_ctrl_info {
	unsigned short  nready_cfg_ch0;
	unsigned short  nready_width_ch0;
};

/*isp sub block: dispatch_yuv*/
struct isp_dev_dispatch_yuv_info {
	unsigned int  bayer_ch0;
	struct isp_img_size ch0_size;
	struct isp_img_size ch1_size;
	unsigned short  width_dly_num_ch0;
	unsigned short  height_dly_num_ch0;
	unsigned int  bayer_ch1;
	unsigned short  nready_cfg_ch0;
	unsigned short  nready_width_ch0;
	unsigned short  nready_cfg_ch1;
	unsigned short  nready_width_ch1;
	unsigned short  width_dly_num_ch1;
	unsigned short  height_dly_num_ch1;
};
struct isp_dispatch_yuv_dly_info {
	unsigned short height_dly_num_ch0;
	unsigned short width_dly_num_ch0;
};

struct isp_dispatch_yuv_hw_ctrl_info {
	unsigned short nready_cfg_ch0;
	unsigned short nready_width_ch0;
};

struct isp_dispatch_yuv_size_info {
	unsigned int  width;
	unsigned int  height;
};

struct isp_dispatch_yuv_cap_ctrl_info {
	unsigned short cap_line_scl_dly_cycle_ctrl;
	unsigned short pre_line_scl_dly_cycle_ctrl;
};

struct isp_dispatch_yuv_cap_ctrl2_info {
	unsigned short  pre_line_hole_pix_num;
	unsigned short  pre_line_whole_cyle;
};

/*isp sub block: edge*/
struct edge_pn_config {
	unsigned int  p;
	unsigned int  n;
};

struct isp_dev_edge_info {
	unsigned int  bypass;
	/*CFG0*/
	unsigned int  flat_smooth_mode;
	unsigned int  edge_smooth_mode;
	struct edge_pn_config ee_str_d;
	unsigned int  mode;
	/*CFG1*/
	struct edge_pn_config ee_incr_d;
	struct edge_pn_config ee_edge_thr_d;
	/*CFG2*/
	struct edge_pn_config ee_corner_sm;
	struct edge_pn_config ee_corner_gain;
	struct edge_pn_config ee_corner_th;
	unsigned int  ee_corner_cor;
	unsigned int  ee_cv_t[4];
	struct edge_pn_config ee_cv_clip;
	unsigned int  ee_cv_r[3];

	unsigned int  ipd_bypass;
	unsigned int  ipd_mask_mode;
	struct edge_pn_config ipd_less_thr;
	unsigned int  ipd_smooth_en;
	struct edge_pn_config ipd_smooth_mode;
	struct edge_pn_config ipd_flat_thr;
	struct edge_pn_config ipd_eq_thr;
	struct edge_pn_config ipd_more_thr;
	struct edge_pn_config ipd_smooth_edge_thr;
	struct edge_pn_config ipd_smooth_edge_diff;
	unsigned int  ee_ratio_hv_3;
	unsigned int  ee_ratio_hv_5;
	unsigned int  ee_ratio_diag_3;
	unsigned int  ee_weight_hv2diag;
	unsigned int  ee_gradient_computation_type;
	unsigned int  ee_weight_diag2hv;
	unsigned int  ee_gain_hv_t[2][4];
	unsigned int  ee_gain_hv_r[2][3];
	unsigned int  ee_ratio_diag_5;
	unsigned int  ee_gain_diag_t[2][4];
	unsigned int  ee_gain_diag_r[2][3];
	unsigned int  ee_lum_t[4];
	unsigned int  ee_lum_r[3];
	unsigned int  ee_pos_t[4];
	unsigned int  ee_pos_r[3];
	unsigned int  ee_pos_c[3];
	unsigned int  ee_neg_t[4];
	unsigned int  ee_neg_r[3];
	unsigned int  ee_neg_c[3];
	unsigned int  ee_freq_t[4];
	unsigned int  ee_freq_r[3];
};

/*isp sub block: fetch*/
struct isp_addr_fs {
	unsigned int  chn0;
	unsigned int  chn1;
	unsigned int  chn2;
};

struct isp_pitch_fs	{
	unsigned int  chn0;
	unsigned int  chn1;
	unsigned int  chn2;
};

struct isp_dev_block_addr {
	struct isp_addr img_vir;
	struct isp_addr img_offset;
	unsigned int img_fd;
};

struct isp_dev_fetch_info {
	unsigned int  bypass;
	unsigned int  subtract;
	unsigned int  color_format;
	unsigned int  start_isp;
	struct isp_img_size size;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;
	unsigned int  mipi_word_num;
	unsigned int  mipi_byte_rel_pos;
	unsigned int  no_line_dly_ctrl;
	unsigned int  req_cnt_num;
	unsigned int  line_dly_num;
	struct isp_dev_block_addr fetch_addr;
};

struct isp_dev_fetch2_info {
	unsigned int  bypass;
	unsigned int  subtract;
	unsigned int  color_format;
	unsigned int  ft0_axi_reorder_en;
	unsigned int  ft1_axi_reorder_en;
	unsigned int  ft2_axi_reorder_en;
	unsigned int  chk_sum_clr_en;
	unsigned int  first_line_mode;
	unsigned int  last_line_mode;
	unsigned int  start_isp;
	struct isp_img_size size;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;
	unsigned int  hblank_num;
	struct	isp_addr_fs retain_num;
	struct	isp_addr_fs max_len_sel;
};

/*isp sub block: edge*/
struct ee_param {
	unsigned int  ee_mode;
	unsigned int  ee_str_d_p;
	unsigned int  ee_str_d_n;
	unsigned int  ee_edge_thr_d_p;
	unsigned int  ee_edge_thr_d_n;
	unsigned int  ee_incr_d_p;
	unsigned int  ee_incr_d_n;
	unsigned int  ee_corner_cor;
	unsigned int  ee_corner_th_p;
	unsigned int  ee_corner_th_n;
	unsigned int  ee_corner_gain_p;
	unsigned int  ee_corner_gain_n;
	unsigned int  ee_corner_sm_p;
	unsigned int  ee_corner_sm_n;
	unsigned int  ee_edge_smooth_mode;
	unsigned int  ee_flat_smooth_mode;
	unsigned int  ipd_bypass;
	unsigned int  ipd_mask_mode;
	unsigned int  ipd_flat_thr_p;
	unsigned int  ipd_flat_thr_n;
	unsigned int  ipd_eq_thr_p;
	unsigned int  ipd_eq_thr_n;
	unsigned int  ipd_more_thr_p;
	unsigned int  ipd_more_thr_n;
	unsigned int  ipd_less_thr_p;
	unsigned int  ipd_less_thr_n;
	unsigned int  ipd_smooth_en;
	unsigned int  ipd_smooth_mode_p;
	unsigned int  ipd_smooth_mode_n;
	unsigned int  ipd_smooth_edge_thr_p;
	unsigned int  ipd_smooth_edge_thr_n;
	unsigned int  ipd_smooth_edge_diff_p;
	unsigned int  ipd_smooth_edge_diff_n;
	unsigned int  ee_cv_t1;
	unsigned int  ee_cv_t2;
	unsigned int  ee_cv_t3;
	unsigned int  ee_cv_t4;
	unsigned int  ee_cv_r1;
	unsigned int  ee_cv_r2;
	unsigned int  ee_cv_r3;
	unsigned int  ee_cv_clip_p;
	unsigned int  ee_cv_clip_n;
	unsigned int  ee_ratio_hv_3;
	unsigned int  ee_ratio_hv_5;
	unsigned int  ee_ratio_diag_3;
	unsigned int  ee_ratio_diag_5;
	unsigned int  ee_gain_hv_1_t1;
	unsigned int  ee_gain_hv_1_t2;
	unsigned int  ee_gain_hv_1_t3;
	unsigned int  ee_gain_hv_1_t4;
	unsigned int  ee_gain_hv_1_r1;
	unsigned int  ee_gain_hv_1_r2;
	unsigned int  ee_gain_hv_1_r3;
	unsigned int  ee_gain_hv_2_t1;
	unsigned int  ee_gain_hv_2_t2;
	unsigned int  ee_gain_hv_2_t3;
	unsigned int  ee_gain_hv_2_t4;
	unsigned int  ee_gain_hv_2_r1;
	unsigned int  ee_gain_hv_2_r2;
	unsigned int  ee_gain_hv_2_r3;
	unsigned int  ee_gain_diag_1_t1;
	unsigned int  ee_gain_diag_1_t2;
	unsigned int  ee_gain_diag_1_t3;
	unsigned int  ee_gain_diag_1_t4;
	unsigned int  ee_gain_diag_1_r1;
	unsigned int  ee_gain_diag_1_r2;
	unsigned int  ee_gain_diag_1_r3;
	unsigned int  ee_gain_diag_2_t1;
	unsigned int  ee_gain_diag_2_t2;
	unsigned int  ee_gain_diag_2_t3;
	unsigned int  ee_gain_diag_2_t4;
	unsigned int  ee_gain_diag_2_r1;
	unsigned int  ee_gain_diag_2_r2;
	unsigned int  ee_gain_diag_2_r3;
	unsigned int  ee_weightt_hv2diag;
	unsigned int  ee_weightt_diag2hv;
	unsigned int  ee_gradient_computation_type;
	unsigned int  ee_lum_t1;
	unsigned int  ee_lum_t2;
	unsigned int  ee_lum_t3;
	unsigned int  ee_lum_t4;
	unsigned int  ee_lum_r1;
	unsigned int  ee_lum_r2;
	unsigned int  ee_lum_r3;
	unsigned int  ee_pos_t1;
	unsigned int  ee_pos_t2;
	unsigned int  ee_pos_t3;
	unsigned int  ee_pos_t4;
	unsigned int  ee_pos_r1;
	unsigned int  ee_pos_r2;
	unsigned int  ee_pos_r3;
	unsigned int  ee_pos_c1;
	unsigned int  ee_pos_c2;
	unsigned int  ee_pos_c3;
	unsigned int  ee_neg_t1;
	unsigned int  ee_neg_t2;
	unsigned int  ee_neg_t3;
	unsigned int  ee_neg_t4;
	unsigned int  ee_neg_r1;
	unsigned int  ee_neg_r2;
	unsigned int  ee_neg_r3;
	unsigned int  ee_neg_c1;
	unsigned int  ee_neg_c2;
	unsigned int  ee_neg_c3;
	unsigned int  ee_freq_t1;
	unsigned int  ee_freq_t2;
	unsigned int  ee_freq_t3;
	unsigned int  ee_freq_t4;
	unsigned int  ee_freq_r1;
	unsigned int  ee_freq_r2;
	unsigned int  ee_freq_r3;
};

/*isp sub block: gamma*/
struct coordinate_xy {
	 unsigned short node_x;
	 unsigned short node_y;
};

struct coordinate_xyz {
	 unsigned short node_x;
	 unsigned short node_y;
	 unsigned short node_z;
};

struct gamc_curve_info {
	struct coordinate_xy nodes_r[ISP_PINGPANG_FRGB_GAMC_NUM];
	struct coordinate_xy nodes_g[ISP_PINGPANG_FRGB_GAMC_NUM];
	struct coordinate_xy nodes_b[ISP_PINGPANG_FRGB_GAMC_NUM];
};

struct isp_dev_gamma_info {
	unsigned int  bypass;
	struct gamc_curve_info gamc_nodes;
};

struct gamma_buf_sel {
	unsigned int r_buf_sel;
	unsigned int g_buf_sel;
	unsigned int b_buf_sel;
};

/*isp sub block: grgb*/
struct grgb_param_info {
	unsigned int	grgb_diff_th;
	unsigned int	grgb_hv_edge_thr;
	unsigned int	grgb_slash_edge_thr;
	unsigned int	grgb_hv_flat_thr;
	unsigned int	grgb_slash_flat_thr;
	unsigned int	grgb_gr_ratio;
	unsigned int	grgb_gb_ratio;
	unsigned int	grgb_lum_curve_flat_t1;
	unsigned int	grgb_lum_curve_flat_t2;
	unsigned int	grgb_lum_curve_flat_t3;
	unsigned int	grgb_lum_curve_flat_t4;
	unsigned int	grgb_lum_curve_flat_r1;
	unsigned int	grgb_lum_curve_flat_r2;
	unsigned int	grgb_lum_curve_flat_r3;
	unsigned int	grgb_lum_curve_edge_t1;
	unsigned int	grgb_lum_curve_edge_t2;
	unsigned int	grgb_lum_curve_edge_t3;
	unsigned int	grgb_lum_curve_edge_t4;
	unsigned int	grgb_lum_curve_edge_r1;
	unsigned int	grgb_lum_curve_edge_r2;
	unsigned int	grgb_lum_curve_edge_r3;
	unsigned int	grgb_lum_curve_texture_t1;
	unsigned int	grgb_lum_curve_texture_t2;
	unsigned int	grgb_lum_curve_texture_t3;
	unsigned int	grgb_lum_curve_texture_t4;
	unsigned int	grgb_lum_curve_texture_r1;
	unsigned int	grgb_lum_curve_texture_r2;
	unsigned int	grgb_lum_curve_texture_r3;
	unsigned int	grgb_frez_curve_flat_t1;
	unsigned int	grgb_frez_curve_flat_t2;
	unsigned int	grgb_frez_curve_flat_t3;
	unsigned int	grgb_frez_curve_flat_t4;
	unsigned int	grgb_frez_curve_flat_r1;
	unsigned int	grgb_frez_curve_flat_r2;
	unsigned int	grgb_frez_curve_flat_r3;
	unsigned int	grgb_frez_curve_edge_t1;
	unsigned int	grgb_frez_curve_edge_t2;
	unsigned int	grgb_frez_curve_edge_t3;
	unsigned int	grgb_frez_curve_edge_t4;
	unsigned int	grgb_frez_curve_edge_r1;
	unsigned int	grgb_frez_curve_edge_r2;
	unsigned int	grgb_frez_curve_edge_r3;
	unsigned int	grgb_frez_curve_texture_t1;
	unsigned int	grgb_frez_curve_texture_t2;
	unsigned int	grgb_frez_curve_texture_t3;
	unsigned int	grgb_frez_curve_texture_t4;
	unsigned int	grgb_frez_curve_texture_r1;
	unsigned int	grgb_frez_curve_texture_r2;
	unsigned int	grgb_frez_curve_texture_r3;
};

struct grgb_param {
	unsigned int  curve_t[3][4];
	unsigned int  curve_r[3][3];
};

struct isp_dev_grgb_info {
	unsigned int  bypass;
	unsigned int  diff_thd;
	unsigned int  hv_edge_thr;
	unsigned int  check_sum_clr;
	unsigned int  slash_edge_thr;
	unsigned int  slash_flat_thr;
	unsigned int  gr_ratio;
	unsigned int  hv_flat_thr;
	unsigned int  gb_ratio;
	struct grgb_param lum;
	struct grgb_param frez;
};

/*isp sub block: hist*/
struct isp_dev_hist_info {
	unsigned int  bypass;
	unsigned int  mode;
	unsigned int  skip_num;
};

/*isp sub block: hist2*/
struct isp_dev_hist2_info {
	unsigned int  bypass;
	unsigned int  mode;
	unsigned int  skip_num;
	struct isp_coord hist_roi;
	uint32_t  skip_num_clr;
};

struct hist_roi {
	unsigned int hist_roi_x_s;
	unsigned int hist_roi_y_s;
	unsigned int hist_roi_x_e;
	unsigned int hist_roi_y_e;
};

/*isp sub block: hsv*/
struct isp_hsv_region_info {
	unsigned short  s_curve[5][4];
	unsigned short  v_curve[5][4];
	unsigned int  hrange_left[5];
	unsigned int  hrange_right[5];
};

struct hsv_curve {
	unsigned int h[5][2];
	unsigned int s[5][4];
	unsigned int v[5][4];
};

struct isp_dev_hsv_info {
	unsigned int  bypass;
	unsigned int  buf_sel;
	struct isp_hsv_region_info region_info[2];
	unsigned int data_ptr[2]; /*compatible with 64bit cpu*/
	unsigned int size;
};

/*isp sub block: hue*/
struct isp_dev_hue_info {
	unsigned int bypass;
	unsigned int theta;
};

/*isp sub block: iircnr*/
struct isp_dev_iircnr_info {
	unsigned int  bypass;
	unsigned int  mode;
	unsigned int  uv_th;
	unsigned int  y_max_th;
	unsigned int  y_min_th;
	unsigned int  uv_dist;
	unsigned int  uv_pg_th;
	unsigned int  sat_ratio;
	unsigned int  uv_low_thr2;
	unsigned int  uv_low_thr1;
	unsigned int  ymd_u;
	unsigned int  ymd_v;
	unsigned int  uv_s_th;
	unsigned int  slope_y_0;
	unsigned int  y_th;
	unsigned int  alpha_low_u;
	unsigned int  alpha_low_v;
	unsigned int  middle_factor_y_0;
	unsigned int  uv_high_thr2_0;

	unsigned int  yrandom_bypass;
	unsigned int  yrandom_mode;
	unsigned int  seed;
	unsigned int  offset;
	unsigned int  shift;
	unsigned char   takeBit[8];
	unsigned int  init;
	unsigned int  ymd_min_u;
	unsigned int  ymd_min_v;
	unsigned int  uv_low_thr[7][2];
	unsigned int  y_edge_thr_max[8];
	unsigned int  y_edge_thr_min[8];
	unsigned int  uv_high_thr2[7];
	unsigned int  slope_y[7];
	unsigned int  middle_factor_y[7];
	unsigned int  middle_factor_uv[8];
	unsigned int  slope_uv[8];
	unsigned int  pre_uv_th;
	unsigned int  css_lum_thr;
	unsigned int  uv_diff_thr;
};

struct iircnr_thr {
	unsigned int y_th;
	unsigned int uv_th;
	unsigned int uv_pg_th;
	unsigned int uv_dist;
	unsigned int uv_s_th;
	unsigned int alpha_low_u;
	unsigned int alpha_low_v;
	unsigned int y_max_th;
	unsigned int y_min_th;
	unsigned int pre_uv_th;
	unsigned int sat_ratio;
	unsigned int y_edge_thr_max[8];
	unsigned int y_edge_thr_min[8];
	unsigned int uv_low_thr1_tbl[8];
	unsigned int uv_low_thr2_tbl[8];
	unsigned int uv_high_thr2_tbl[8];
	unsigned int slope_uv[8];
	unsigned int middle_factor_uv[8];
	unsigned int middle_factor_y[8];
	unsigned int slope_y[8];
	unsigned int uv_diff_thr;
	unsigned int css_lum_thr;
};

struct iircnr_ymd {
	unsigned int ymd_u;
	unsigned int ymd_v;
	unsigned int ymd_min_u;
	unsigned int ymd_min_v;
};

struct yrandom_takebit {
	unsigned int takeBit[8];
};

/*isp sub block: nlc*/
struct nlc_node {
	unsigned short r_node[29];
	unsigned short reserved0;
	unsigned short g_node[29];
	unsigned short reserved1;
	unsigned short b_node[29];
	unsigned short reserved2;
	unsigned short l_node[27];
	unsigned short reserved3;
};

struct isp_dev_nlc_info {
	unsigned int bypass;
	struct nlc_node node;
};

/*isp sub block: nlm*/
struct isp_dev_vst_info {
	unsigned int  bypass;
	unsigned int  buf_sel;
};

#pragma pack(push)
#pragma pack(4)
struct lum_flat_param {
	unsigned short thresh;
	unsigned short match_count;
	unsigned short inc_strength;
	unsigned short reserved;
};
#pragma pack()

#pragma pack(push)
#pragma pack(4)
struct isp_dev_nlm_info {
	unsigned int  bypass;
	unsigned int  imp_opt_bypass;
	unsigned int  flat_opt_bypass;
	unsigned int  direction_mode_bypass;
	unsigned int  first_lum_byapss;
	unsigned int  simple_bpc_bypass;
	unsigned int  dist_mode;
	unsigned int  flat_opt_mode;
	unsigned int  den_strength;
	unsigned char   w_shift[3];
	unsigned char   reserved;
	unsigned int  direction_cnt_th;
	unsigned int  simple_bpc_lum_th;
	unsigned int  simple_bpc_th;
	unsigned int  lum_th0;
	unsigned int  lum_th1;
	unsigned int  diff_th;
	unsigned int  tdist_min_th;
	unsigned short  lut_w[72];
	struct lum_flat_param  lum_flat[3][3];
	unsigned short  lum_flat_addback0[3][4];
	unsigned short  lum_flat_addback1[3][4];
	unsigned short  lum_flat_addback_min[3][4];
	unsigned short  lum_flat_addback_max[3][4];
	unsigned int  lum_flat_dec_strenth[3];
	unsigned int vst_addr[2];
	unsigned int vst_len;
	unsigned int ivst_addr[2];
	unsigned int ivst_len;
	unsigned int nlm_len;
};
#pragma pack()

struct isp_dev_ivst_info {
	unsigned int  bypass;
	unsigned int  buf_sel;
};

struct isp_dev_noise_filter_info {
	unsigned int  yrandom_bypass;
	unsigned int  shape_mode;
	unsigned int  filter_thr_mode;
	unsigned int  yrandom_mode;
	unsigned int  yrandom_seed[4];
	unsigned int  takebit[8];
	unsigned int  r_offset;
	unsigned int  r_shift;
	unsigned int  filter_thr;
	unsigned int  cv_t[4];
	unsigned int  cv_r[3];
	struct edge_pn_config  noise_clip;
};

struct noise_filter_param {
	unsigned int	noisefilter_shape_mode;
	unsigned int	noisefilter_takeBit[8];
	unsigned int	noisefilter_r_shift;
	unsigned int	noisefilter_r_offset;
	unsigned int	noisefilter_filter_thr;
	unsigned int	noisefilter_filter_thr_m;
	unsigned int	noisefilter_av_t1;
	unsigned int	noisefilter_av_t2;
	unsigned int	noisefilter_av_t3;
	unsigned int	noisefilter_av_t4;
	unsigned int	noisefilter_av_r1;
	unsigned int	noisefilter_av_r2;
	unsigned int	noisefilter_av_r3;
	unsigned int	noisefilter_clip_p;
	unsigned int	noisefilter_clip_n;
};

/*isp sub block: pgg*/
struct isp_dev_pre_glb_gain_info {
	unsigned int bypass;
	unsigned int gain;
};

/*isp sub block: ynr*/
struct isp_dev_ynr_info {
	unsigned int  bypass;
	unsigned int  lowlux_bypass;
	unsigned int  nr_enable;
	unsigned int  l_blf_en[3];
	unsigned int  txt_th;
	unsigned int  edge_th;
	unsigned int  flat_th[7];
	unsigned int  lut_th[7];
	unsigned int  addback[9];
	unsigned int  sub_th[9];
	unsigned int  l_euroweight[3][3];
	unsigned int  l_wf_index[3];
	unsigned int  l0_lut_th0;
	unsigned int  l0_lut_th1;
	unsigned int  l1_txt_th0;
	unsigned int  l1_txt_th1;
	unsigned int  wlt_th[24];
	unsigned int  freq_ratio[24];
	struct img_offset start_pos;
	struct img_offset center;
	unsigned int  radius;
	unsigned int  dist_interval;
	unsigned int   sal_nr_str[8];
	unsigned int   sal_offset[8];
	unsigned int  edgeStep[8];
	unsigned int wlt_T[3];
	unsigned int ad_para[3];
	unsigned int ratio[3];
	unsigned int maxRadius;
};

struct ynr_param {
	unsigned int ydenoise_lowlux_bypass;
	unsigned int ydenoise_flat[7];
	unsigned int ydenoise_lut_thresh[7];
	unsigned int ydenoise_subthresh[9];
	unsigned int ydenoise_addback[9];
	unsigned int ydenoise_sedgethresh;
	unsigned int ydenoise_txtthresh;
	unsigned int ydenoise_l1_txt_thresh1;
	unsigned int ydenoise_l1_txt_thresh0;
	unsigned int ydenoise_l0_lut_thresh1;
	unsigned int ydenoise_l0_lut_thresh0;
	unsigned int ydenoise_l1_eurodist[3];
	unsigned int ydenoise_l3_wfindex;
	unsigned int ydenoise_l2_wfindex;
	unsigned int ydenoise_l1_wfindex;
	unsigned int ydenoise_l2_eurodist[3];
	unsigned int ydenoise_l3_eurodist[3];
	unsigned int ydenoise_wv_nr_enable;
	unsigned int ydenoise_l1_blf_enable;
	unsigned int ydenoise_l2_blf_enable;
	unsigned int ydenoise_l3_blf_enable;
	unsigned int wltt[24];
	unsigned int freqratio[24];
	unsigned int dist_interval;
	unsigned int ydenoise_radius;
	unsigned int ydenoise_imgcenterx;
	unsigned int ydenoise_imgcentery;
	unsigned int ydenoise_sal_nr_str[8];
	unsigned int ydenoise_sal_offset[8];
};

/*isp sub block: ynr*/
struct isp_rrgb	{
	unsigned int  r;
	unsigned int  b;
	unsigned int  gr;
	unsigned int  gb;
};

struct isp_dev_pdaf_info {
	unsigned int  bypass;
	unsigned int  corrector_bypass;
	unsigned int  phase_map_corr_en;
	struct isp_img_size block_size;
	unsigned int  grid_mode;
	struct isp_coord win;
	struct isp_coord block;
	struct isp_rrgb gain_upperbound;
	unsigned int phase_txt_smooth;
	unsigned int phase_gfilter;
	unsigned int phase_flat_smoother;
	unsigned int  hot_pixel_th[3];
	unsigned int  dead_pixel_th[3];
	unsigned int  flat_th;
	unsigned int  edge_ratio_hv;
	unsigned int  edge_ratio_rd;
	unsigned int  edge_ratio_hv_rd;
	unsigned int  phase_left_addr;
	unsigned int  phase_right_addr;
	unsigned int  phase_pitch;
	unsigned int  pattern_pixel_is_right[PDAF_PPI_NUM];
	unsigned int  pattern_pixel_row[PDAF_PPI_NUM];
	unsigned int  pattern_pixel_col[PDAF_PPI_NUM];
	unsigned int  gain_ori_left[2];
	unsigned int  gain_ori_right[2];
	unsigned int  extractor_bypass;
	unsigned int  mode_sel;
	unsigned int  skip_num;
	unsigned int  phase_data_dword_num;
	struct isp_rrgb  pdaf_blc;
	unsigned int data_ptr_left[2];
	unsigned int data_ptr_right[2];
};

struct pdaf_addr_info {
	unsigned int addr_l;
	unsigned int addr_r;
};

struct pdaf_extraction_param {
	unsigned int ppi_block_start_col;
	unsigned int ppi_block_start_row;
	unsigned int ppi_block_end_col;
	unsigned int ppi_block_end_row;
	unsigned int ppi_block_width;
	unsigned int ppi_block_height;
	unsigned int pattern_row[PDAF_PPI_NUM];
	unsigned int pattern_col[PDAF_PPI_NUM];
	unsigned int pattern_pos[PDAF_PPI_NUM];
	unsigned int ppi_extractor_bypass;
	unsigned int ppi_skip_num;
	unsigned int skip_mode;
	unsigned int phase_data_write_num;
	unsigned int ppi_af_win_sy0;
	unsigned int ppi_af_win_sx0;
	unsigned int ppi_af_win_ey0;
	unsigned int ppi_af_win_ex0;
};

struct pdaf_correction_param {
	unsigned int ppi_grid;
	unsigned int l_gain[2];
	unsigned int r_gain[2];
	unsigned int ppi_corrector_bypass;
	unsigned int ppi_phase_map_corr_en;
	unsigned int ppi_upperbound_gr;
	unsigned int ppi_upperbound_gb;
	unsigned int ppi_upperbound_r;
	unsigned int ppi_upperbound_b;
	unsigned int ppi_blacklevel_gr;
	unsigned int ppi_blacklevel_r;
	unsigned int ppi_blacklevel_b;
	unsigned int ppi_blacklevel_gb;
	unsigned int ppi_phase_gfilter;
	unsigned int ppi_phase_flat_smoother;
	unsigned int ppi_phase_txt_smoother;
	unsigned int ppi_hot_1pixel_th;
	unsigned int ppi_hot_2pixel_th;
	unsigned int ppi_hot_3pixel_th;
	unsigned int ppi_dead_1pixel_th;
	unsigned int ppi_dead_2pixel_th;
	unsigned int ppi_dead_3pixel_th;
	unsigned int ppi_flat_th;
	unsigned int ppi_edgeRatio_hv_rd;
	unsigned int ppi_edgeRatio_hv;
	unsigned int ppi_edgeRatio_rd;
	unsigned short data_ptr_left[PDAF_CORRECT_GAIN_NUM];
	unsigned short data_ptr_right[PDAF_CORRECT_GAIN_NUM];
};

struct pdaf_ppi_info {
	struct isp_img_size block_size;
	struct isp_coord block;
	unsigned int  pattern_pixel_is_right[PDAF_PPI_NUM];
	unsigned int  pattern_pixel_row[PDAF_PPI_NUM];
	unsigned int  pattern_pixel_col[PDAF_PPI_NUM];
};

struct pdaf_roi_info {
	struct isp_coord win;
	unsigned int phase_data_write_num;
};

/*isp sub block: post_blc*/
struct isp_dev_post_blc_info {
	unsigned int  bypass;
	unsigned int  r_para;
	unsigned int  b_para;
	unsigned int  gr_para;
	unsigned int  gb_para;
};

/*isp sub block: post_cdn*/
struct cdn_thruv {
	unsigned short  thru0;
	unsigned short  thru1;
	unsigned short  thrv0;
	unsigned short  thrv1;
};

struct isp_dev_post_cdn_info {
	unsigned int  bypass;
	unsigned int  downsample_bypass;
	unsigned int  mode;
	unsigned int  writeback_en;
	unsigned int  uvjoint;
	unsigned int  median_mode;
	unsigned int  adapt_med_thr;
	unsigned int  uvthr0;
	unsigned int  uvthr1;
	struct cdn_thruv thr_uv;
	unsigned char   r_segu[2][7];
	unsigned char   r_segv[2][7];
	unsigned char   r_distw[15][5];
	unsigned char   reserved;
};

struct post_cdn_thr {
	unsigned int uvthr0;
	unsigned int uvthr1;
	unsigned int thru0;
	unsigned int thru1;
	unsigned int thrv0;
	unsigned int thrv1;
};

struct post_cdn_rseg {
	unsigned int r_segu[2][7];
	unsigned int r_segv[2][7];
};

struct post_cdn_distw {
	unsigned int r_distw[15][5];
};

/*isp sub block: pstrz*/
struct isp_dev_posterize_info {
	unsigned int  bypass;
	unsigned char   posterize_level_bottom[POSTERIZE_NUM];
	unsigned char   posterize_level_top[POSTERIZE_NUM];
	unsigned char   posterize_level_out[POSTERIZE_NUM];
};

struct pstrz_level {
	int posterize_level_bottom[8];
	int posterize_level_top[8];
	int posterize_level_out[8];
};

/*isp sub block: rlsc*/
struct rlsc_init_node {
	unsigned int  init_r0c0;
	unsigned int  init_r0c1;
	unsigned int  init_r1c0;
	unsigned int  init_r1c1;
};

struct isp_dev_radial_lsc_info {
	unsigned int  bypass;
	unsigned int  radius_step;
	unsigned int  buf_sel;
	struct img_offset center_r0c0_pos;
	struct img_offset center_r0c1_pos;
	struct img_offset center_r1c0_pos;
	struct img_offset center_r1c1_pos;
	struct img_offset start_pos;
	struct rlsc_init_node r_cfg;
	struct rlsc_init_node r2_cfg;
	struct rlsc_init_node dr2_cfg;
};

/*isp sub block: rgb_gain*/
struct isp_dev_rgb_gain_info {
	unsigned int  bypass;
	unsigned int  global_gain;
	unsigned int  r_gain;
	unsigned int  g_gain;
	unsigned int  b_gain;
};

struct isp_dev_rgb_dither_info {
	unsigned int random_bypass;
	unsigned int random_mode;
	unsigned int seed;
	unsigned int range;
	unsigned int r_offset;
	unsigned int r_shift;
	unsigned int takebit[8];
};

struct rgbg_gain {
	unsigned int r_gain;
	unsigned int g_gain;
	unsigned int b_gain;
};

/*isp sub block: rgb2y*/
struct isp_rgb {
	unsigned int  r;
	unsigned int  g;
	unsigned int  b;
};

struct isp_dev_rgb2y_info {
	struct isp_rgb coef;
};

/*isp sub block: store*/
struct store_border	{
	unsigned short  up_border;
	unsigned short  down_border;
	unsigned short  left_border;
	unsigned short  right_border;
};

struct isp_dev_feeder_info {
	unsigned int  data_type;
};

struct isp_dev_store_info {
	unsigned int  bypass;
	unsigned int  endian;
	unsigned int subtract;
	unsigned int  color_format;
	struct isp_img_size size;
	struct store_border border;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;
	unsigned short  shadow_clr_sel;
	unsigned short  shadow_clr;
	unsigned short  store_res;
	unsigned short  rd_ctrl;
};

struct isp_dev_store0_info {
	unsigned int  bypass;
	unsigned int  st_max_len_sel;
	unsigned int  yuv_mode;
	unsigned int  shadow_clr_sel;
	unsigned int  st_y_axi_reorder_en;
	unsigned int  st_uv_axi_reorder_en;
	struct isp_img_size size;
	unsigned int  st_luma_addr[NR3_BUF_NUM];
	unsigned int  st_chroma_addr[NR3_BUF_NUM];
	unsigned int  st_pitch;
	unsigned int  shadow_clr;
	unsigned int  buf_sel;
};

struct isp_dev_store_preview_info {
	unsigned int  bypass;
	unsigned int  endian;
	unsigned int  speed_2x;
	unsigned int  mirror_en;
	unsigned int  color_format;
	unsigned int  max_len_sel;
	struct isp_img_size size;
	struct store_border border;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;
	unsigned short  shadow_clr;
	unsigned short  store_res;
	unsigned short  rd_ctrl;
	unsigned short  shadow_clr_sel;
};

struct isp_dev_store_video_info {
	unsigned int  bypass;
	unsigned int  endian;
	unsigned int  speed_2x;
	unsigned int  mirror_en;
	unsigned int  color_format;
	unsigned int  max_len_sel;
	struct isp_img_size size;
	struct store_border border;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;
	unsigned short  shadow_clr;
	unsigned short  store_res;
	unsigned short  rd_ctrl;
	unsigned short  shadow_clr_sel;
};

struct isp_dev_store_capture_info {
	unsigned int  bypass;
	unsigned int  endian;
	unsigned int  speed_2x;
	unsigned int  mirror_en;
	unsigned int  color_format;
	unsigned int  max_len_sel;
	struct isp_img_size size;
	struct store_border	border;
	struct isp_addr_fs addr;
	struct isp_pitch_fs	pitch;
	unsigned short  shadow_clr;
	unsigned short  store_res;
	unsigned short  rd_ctrl;
	unsigned short  shadow_clr_sel;
};

struct isp_dev_scaler0_pre_info {
	unsigned int isp_sacler_all_bypass;
	unsigned int isp_uv_sync_y;
	struct isp_img_size size;
};

struct isp_dev_scaler_pre_info {
	unsigned int  bypass;
	struct isp_img_size size;
};

struct isp_dev_scaler_vid_info {
	unsigned int  bypass;
	struct isp_img_size size;
};

struct isp_dev_scaler_cap_info {
	unsigned int  bypass;
	struct isp_img_size size;
};

struct isp_dev_store_cce_info {
	unsigned int  bypass;
	unsigned int  max_len_sel;
	unsigned int  speed_2x;
	unsigned int  mirror_en;
	unsigned int  color_format;
	unsigned int  endian;
	struct isp_img_size size;
	struct store_border border;
	struct isp_addr_fs addr[NR3_BUF_NUM];
	struct isp_pitch_fs	pitch;
	unsigned short  shadow_clr;
	unsigned short  store_res;
	unsigned short  rd_ctrl;
	unsigned short  shadow_clr_sel;
	unsigned int  total_word;
	unsigned int  up_border;
	unsigned int  down_border;
	unsigned int  buf_sel;
};

struct slice_overlap {
	unsigned int overlap_up;
	unsigned int overlap_down;
	unsigned int overlap_left;
	unsigned int overlap_right;
};

/*isp sub block: uvd*/
struct uvd_th {
	unsigned char th_h[2];
	unsigned char th_l[2];
};

struct isp_dev_uvd_info {
	unsigned int  bypass;
	unsigned int  lum_th_h_len;
	unsigned int  lum_th_h;
	unsigned int  lum_th_l_len;
	unsigned int  lum_th_l;
	unsigned int  chroma_min_h;
	unsigned int  chroma_min_l;
	unsigned int  chroma_max_h;
	unsigned int  chroma_max_l;
	struct uvd_th u_th;
	struct uvd_th v_th;
	unsigned int  ratio;
	unsigned int  ratio_uv_min;
	unsigned int  ratio_y_min[2];
	unsigned int  ratio0;
	unsigned int  ratio1;
	unsigned int  y_th_l_len;
	unsigned int  y_th_h_len;
	unsigned int  uv_abs_th_len;
};

/*isp sub block: y_delay*/
struct isp_dev_ydelay_info {
	unsigned int  bypass;
	unsigned int  step;
};

/*isp sub block: y_gamma*/
struct isp_dev_ygamma_info {
	unsigned int bypass;
	unsigned int buf_sel;
	struct coordinate_xy nodes[ISP_PINGPANG_YUV_YGAMMA_NUM];
};

/*isp sub block: pre_cdn*/
struct isp_dev_yuv_precdn_info {
	unsigned int  bypass;
	unsigned int  mode;
	unsigned int  median_writeback_en;
	unsigned int  median_mode;
	unsigned int  den_stren;
	unsigned int  uv_joint;
	struct cdn_thruv median_thr_uv;
	unsigned int  median_thr;
	unsigned int  uv_thr;
	unsigned int  y_thr;
	unsigned char   r_segu[2][7];
	unsigned char   r_segv[2][7];
	unsigned char   r_segy[2][7];
	unsigned char   r_distw[25];
	unsigned char   reserved;
};

struct pre_cdn_thr {
	unsigned int median_thr;
	unsigned int y_thr;
	unsigned int uv_thr;
	unsigned int median_thr_u[2];
	unsigned int median_thr_v[2];
};

struct pre_cdn_seg {
	unsigned int r_segu[2][7];
	unsigned int r_segv[2][7];
	unsigned int r_segy[2][7];
};

struct pre_cdn_dist {
	unsigned int r_distw[25];
};

/*isp sub block: binning*/
struct isp_buf_node {
	unsigned int    type;
	unsigned long    k_addr;
	unsigned long    u_addr;
};

struct isp_scaling_ratio {
	unsigned char vertical;
	unsigned char horizontal;
};

struct isp_b4awb_phys {
	unsigned int phys0;
	unsigned int phys1;
};

enum isp_buf_node_type {
	ISP_NODE_TYPE_BINNING4AWB,
	ISP_NODE_TYPE_RAWAEM,
	ISP_NODE_TYPE_AE_RESERVED,
};

struct isp_statis_frame_output {
	unsigned int                     format;
	unsigned int                     buf_size;
	unsigned long                    phy_addr;
	unsigned long                    vir_addr;
};

struct isp_statis_buf_input {
	uint32_t                         buf_size;
	uint32_t                         buf_num;
	unsigned long                    phy_addr;
	unsigned long                    vir_addr;
	unsigned long                    addr_offset;
	unsigned int                     kaddr[2];
	unsigned long                    mfd;
	unsigned long			 dev_fd;
	uint32_t                         buf_property;
	uint32_t                         buf_flag;
	uint32_t			 is_statis_buf_reserved;
	uint32_t			 reserved[4];
};

enum statis_buf_flag {
	STATIS_BUF_FLAG_INIT,
	STATIS_BUF_FLAG_RUNNING,
	STATIS_BUF_FLAG_MAX
};

enum isp_3a_block_id {
	ISP_AEM_BLOCK,
	ISP_AFL_BLOCK,
	ISP_AFM_BLOCK,
	ISP_BINNING_BLOCK,
	ISP_PDAF_BLOCK,
};

enum isp_irq_done_id {
	IRQ_SHADOW_DONE,
	IRQ_DCAM_SOF,
	IRQ_ALL_DONE,
	IRQ_RAW_CAP_DONE,
	IRQ_YNR_DONE,
	IRQ_AEM_STATIS,
	IRQ_AFL_STATIS,
	IRQ_AFM_STATIS,
	IRQ_BINNING_STATIS,
	IRQ_PDAF_STATIS,
	IRQ_MAX_DONE,
};

struct sprd_isp_time {
	unsigned int sec;
	unsigned int usec;
};

struct isp_irq_info {
	unsigned int irq_type;
	unsigned int irq_flag;
	unsigned int format;
	unsigned int channel_id;
	unsigned int base_id;
	unsigned int img_id;
	unsigned int irq_id;
	unsigned int sensor_id;
	unsigned long yaddr;
	unsigned long uaddr;
	unsigned long vaddr;
	unsigned long yaddr_vir;
	unsigned long uaddr_vir;
	unsigned long vaddr_vir;
	unsigned int img_y_fd;
	unsigned int img_u_fd;
	unsigned int img_v_fd;
	unsigned long length;
	struct isp_img_size buf_size;
	struct sprd_isp_time time_stamp;
	struct isp_statis_frame_output isp_statis_output;
	unsigned int frm_index;
};

/* add for compile problem*/
struct isp_dev_lsc_info {
	unsigned int bypass;
	unsigned int grid_pitch;
	unsigned int grid_mode;
	unsigned int endian;
	unsigned int buf_addr[2];
	unsigned int buf_len;
};

/*awbm*/
struct awbm_rect_pos {
	unsigned int start_x[5];
	unsigned int start_y[5];
	unsigned int end_x[5];
	unsigned int end_y[5];
};

struct awbm_circle_pos {
	unsigned int x[5];
	unsigned int y[5];
	unsigned int r[5];
};

struct awbm_pixel_num {
	unsigned int pixel_num[5];
};

struct awbm_thr {
	unsigned int r_high;
	unsigned int r_low;
	unsigned int g_high;
	unsigned int g_low;
	unsigned int b_high;
	unsigned int b_low;
};

struct isp_dev_awb_info_v2 {
	/*AWBM*/
	unsigned int awbm_bypass;
	unsigned int mode;
	unsigned int skip_num;
	struct img_offset block_offset;
	struct isp_img_size block_size;
	unsigned int shift;
	unsigned int thr_bypass;
	struct awbm_rect_pos rect_pos;
	struct awbm_circle_pos circle_pos;
	struct awbm_rect_pos clctor_pos;
	struct awbm_pixel_num pix_num;
	struct awbm_thr thr;
	struct isp_img_size slice_size;
	unsigned int skip_num_clear;
	unsigned int position_sel;
	unsigned int mem_addr;
	/*AWBC*/
	unsigned int awbc_bypass;
	unsigned int alpha_bypass;
	unsigned int alpha_value;
	unsigned int buf_sel;
	struct awbc_param gain;
	struct awbc_rgb thrd;
	struct awbc_param gain_offset;
	struct awbc_param gain_buff;
	struct awbc_param gain_offset_buff;
};

struct isp_dev_pre_wavelet_info {
	unsigned int bypass;
	unsigned int radial_bypass;
	unsigned int gain_thrs0;
	unsigned int gain_thrs1;
	unsigned int bitshift0;
	unsigned int bitshift1;
	unsigned int offset;
	unsigned int nsr_slope;
	unsigned int lum_ratio;
	unsigned int center_pos_x;
	unsigned int center_pos_y;
	unsigned int delta_x2;
	unsigned int delta_y2;
	unsigned int r2_thr;
	unsigned int p_param1;
	unsigned int p_param2;
	unsigned int addback;
	unsigned int gain_max_thr;
	unsigned int pos_x;
	unsigned int pos_y;
	unsigned int lum_shink_level;
};

struct isp_dev_bdn_info {
	unsigned int bypass;
	unsigned int radial_bypass;
	unsigned int addback;
	unsigned int dis[10][2];
	unsigned int ran[10][8];
	unsigned int offset_x;
	unsigned int offset_y;
	unsigned int squ_x2;
	unsigned int squ_y2;
	unsigned int coef;
	unsigned int coef2;
	unsigned int start_pos_x;
	unsigned int start_pos_y;
	unsigned int offset;
	unsigned int dis_level;
	unsigned int ran_level;
};

struct isp_dev_nlm_info_v2 {
	unsigned int  bypass;
	unsigned int  imp_opt_bypass;
	unsigned int  flat_opt_bypass;
	unsigned int  buf_sel;
	unsigned char strength[5];
	unsigned char cnt[5];
	unsigned short  thresh[5];
	unsigned int  streng_th;
	unsigned int  texture_dec;
	unsigned int  is_flat;
	unsigned int  addback;
	unsigned short  lut_w[72];
	unsigned int  vst_addr[2];
	unsigned int  vst_len;
	unsigned int  ivst_addr[2];
	unsigned int  ivst_len;
	unsigned int  nlm_addr[2];
	unsigned int  nlm_len;
	unsigned int  strength_level;

};

struct isp_dev_cmc8_info {
	unsigned int  bypass;
	unsigned int  buf_sel;
	unsigned int  alpha;
	unsigned int  alpha_bypass;
	struct cmc_matrix matrix;
	struct cmc_matrix matrix_buf;
};


struct isp_dev_rgb_gain2_info {
	unsigned int  bypass;
	unsigned int  r_gain;
	unsigned int  g_gain;
	unsigned int  b_gain;
	unsigned int  r_offset;
	unsigned int  g_offset;
	unsigned int  b_offset;
};

struct isp_dev_ct_info {
	unsigned int  bypass;
	unsigned int  data_ptr[2];
	unsigned int  size;
};

struct isp_dev_csc_info {
	unsigned int  bypass;
	unsigned int  red_centre_x;
	unsigned int  red_centre_y;
	unsigned int  blue_centre_x;
	unsigned int  blue_centre_y;
	unsigned int  red_x2_init;
	unsigned int  red_y2_init;
	unsigned int  blue_x2_init;
	unsigned int  blue_y2_init;
	unsigned int  red_threshold;
	unsigned int  blue_threshold;
	unsigned int  red_p1_param;
	unsigned int  red_p2_param;
	unsigned int  blue_p1_param;
	unsigned int  blue_p2_param;
	unsigned int  max_gain_thr;
	struct isp_img_size img_size;
	struct img_offset start_pos;
};

struct isp_dev_css_info {
	unsigned int  bypass;
	unsigned int  lh_chrom_th;
	unsigned char  chrom_lower_th[7];
	unsigned char  reserved0;
	unsigned char  chrom_high_th[7];
	unsigned char  reserved1;
	unsigned int  lum_low_shift;
	unsigned int  lum_hig_shift;
	unsigned char  lh_ratio[8];
	unsigned char  ratio[8];
	unsigned int  lum_low_th;
	unsigned int  lum_ll_th;
	unsigned int  lum_hig_th;
	unsigned int  lum_hh_th;
	unsigned int  u_th_0_l;
	unsigned int  u_th_0_h;
	unsigned int  v_th_0_l;
	unsigned int  v_th_0_h;
	unsigned int  u_th_1_l;
	unsigned int  u_th_1_h;
	unsigned int  v_th_1_l;
	unsigned int  v_th_1_h;
	unsigned int  cutoff_th;
};

struct isp_dev_emboss_info {
	unsigned int  y_bypass;
	unsigned int  uv_bypass;
	unsigned int  y_step;
	unsigned int  uv_step;
};

struct isp_dev_yrandom_info {
	unsigned int  bypass;
	unsigned int  seed;
	unsigned int  mode;
	unsigned int  init;
	unsigned int  offset;
	unsigned int  shift;
	unsigned int  takeBit[8];
};

struct isp_dev_prefilter_info {
	unsigned int  bypass;
	unsigned int  writeback;
	unsigned int  thrd;
};

struct afm_subfilter {
	unsigned int  average;
	unsigned int  median;
};

struct afm_shift {
	unsigned int  shift_spsmd;
	unsigned int  shift_sobel5;
	unsigned int  shift_sobel9;
};

struct afm_thrd_rgb {
	unsigned int  sobel5_thr_min_red;
	unsigned int  sobel5_thr_max_red;
	unsigned int  sobel5_thr_min_green;
	unsigned int  sobel5_thr_max_green;
	unsigned int  sobel5_thr_min_blue;
	unsigned int  sobel5_thr_max_blue;
	unsigned int  sobel9_thr_min_red;
	unsigned int  sobel9_thr_max_red;
	unsigned int  sobel9_thr_min_green;
	unsigned int  sobel9_thr_max_green;
	unsigned int  sobel9_thr_min_blue;
	unsigned int  sobel9_thr_max_blue;
	unsigned int  spsmd_thr_min_red;
	unsigned int  spsmd_thr_max_red;
	unsigned int  spsmd_thr_min_green;
	unsigned int  spsmd_thr_max_green;
	unsigned int  spsmd_thr_min_blue;
	unsigned int  spsmd_thr_max_blue;
};

struct isp_dev_yiq_aem_info {
	unsigned int  ygamma_bypass;
	short  gamma_xnode[10];
	short  gamma_ynode[10];
	unsigned int  gamma_node_idx[10];
	unsigned int  aem_bypass;
	unsigned int  aem_mode;
	unsigned int  aem_skip_num;
	unsigned int  offset_x;
	unsigned int  offset_y;
	unsigned int  width;
	unsigned int  height;
};

struct isp_dev_yiq_aem_info_v2 {
	unsigned int  ygamma_bypass;
	unsigned int  gamma_xnode[10];
	unsigned int  gamma_ynode[10];
	unsigned int  gamma_node_idx[10];
	unsigned int  aem_bypass;
	unsigned int  aem_mode;
	unsigned int  aem_skip_num;
	unsigned int  offset_x;
	unsigned int  offset_y;
	unsigned int  avgshift;
	unsigned int  width;
	unsigned int  height;
};

struct isp_dev_yiq_afm_info {
	unsigned int  bypass;
	unsigned int  mode;
	unsigned int  source_pos;
	unsigned int  shift;
	unsigned int  skip_num;
	unsigned int  skip_num_clear;
	unsigned int  format;
	unsigned int  iir_bypass;
	struct isp_coord coord[25];
	short IIR_c[11];
	unsigned short  reserved;
};

struct isp_dev_yiq_afm_info_v2 {
	unsigned int  bypass;
	unsigned int  mode;
	unsigned int  shift;
	unsigned int  skip_num;
	unsigned int  skip_num_clear;
	unsigned int  af_position;
	unsigned int  format;
	unsigned int  iir_bypass;
	struct isp_coord coord[25];
	int IIR_c[11];
};

struct isp_time {
	unsigned int  sec;
	unsigned int  usec;
};

struct isp_irq {
	unsigned int  irq_val0;
	unsigned int  irq_val1;
	unsigned int  irq_val2;
	unsigned int  irq_val3;
	unsigned int  reserved;
	int32_t ret_val;
	struct isp_time time;
};

struct isp_edge_thrd {
	unsigned int  detail;
	unsigned int  smooth;
	unsigned int  strength;
};

struct isp_capability {
	unsigned int  isp_id;
	unsigned int                      index;
	void __user *property_param;
};

struct isp_cce_matrix_tab {
	unsigned short  matrix[ISP_CCE_MATRIX_TAB_MAX];
	unsigned short  reserved;
};

struct isp_cce_shift {
	unsigned int  y_shift;
	unsigned int  u_shift;
	unsigned int  v_shift;
};

struct isp_cce_uvd {
	unsigned char uvd[ISP_CCE_UVD_NUM];
	unsigned char reserved;
};

struct isp_cce_uvc {
	unsigned char uvc0[ISP_CCE_UVC0_NUM];
	unsigned char reserved0;
	unsigned char reserved1;
	unsigned char uvc1[ISP_CCE_UVC1_NUM];
	unsigned char reserved2;
};

struct isp_awbm_statistics {
	unsigned int  r[ISP_AWBM_ITEM];
	unsigned int  g[ISP_AWBM_ITEM];
	unsigned int  b[ISP_AWBM_ITEM];
};

struct isp_awbc_rgb {
	unsigned int  r;
	unsigned int  g;
	unsigned int  b;
};

struct isp_raw_awbm_statistics {
	unsigned int  num0;
	unsigned int  num1;
	unsigned int  num2;
	unsigned int  num3;
	unsigned int  num4;
	unsigned int  num_t;
	unsigned int  block_r;
	unsigned int  block_g;
	unsigned int  block_b;
};

struct isp_fetch_endian {
	unsigned int  endian;
	unsigned int  bit_recorder;
};

struct isp_bayer_mode {
	unsigned int  nlc_bayer;
	unsigned int  awbc_bayer;
	unsigned int  wave_bayer;
	unsigned int  cfa_bayer;
	unsigned int  gain_bayer;
};

struct isp_cfa_thrd {
	unsigned int  edge;
	unsigned int  ctrl;
};

struct isp_pitch {
	unsigned int  chn0;
	unsigned int  chn1;
	unsigned int  chn2;
};

struct isp_grgb_thrd {
	unsigned int  edge;
	unsigned int  diff;
};

struct isp_raw_aem_statistics {
	unsigned int  r[ISP_RAW_AEM_ITEM];
	unsigned int  g[ISP_RAW_AEM_ITEM];
	unsigned int  b[ISP_RAW_AEM_ITEM];
};

struct isp_bpc_common {
	unsigned int  pattern_type;
	unsigned int  detect_thrd;
	unsigned int  super_bad_thrd;
};

struct isp_bpc_thrd {
	unsigned int  flat;
	unsigned int  std;
	unsigned int  texture;
};

struct isp_interrupt {
	unsigned int  isp_id;
	unsigned int  int_mode;
};

struct isp_reg_bits {
	unsigned long reg_addr;
	unsigned long reg_value;
};

struct isp_reg_param {
	unsigned long reg_param;
	unsigned int  counts;
};

struct thrd_min_max	{
	unsigned int  min;
	unsigned int  max;
};

struct isp_dev_rgb_afm_info {
	unsigned int  bypass;
	unsigned int  mode;
	unsigned int  skip_num;
	unsigned int  data_update_sel;
	unsigned int  iir_eb;
	unsigned int  overflow_protect_en;
	unsigned int  touch_mode;
	struct isp_img_size frame_size;
	struct isp_coord win[ISP_AFM_WIN_NUM];
	unsigned short  iir_g0;
	unsigned short  iir_g1;
	unsigned short  iir_c[10];
	unsigned int  channel_sel;
	unsigned int  fv0_enhance_mode;
	unsigned int  fv1_enhance_mode;
	unsigned int  denoise_mode;
	unsigned int  center_weight;
	unsigned int  clip_en0;
	unsigned int  clip_en1;
	unsigned int  fv0_shift;
	unsigned int  fv1_shift;
	struct thrd_min_max fv0_th;
	struct thrd_min_max fv1_th;
	unsigned int  fv1_coeff[4][9];
};

struct af_enhanced_module_info {
	unsigned char chl_sel;
	unsigned char nr_mode;
	unsigned char center_weight;
	unsigned char fv_enhanced_mode[2];
	unsigned char clip_en[2];
	unsigned int max_th[2];
	unsigned int min_th[2];
	unsigned char fv_shift[2];
	char fv1_coeff[36];
};

struct af_iir_nr_info {
	unsigned char iir_nr_en;
	short iir_g0;
	short iir_c1;
	short iir_c2;
	short iir_c3;
	short iir_c4;
	short iir_c5;
	short iir_g1;
	short iir_c6;
	short iir_c7;
	short iir_c8;
	short iir_c9;
	short iir_c10;
};


/*isp block int*/
struct skip_ctrl_param {
	unsigned int sdw_done_int_cnt_num;
	unsigned int sdw_down_skip_cnt_num;
	unsigned int all_done_int_cnt_num;
	unsigned int all_done_skip_cnt_num;
};

struct skip_ctrl1_param {
	unsigned int vid_done_int_cnt_num;
	unsigned int vid_done_skip_cnt_num;
};

enum SCINFO_COLOR_ORDER {
	COLOR_ORDER_RG = 0,
	COLOR_ORDER_GR,
	COLOR_ORDER_GB,
	COLOR_ORDER_BG
};

struct isp_raw_proc_info {
	struct isp_img_size in_size;
	struct isp_img_size out_size;
	struct isp_addr img_vir;
	struct isp_addr img_offset;
	unsigned int img_fd;
	unsigned int sensor_id;
};

struct isp_3dnr_const_param {
	unsigned int fusion_mode;
	unsigned int filter_switch;
	unsigned int y_pixel_noise_threshold;
	unsigned int u_pixel_noise_threshold;
	unsigned int v_pixel_noise_threshold;
	unsigned int y_pixel_noise_weight;
	unsigned int u_pixel_noise_weight;
	unsigned int v_pixel_noise_weight;
	unsigned int threshold_radial_variation_u_range_min;
	unsigned int threshold_radial_variation_u_range_max;
	unsigned int threshold_radial_variation_v_range_min;
	unsigned int threshold_radial_variation_v_range_max;
	unsigned int y_threshold_polyline_0;
	unsigned int y_threshold_polyline_1;
	unsigned int y_threshold_polyline_2;
	unsigned int y_threshold_polyline_3;
	unsigned int y_threshold_polyline_4;
	unsigned int y_threshold_polyline_5;
	unsigned int y_threshold_polyline_6;
	unsigned int y_threshold_polyline_7;
	unsigned int y_threshold_polyline_8;
	unsigned int u_threshold_polyline_0;
	unsigned int u_threshold_polyline_1;
	unsigned int u_threshold_polyline_2;
	unsigned int u_threshold_polyline_3;
	unsigned int u_threshold_polyline_4;
	unsigned int u_threshold_polyline_5;
	unsigned int u_threshold_polyline_6;
	unsigned int u_threshold_polyline_7;
	unsigned int u_threshold_polyline_8;
	unsigned int v_threshold_polyline_0;
	unsigned int v_threshold_polyline_1;
	unsigned int v_threshold_polyline_2;
	unsigned int v_threshold_polyline_3;
	unsigned int v_threshold_polyline_4;
	unsigned int v_threshold_polyline_5;
	unsigned int v_threshold_polyline_6;
	unsigned int v_threshold_polyline_7;
	unsigned int v_threshold_polyline_8;
	unsigned int y_intensity_gain_polyline_0;
	unsigned int y_intensity_gain_polyline_1;
	unsigned int y_intensity_gain_polyline_2;
	unsigned int y_intensity_gain_polyline_3;
	unsigned int y_intensity_gain_polyline_4;
	unsigned int y_intensity_gain_polyline_5;
	unsigned int y_intensity_gain_polyline_6;
	unsigned int y_intensity_gain_polyline_7;
	unsigned int y_intensity_gain_polyline_8;
	unsigned int u_intensity_gain_polyline_0;
	unsigned int u_intensity_gain_polyline_1;
	unsigned int u_intensity_gain_polyline_2;
	unsigned int u_intensity_gain_polyline_3;
	unsigned int u_intensity_gain_polyline_4;
	unsigned int u_intensity_gain_polyline_5;
	unsigned int u_intensity_gain_polyline_6;
	unsigned int u_intensity_gain_polyline_7;
	unsigned int u_intensity_gain_polyline_8;
	unsigned int v_intensity_gain_polyline_0;
	unsigned int v_intensity_gain_polyline_1;
	unsigned int v_intensity_gain_polyline_2;
	unsigned int v_intensity_gain_polyline_3;
	unsigned int v_intensity_gain_polyline_4;
	unsigned int v_intensity_gain_polyline_5;
	unsigned int v_intensity_gain_polyline_6;
	unsigned int v_intensity_gain_polyline_7;
	unsigned int v_intensity_gain_polyline_8;
	unsigned int gradient_weight_polyline_0;
	unsigned int gradient_weight_polyline_1;
	unsigned int gradient_weight_polyline_2;
	unsigned int gradient_weight_polyline_3;
	unsigned int gradient_weight_polyline_4;
	unsigned int gradient_weight_polyline_5;
	unsigned int gradient_weight_polyline_6;
	unsigned int gradient_weight_polyline_7;
	unsigned int gradient_weight_polyline_8;
	unsigned int gradient_weight_polyline_9;
	unsigned int gradient_weight_polyline_10;
	unsigned int y_pixel_src_weight[4];
	unsigned int u_pixel_src_weight[4];
	unsigned int v_pixel_src_weight[4];
	unsigned int u_threshold_factor[4];
	unsigned int v_threshold_factor[4];
	unsigned int u_divisor_factor[4];
	unsigned int v_divisor_factor[4];
	unsigned int r1_circle;
	unsigned int r2_circle;
	unsigned int r3_circle;
};

#endif
