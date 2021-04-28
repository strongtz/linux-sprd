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

/* Five 64-bit data for one block */
/* AEM_HOR_BLK_NUM * AEM_VER_BLK_NUM * 5 * 8 byte*/
#define ISP_AEM_STATIS_BUF_SIZE            BUF_ALIGN(0xA0000)
#define ISP_AEM_STATIS_BUF_NUM             4

/* Three 64-bit data for two block */
/* AFM_HOR_WIN_NUM * AFM_VER_WIN_NUM * 3/2 * 8 byte*/
#define ISP_AFM_STATIS_BUF_SIZE            BUF_ALIGN(0xE10)
#define ISP_AFM_STATIS_BUF_NUM             4

/* 240 64-bit global data for one frame */
/* 964 64-bit region & flatness data for one frame*/
/* (240 + 964) * 8 byte*/
#define ISP_AFL_STATIS_BUF_SIZE            BUF_ALIGN(0x25A0 * 15)
#define ISP_AFL_STATIS_BUF_NUM             4

#define ISP_PDAF_STATIS_BUF_SIZE           BUF_ALIGN(0x98000)
#define ISP_PDAF_STATIS_BUF_NUM            4
#define ISP_EBD_STATIS_BUF_SIZE            BUF_ALIGN(0x400*15)
#define ISP_EBD_STATIS_BUF_NUM             4
#define ISP_HIST_ITEMS                     256
/* 256 * 8 byte=0x800*/
#define ISP_HIST_STATIS_BUF_SIZE           BUF_ALIGN(0x800)
#define ISP_HIST_STATIS_BUF_NUM            4

#define ISP_PINGPANG_FRGB_GAMC_NODE        129
#define ISP_PINGPANG_FRGB_GAMC_NUM         257
#define NR3_BUF_NUM                        8
#define POSTERIZE_NUM                      8
#define ISP_PINGPANG_YUV_YGAMMA_NUM        129
#define ISP_CCE_MATRIX_TAB_MAX             9
#define ISP_CCE_UVD_NUM                    7
#define ISP_CCE_UVC0_NUM                   2
#define ISP_CCE_UVC1_NUM                   3
#define ISP_AWBM_ITEM                      1024
#define ISP_RAW_AEM_ITEM                   1024
#define ISP_AFM_WIN_NUM                    10
#define ISP_VST_IVST_NUM                   1024
#define PDAF_PPI_NUM                       64
#define PDAF_CORRECT_GAIN_NUM              128
#define v_counter_interval                 524288

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
	ISP_BLOCK_EBD,
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

enum isp_anti_flicker_new_property {
	ISP_PRO_ANTI_FLICKER_NEW_BLOCK,
	ISP_PRO_ANTI_FLICKER_NEW_BYPASS,
};

enum isp_arbiter_property {
	ISP_PRO_ARBITER_BLOCK,
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

/*isp sub block: iircnr*/
enum isp_iircnr_property {
	ISP_PRO_IIRCNR_BLOCK,
};

/*isp sub block: dispatch_yuv*/
enum isp_dispatch_yuv_property {
	ISP_PRO_DISPATCH_YUV_BLOCK,
};

/*isp sub block: dispatch_fetch*/
enum isp_fetch_property {
	ISP_PRO_FETCH_BLOCK,
	ISP_PRO_FETCH_START,
	ISP_PRO_FETCH_SLICE_SIZE,
	ISP_PRO_FETCH_TRANSADDR,
	ISP_PRO_FETCH_RAW_BLOCK,
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

/*isp sub block: yrandom*/
enum isp_yrandom_property {
	ISP_PRO_YRANDOM_BLOCK,
	ISP_PRO_YRANDOM_CHK_SUM_CLR,
	ISP_PRO_YRANDOM_INIT,
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
	ISP_PRO_YNR_SLICE,
};

/*isp sub block: pdaf*/
enum isp_pdaf_property {
	ISP_PRO_PDAF_BLOCK,
	ISP_PRO_PDAF_BYPASS,
	ISP_PRO_PDAF_SET_MODE,
	ISP_PRO_PDAF_SET_SKIP_NUM,
	ISP_PRO_PDAF_SET_ROI,
	ISP_PRO_PDAF_SET_PPI_INFO,
	ISP_PRO_PDAF_TYPE1_BLOCK,
	ISP_PRO_PDAF_TYPE2_BLOCK,
	ISP_PRO_EBD_BLOCK,
};

/*isp sub block: 3dnr_me*/
enum isp_3dnr_me_property {
	ISP_PRO_3DNR_ME_BLOCK,
	ISP_PRO_3DNR_ME_BYPASS,
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
	ISP_PRO_RAW_AEM_BLK_NUM,
	ISP_PRO_RAW_AEM_RGB_THR,
	ISP_PRO_RAW_AEM_SKIP_NUM_CLR,
};

enum isp_raw_af_property {
	ISP_PRO_RGB_AFM_BYPASS,
	ISP_PRO_RGB_AFM_BLOCK,
	ISP_PRO_RGB_AFM_WIN,
	ISP_PRO_RGB_AFM_WIN_NUM,
	ISP_PRO_RGB_AFM_MODE,
	ISP_PRO_RGB_AFM_SKIP_NUM,
	ISP_PRO_RGB_AFM_SKIP_NUM_CLR,
	ISP_PRO_RGB_AFM_CROP_EB,
	ISP_PRO_RGB_AFM_CROP_SIZE,
	ISP_PRO_RGB_AFM_DONE_TILE_NUM,
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
	uint32_t isp_id;
	uint32_t scene_id;
	uint32_t sub_block;
	uint32_t property;
	void __user *property_param;
};

struct isp_addr {
	unsigned long chn0;
	unsigned long chn1;
	unsigned long chn2;
};

struct isp_img_size {
	uint32_t width;
	uint32_t height;
};

struct isp_lsc_addr {
	uint32_t phys_addr;
	uint32_t virt_addr;
	uint32_t buf_len;
};

struct isp_coord {
	uint32_t start_x;
	uint32_t start_y;
	uint32_t end_x;
	uint32_t end_y;
};
struct img_offset {
	uint32_t x;
	uint32_t y;
	uint32_t Z;
};

struct awbc_param {
	uint32_t r;
	uint32_t b;
	uint32_t gr;
	uint32_t gb;
};

struct awbc_rgb {
	uint32_t r;
	uint32_t g;
	uint32_t b;
};

struct cmc_matrix {
	unsigned short val[9];
	unsigned short reserved;
};

struct isp_common_lbuf_param {
	uint32_t comm_lbuf_offset;
	uint32_t ydly_lbuf_offset;
};

struct isp_common_sdw_ctrl {
	uint32_t comm_cfg_rdy;
	uint32_t shadow_mctrl;
	uint32_t sdw_ctrl;
};

struct isp_range_thr {
	uint32_t high;
	uint32_t low;
	uint32_t middle;
};

struct isp_img_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct isp_dev_raw_aem_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t skip_num;
	struct img_offset offset;
	struct isp_img_size blk_num;
	uint32_t aem_h_avgshf;
	uint32_t aem_l_avgshf;
	uint32_t aem_m_avgshf;
	struct isp_img_size blk_size;
	struct awbc_rgb rgb_thr[2];
};

struct yuv_param {
	uint32_t y;
	uint32_t u;
	uint32_t v;
};

struct uv_param {
	uint32_t u;
	uint32_t v;
};

struct isp_img_addr {
	unsigned long y_phy_addr;
	unsigned long uv_phy_addr;
};

struct isp_dev_1d_lsc_info {
	uint32_t bypass;
	uint32_t buf_sel;
	uint32_t radius_step;
	uint32_t center_r0c0_row_y;
	uint32_t center_r0c0_col_x;
	uint32_t center_r0c1_row_y;
	uint32_t center_r0c1_col_x;
	uint32_t center_r1c0_row_y;
	uint32_t center_r1c0_col_x;
	uint32_t center_r1c1_row_y;
	uint32_t center_r1c1_col_x;
	uint32_t init_r_r0c1;
	uint32_t init_r_r0c0;
	uint32_t init_r_r1c1;
	uint32_t init_r_r1c0;
	uint32_t init_r2_r0c0;
	uint32_t init_r2_r0c1;
	uint32_t init_r2_r1c0;
	uint32_t init_r2_r1c1;
	uint32_t init_dr2_r0c0;
	uint32_t init_dr2_r0c1;
	uint32_t init_dr2_r1c0;
	uint32_t init_dr2_r1c1;
	uint32_t data_ptr[2];/*compatible with 64bit cpu*/
};

struct lnc_1d_slice_param {
	uint32_t start_col;
	uint32_t start_row;
	uint32_t r[4];
	uint32_t r2[4];
	uint32_t dr2[4];
};

struct isp_dev_2d_lsc_info {
	uint32_t bypass;
	uint32_t offset_x;
	uint32_t offset_y;
	uint32_t loader_enable;
	uint32_t grid_width;
	uint32_t grid_x_num;
	uint32_t grid_y_num;
	uint32_t grid_num_t;
	uint32_t endian;
	uint32_t relative_x;
	uint32_t relative_y;
	uint32_t q_value[2][5];
	uint32_t buf_addr[2];
	uint32_t buf_len;
	uint32_t weight_num;
	uint32_t data_ptr[2];
};

struct isp_dev_anti_flicker_new_info {
	uint32_t bypass;
	uint32_t bayer2y_mode;
	uint32_t bayer2y_chanel;
	uint32_t mode;
	uint32_t skip_frame_num;
	uint32_t afl_stepx;
	uint32_t afl_stepy;
	uint32_t frame_num;
	uint32_t afl_chk_auto_clr;
	uint32_t start_col;
	uint32_t end_col;
	uint32_t step_x_region;
	uint32_t step_y_region;
	uint32_t step_x_start_region;
	uint32_t step_x_end_region;
	struct isp_img_size img_size;
};

struct isp_dev_anti_flicker_info {
	unsigned int bypass;
	unsigned int mode;
	unsigned int skip_frame_num;
	unsigned int line_step;
	unsigned int frame_num;
	unsigned int vheight;
	unsigned int start_col;
	unsigned int end_col;
	unsigned int afl_total_num;
	struct isp_img_size img_size;
};

struct isp_dev_3dnr_me_info {
	uint32_t bypass;
	uint32_t ping_pong_en;
	uint32_t project_mode;
	uint32_t channel_sel;
	struct isp_img_rect roi;
};

struct isp_dev_arbiter_info {
	uint32_t fetch_raw_endian;
	uint32_t fetch_bit_reorder;
	uint32_t fetch_raw_word_change;
};

struct isp_dev_awb_info {
	uint32_t awbc_bypass;
	struct awbc_param gain;
	struct awbc_rgb thrd;
	struct awbc_param gain_offset;
};

struct isp_dev_blc_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t r;
	uint32_t b;
	uint32_t gr;
	uint32_t gb;
};

struct isp_dev_bpc_info {
	uint32_t bpc_gc_cg_dis;
	uint32_t bpc_mode_en;
	uint32_t bpc_mode_en_gc;
	uint32_t bypass;
	uint32_t double_bypass;
	uint32_t three_bypass;
	uint32_t four_bypass;
	uint32_t bpc_mode;
	uint32_t pos_out_continue_mode;
	uint32_t is_mono_sensor;
	uint32_t edge_hv_mode;
	uint32_t edge_rd_mode;
	uint32_t pos_out_skip_num;
	uint32_t rd_retain_num;
	uint32_t rd_max_len_sel;
	uint32_t wr_max_len_sel;
	uint32_t bpc_blk_mode;
	uint32_t double_badpixel_th[4];
	uint32_t three_badpixel_th[4];
	uint32_t four_badpixel_th[4];
	uint32_t texture_th;
	uint32_t flat_th;
	uint32_t shift[3];
	uint32_t edge_ratio_hv;
	uint32_t edge_ratio_rd;
	uint32_t high_offset;
	uint32_t low_offset;
	uint32_t high_coeff;
	uint32_t low_coeff;
	uint32_t min_coeff;
	uint32_t max_coeff;
	unsigned short intercept_b[8];
	unsigned short slope_k[8];
	unsigned short lut_level[8];
	uint32_t map_addr;
	uint32_t bad_map_hw_fifo_clr_en;
	uint32_t bad_pixel_num;
	uint32_t bad_pixel_pos_out_addr;
};

struct isp_dev_brightness_info {
	uint32_t bypass;
	uint32_t factor;
};

struct isp_dev_cce_info {
	uint32_t bypass;
	unsigned short matrix[9];
	unsigned short y_offset;
	unsigned short u_offset;
	unsigned short v_offset;
};

struct isp_dev_yuv_cdn_info {
	uint32_t bypass;
	uint32_t filter_bypass;
	uint32_t median_writeback_en;
	uint32_t median_mode;
	uint32_t gaussian_mode;
	uint32_t median_thr;
	uint32_t median_thru0;
	uint32_t median_thru1;
	uint32_t median_thrv0;
	uint32_t median_thrv1;
	uint32_t rangewu[31];
	uint32_t rangewv[31];
	uint32_t level;
};

struct isp_dev_cfa_info {
	uint32_t bypass;
	uint32_t css_bypass;
	uint32_t grid_thr;
	uint32_t min_grid_new;
	uint32_t grid_gain_new;
	uint32_t strong_edge_thr;
	uint32_t uni_dir_intplt_thr_new;
	uint32_t weight_control_bypass;
	uint32_t cdcr_adj_factor;
	uint32_t smooth_area_thr;
	uint32_t readblue_high_sat_thr;
	uint32_t grid_dir_weight_t1;
	uint32_t grid_dir_weight_t2;
	uint32_t round_diff_03_thr;
	uint32_t low_lux_03_thr;
	uint32_t round_diff_12_thr;
	uint32_t low_lux_12_thr;
	uint32_t css_weak_edge_thr;
	uint32_t css_edge_thr;
	uint32_t css_texture1_thr;
	uint32_t css_texture2_thr;
	uint32_t css_uv_val_thr;
	uint32_t css_uv_diff_thr;
	uint32_t css_gray_thr;
	uint32_t css_pix_similar_thr;
	uint32_t css_green_edge_thr;
	uint32_t css_green_weak_edge_thr;
	uint32_t css_green_tex1_thr;
	uint32_t css_green_tex2_thr;
	uint32_t css_green_flat_thr;
	uint32_t css_edge_corr_ratio_r;
	uint32_t css_edge_corr_ratio_b;
	uint32_t css_text1_corr_ratio_r;
	uint32_t css_text1_corr_ratio_b;
	uint32_t css_text2_corr_ratio_r;
	uint32_t css_text2_corr_ratio_b;
	uint32_t css_flat_corr_ratio_r;
	uint32_t css_flat_corr_ratio_b;
	uint32_t css_wedge_corr_ratio_r;
	uint32_t css_wedge_corr_ratio_b;
	uint32_t css_alpha_for_tex2;
	uint32_t css_skin_u_top[2];
	uint32_t css_skin_u_down[2];
	uint32_t css_skin_v_top[2];
	uint32_t css_skin_v_down[2];
};

struct isp_dev_cmc10_info {
	uint32_t bypass;
	struct cmc_matrix matrix;
};

struct isp_dev_common_info {
	uint32_t fetch_sel_0;
	uint32_t store_sel_0;
	uint32_t fetch_sel_1;
	uint32_t store_sel_1;
	uint32_t fetch_color_space_sel;
	uint32_t store_color_space_sel;
	uint32_t ch0_path_ctrl;
	uint32_t bin_pos_sel;
	uint32_t ram_mask;
	uint32_t gclk_ctrl_rrgb;
	uint32_t gclk_ctrl_yiq_frgb;
	uint32_t gclk_ctrl_yuv;
	uint32_t gclk_ctrl_scaler_3dnr;
	struct isp_common_lbuf_param lbuf_off;
	struct isp_common_sdw_ctrl shadow_ctrl_ch0;
	struct isp_common_sdw_ctrl shadow_ctrl_ch1;
	uint32_t afl_version_sel_ch0;
	uint32_t res[3];
	uint32_t yuv_disp_path_sel_sdw_en;
	uint32_t store_cce_path_sel;
	uint32_t store_cce_en;
	uint32_t yuv_disp_path_sel_ch0;
	uint32_t yuv_disp_path_sel_ch1;
	uint32_t scl_pre_path_sel;
	uint32_t scl_vid_path_sel;
	uint32_t scl_cap_path_sel;
	uint32_t store_out_path_sel;
	uint32_t jpg_frame_done_en;
	uint32_t jpg_frame_done_clr;
	uint32_t jpg_line_done_en;
	uint32_t isp_soft_rst;
	uint32_t isp_cfg_sof_rst;
	uint32_t fetch_color_format;
	uint32_t store_color_format;
};

struct isp_dev_contrast_info {
	uint32_t bypass;
	uint32_t factor;
};

struct csa_factor {
	uint32_t bypass;
};

struct isp_dev_csa_info {
	uint32_t bypass;
	uint32_t factor_u;
	uint32_t factor_v;
	struct csa_factor factor;
};

/*isp sub block: dispatch*/
struct isp_dev_dispatch_info {
	uint32_t bayer;
	struct isp_img_size size;
	uint32_t dispatch_height_dly_num_ch0;
	uint32_t dispatch_width_dly_num_ch0;
	uint32_t dispatch_dbg_mode_ch0;
	uint32_t dispatch_ready_width_ch0;
	uint32_t dispatch_nready_cfg_ch0;
	uint32_t dispatch_nready_width_ch0;
	uint32_t dispatch_width_flash_mode;
	uint32_t dispatch_width_dly_num_flash;
	uint32_t dispatch_done_cfg_mode;
	uint32_t dispatch_done_line_dly_num;
	uint32_t dispatch_pipe_nfull_num;
	uint32_t dispatch_pipe_flush_num;
	uint32_t dispatch_pipe_hblank_num;
};

struct isp_dispatch_dly_num_info {
	unsigned short width_dly_num_ch0;
	unsigned short height_dly_num_ch0;
};

struct isp_dispatch_hw_ctrl_info {
	unsigned short nready_cfg_ch0;
	unsigned short nready_width_ch0;
};

/*isp sub block: dispatch_yuv*/
struct isp_dev_dispatch_yuv_info {
	uint32_t bayer_ch0;
	struct isp_img_size ch0_size;
	struct isp_img_size ch1_size;
	unsigned short width_dly_num_ch0;
	unsigned short height_dly_num_ch0;
	uint32_t bayer_ch1;
	unsigned short nready_cfg_ch0;
	unsigned short nready_width_ch0;
	unsigned short nready_cfg_ch1;
	unsigned short nready_width_ch1;
	unsigned short width_dly_num_ch1;
	unsigned short height_dly_num_ch1;
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
	uint32_t width;
	uint32_t height;
};

struct isp_dispatch_yuv_cap_ctrl_info {
	unsigned short cap_line_scl_dly_cycle_ctrl;
	unsigned short pre_line_scl_dly_cycle_ctrl;
};

struct isp_dispatch_yuv_cap_ctrl2_info {
	unsigned short pre_line_hole_pix_num;
	unsigned short pre_line_whole_cyle;
};

/*isp sub block: edge*/
struct edge_pn_config {
	uint32_t p;
	uint32_t n;
};

struct isp_dev_edge_info {
	uint32_t bypass;
	uint32_t flat_smooth_mode;
	uint32_t edge_smooth_mode;
	struct edge_pn_config ee_str_d;
	uint32_t mode;
	struct edge_pn_config ee_incr_d;
	struct edge_pn_config ee_edge_thr_d;
	struct edge_pn_config ee_corner_sm;
	struct edge_pn_config ee_corner_gain;
	struct edge_pn_config ee_corner_th;
	uint32_t ee_corner_cor;
	uint32_t ee_cv_t[4];
	struct edge_pn_config ee_cv_clip;
	uint32_t ee_cv_r[3];
	uint32_t ipd_bypass;
	uint32_t ipd_mask_mode;
	struct edge_pn_config ipd_less_thr;
	uint32_t ipd_smooth_en;
	struct edge_pn_config ipd_smooth_mode;
	struct edge_pn_config ipd_flat_thr;
	struct edge_pn_config ipd_eq_thr;
	struct edge_pn_config ipd_more_thr;
	struct edge_pn_config ipd_smooth_edge_thr;
	struct edge_pn_config ipd_smooth_edge_diff;
	uint32_t ee_ratio_hv_3;
	uint32_t ee_ratio_hv_5;
	uint32_t ee_ratio_diag_3;
	uint32_t ee_weight_hv2diag;
	uint32_t ee_gradient_computation_type;
	uint32_t ee_weight_diag2hv;
	uint32_t ee_gain_hv_t[2][4];
	uint32_t ee_gain_hv_r[2][3];
	uint32_t ee_ratio_diag_5;
	uint32_t ee_gain_diag_t[2][4];
	uint32_t ee_gain_diag_r[2][3];
	uint32_t ee_lum_t[4];
	uint32_t ee_lum_r[3];
	uint32_t ee_pos_t[4];
	uint32_t ee_pos_r[3];
	uint32_t ee_pos_c[3];
	uint32_t ee_neg_t[4];
	uint32_t ee_neg_r[3];
	uint32_t ee_neg_c[3];
	uint32_t ee_freq_t[4];
	uint32_t ee_freq_r[3];
};

/*isp sub block: fetch*/
struct isp_addr_fs {
	uint32_t chn0;
	uint32_t chn1;
	uint32_t chn2;
};

struct isp_pitch_fs {
	uint32_t chn0;
	uint32_t chn1;
	uint32_t chn2;
};

struct isp_dev_block_addr {
	struct img_offset offset;
	uint32_t img_fd;
};

struct isp_dev_fetch_info {
	uint32_t bypass;
	uint32_t color_format;
	uint32_t pitch[3];
	uint32_t mipi_word_num;
	uint32_t mipi_byte_rel_pos;
	struct isp_img_size size;
	struct isp_dev_block_addr fetch_addr;
	uint32_t dcam_fetch_endian;
};

struct isp_dev_fetch_slice_info {
	uint32_t width;
	uint32_t height;
	uint32_t mipi_word;
	uint32_t mipi_byte;
	uint32_t addr[3];
};

/*isp sub block: edge*/
struct ee_param {
	uint32_t ee_mode;
	uint32_t ee_str_d_p;
	uint32_t ee_str_d_n;
	uint32_t ee_edge_thr_d_p;
	uint32_t ee_edge_thr_d_n;
	uint32_t ee_incr_d_p;
	uint32_t ee_incr_d_n;
	uint32_t ee_corner_cor;
	uint32_t ee_corner_th_p;
	uint32_t ee_corner_th_n;
	uint32_t ee_corner_gain_p;
	uint32_t ee_corner_gain_n;
	uint32_t ee_corner_sm_p;
	uint32_t ee_corner_sm_n;
	uint32_t ee_edge_smooth_mode;
	uint32_t ee_flat_smooth_mode;
	uint32_t ipd_bypass;
	uint32_t ipd_mask_mode;
	uint32_t ipd_flat_thr_p;
	uint32_t ipd_flat_thr_n;
	uint32_t ipd_eq_thr_p;
	uint32_t ipd_eq_thr_n;
	uint32_t ipd_more_thr_p;
	uint32_t ipd_more_thr_n;
	uint32_t ipd_less_thr_p;
	uint32_t ipd_less_thr_n;
	uint32_t ipd_smooth_en;
	uint32_t ipd_smooth_mode_p;
	uint32_t ipd_smooth_mode_n;
	uint32_t ipd_smooth_edge_thr_p;
	uint32_t ipd_smooth_edge_thr_n;
	uint32_t ipd_smooth_edge_diff_p;
	uint32_t ipd_smooth_edge_diff_n;
	uint32_t ee_cv_t1;
	uint32_t ee_cv_t2;
	uint32_t ee_cv_t3;
	uint32_t ee_cv_t4;
	uint32_t ee_cv_r1;
	uint32_t ee_cv_r2;
	uint32_t ee_cv_r3;
	uint32_t ee_cv_clip_p;
	uint32_t ee_cv_clip_n;
	uint32_t ee_ratio_hv_3;
	uint32_t ee_ratio_hv_5;
	uint32_t ee_ratio_diag_3;
	uint32_t ee_ratio_diag_5;
	uint32_t ee_gain_hv_1_t1;
	uint32_t ee_gain_hv_1_t2;
	uint32_t ee_gain_hv_1_t3;
	uint32_t ee_gain_hv_1_t4;
	uint32_t ee_gain_hv_1_r1;
	uint32_t ee_gain_hv_1_r2;
	uint32_t ee_gain_hv_1_r3;
	uint32_t ee_gain_hv_2_t1;
	uint32_t ee_gain_hv_2_t2;
	uint32_t ee_gain_hv_2_t3;
	uint32_t ee_gain_hv_2_t4;
	uint32_t ee_gain_hv_2_r1;
	uint32_t ee_gain_hv_2_r2;
	uint32_t ee_gain_hv_2_r3;
	uint32_t ee_gain_diag_1_t1;
	uint32_t ee_gain_diag_1_t2;
	uint32_t ee_gain_diag_1_t3;
	uint32_t ee_gain_diag_1_t4;
	uint32_t ee_gain_diag_1_r1;
	uint32_t ee_gain_diag_1_r2;
	uint32_t ee_gain_diag_1_r3;
	uint32_t ee_gain_diag_2_t1;
	uint32_t ee_gain_diag_2_t2;
	uint32_t ee_gain_diag_2_t3;
	uint32_t ee_gain_diag_2_t4;
	uint32_t ee_gain_diag_2_r1;
	uint32_t ee_gain_diag_2_r2;
	uint32_t ee_gain_diag_2_r3;
	uint32_t ee_weightt_hv2diag;
	uint32_t ee_weightt_diag2hv;
	uint32_t ee_gradient_computation_type;
	uint32_t ee_lum_t1;
	uint32_t ee_lum_t2;
	uint32_t ee_lum_t3;
	uint32_t ee_lum_t4;
	uint32_t ee_lum_r1;
	uint32_t ee_lum_r2;
	uint32_t ee_lum_r3;
	uint32_t ee_pos_t1;
	uint32_t ee_pos_t2;
	uint32_t ee_pos_t3;
	uint32_t ee_pos_t4;
	uint32_t ee_pos_r1;
	uint32_t ee_pos_r2;
	uint32_t ee_pos_r3;
	uint32_t ee_pos_c1;
	uint32_t ee_pos_c2;
	uint32_t ee_pos_c3;
	uint32_t ee_neg_t1;
	uint32_t ee_neg_t2;
	uint32_t ee_neg_t3;
	uint32_t ee_neg_t4;
	uint32_t ee_neg_r1;
	uint32_t ee_neg_r2;
	uint32_t ee_neg_r3;
	uint32_t ee_neg_c1;
	uint32_t ee_neg_c2;
	uint32_t ee_neg_c3;
	uint32_t ee_freq_t1;
	uint32_t ee_freq_t2;
	uint32_t ee_freq_t3;
	uint32_t ee_freq_t4;
	uint32_t ee_freq_r1;
	uint32_t ee_freq_r2;
	uint32_t ee_freq_r3;
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
	unsigned short nodes_r[ISP_PINGPANG_FRGB_GAMC_NUM];
	unsigned short nodes_g[ISP_PINGPANG_FRGB_GAMC_NUM];
	unsigned short nodes_b[ISP_PINGPANG_FRGB_GAMC_NUM];
};

struct isp_dev_gamma_info {
	uint32_t bypass;
	struct gamc_curve_info gamc_nodes;
};

struct gamma_buf_sel {
	uint32_t r_buf_sel;
	uint32_t g_buf_sel;
	uint32_t b_buf_sel;
};

/*isp sub block: grgb*/
struct grgb_param {
	uint32_t curve_t[3][4];
	uint32_t curve_r[3][3];
};

struct isp_dev_grgb_info {
	uint32_t bypass;
	uint32_t diff_thd;
	uint32_t hv_edge_thr;
	uint32_t check_sum_clr;
	uint32_t slash_edge_thr;
	uint32_t slash_flat_thr;
	uint32_t gr_ratio;
	uint32_t hv_flat_thr;
	uint32_t gb_ratio;
	struct grgb_param lum;
	struct grgb_param frez;
};

/*isp sub block: hist*/
struct isp_dev_hist_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t skip_num;
};

/*isp sub block: hist2*/
struct isp_dev_hist2_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t skip_num;
	struct isp_coord hist_roi;
	uint32_t skip_num_clr;
};

/*isp sub block: hsv*/
struct isp_hsv_region_info {
	unsigned short s_curve[5][4];
	unsigned short v_curve[5][4];
	uint32_t hrange_left[5];
	uint32_t hrange_right[5];
};

struct hsv_curve {
	uint32_t h[5][2];
	uint32_t s[5][4];
	uint32_t v[5][4];
};

struct isp_dev_hsv_info {
	uint32_t bypass;
	uint32_t buf_sel;
	struct isp_hsv_region_info region_info[2];
	uint32_t data_ptr[2]; /*compatible with 64bit cpu*/
	uint32_t size;
};

/*isp sub block: hue*/
struct isp_dev_hue_info {
	uint32_t bypass;
	uint32_t theta;
};

/*isp sub block: iircnr*/
struct isp_dev_iircnr_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t uv_th;
	uint32_t y_max_th;
	uint32_t y_min_th;
	uint32_t uv_dist;
	uint32_t uv_pg_th;
	uint32_t sat_ratio;
	uint32_t uv_low_thr2;
	uint32_t uv_low_thr1;
	uint32_t ymd_u;
	uint32_t ymd_v;
	uint32_t uv_s_th;
	uint32_t slope_y_0;
	uint32_t y_th;
	uint32_t alpha_low_u;
	uint32_t alpha_low_v;
	uint32_t middle_factor_y_0;
	uint32_t uv_high_thr2_0;
	uint32_t ymd_min_u;
	uint32_t ymd_min_v;
	uint32_t uv_low_thr[7][2];
	uint32_t y_edge_thr_max[8];
	uint32_t y_edge_thr_min[8];
	uint32_t uv_high_thr2[7];
	uint32_t slope_y[7];
	uint32_t middle_factor_y[7];
	uint32_t middle_factor_uv[8];
	uint32_t slope_uv[8];
	uint32_t pre_uv_th;
	uint32_t css_lum_thr;
	uint32_t uv_diff_thr;
};

struct iircnr_thr {
	uint32_t y_th;
	uint32_t uv_th;
	uint32_t uv_pg_th;
	uint32_t uv_dist;
	uint32_t uv_s_th;
	uint32_t alpha_low_u;
	uint32_t alpha_low_v;
	uint32_t y_max_th;
	uint32_t y_min_th;
	uint32_t pre_uv_th;
	uint32_t sat_ratio;
	uint32_t y_edge_thr_max[8];
	uint32_t y_edge_thr_min[8];
	uint32_t uv_low_thr1_tbl[8];
	uint32_t uv_low_thr2_tbl[8];
	uint32_t uv_high_thr2_tbl[8];
	uint32_t slope_uv[8];
	uint32_t middle_factor_uv[8];
	uint32_t middle_factor_y[8];
	uint32_t slope_y[8];
	uint32_t uv_diff_thr;
	uint32_t css_lum_thr;
};

struct iircnr_ymd {
	uint32_t ymd_u;
	uint32_t ymd_v;
	uint32_t ymd_min_u;
	uint32_t ymd_min_v;
};

struct yrandom_takebit {
	uint32_t takeBit[8];
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
	uint32_t bypass;
	struct nlc_node node;
};

/*isp sub block: nlm*/
struct isp_dev_vst_info {
	uint32_t bypass;
	uint32_t buf_sel;
};

#pragma pack(push)
#pragma pack(4)
struct lum_flat_param {
	unsigned short thresh;
	unsigned short match_count;
	unsigned short inc_strength;
	unsigned short reserved;
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(4)
struct isp_dev_nlm_info {
	uint32_t bypass;
	uint32_t imp_opt_bypass;
	uint32_t flat_opt_bypass;
	uint32_t direction_mode_bypass;
	uint32_t first_lum_byapss;
	uint32_t simple_bpc_bypass;
	uint32_t dist_mode;
	uint32_t radius_bypass;
	uint32_t update_flat_thr_bypass;
	unsigned char w_shift[3];
	uint32_t direction_cnt_th;
	uint32_t simple_bpc_lum_th;
	uint32_t simple_bpc_th;
	uint32_t lum_th0;
	uint32_t lum_th1;
	uint32_t diff_th;
	uint32_t tdist_min_th;
	unsigned short lut_w[72];
	struct lum_flat_param lum_flat[3][3];
	unsigned short lum_flat_addback0[3][4];
	unsigned short lum_flat_addback1[3][4];
	unsigned short lum_flat_addback_min[3][4];
	unsigned short lum_flat_addback_max[3][4];
	uint32_t lum_flat_dec_strenth[3];
	uint32_t vst_bypass;
	uint32_t ivst_bypass;
	uint32_t nlm_first_lum_flat_thresh_coef[3][3];
	uint32_t nlm_first_lum_flat_thresh_max[3][3];
	uint32_t nlm_radial_1D_center_x;
	uint32_t nlm_radial_1D_center_y;
	uint32_t nlm_radial_1D_radius_threshold;
	uint32_t nlm_radial_1D_bypass;
	uint32_t nlm_radial_1D_protect_gain_max;
	uint32_t nlm_radial_1D_radius_threshold_filter_ratio[3][4];
	uint32_t nlm_radial_1D_coef2[3][4];
	uint32_t nlm_radial_1D_protect_gain_min[3][4];

	uint32_t nlm_direction_addback_mode_bypass;
	uint32_t nlm_first_lum_direction_addback[3][4];
	uint32_t nlm_first_lum_direction_addback_noise_clip[3][4];
	uint32_t vst_addr[2];
	uint32_t vst_len;
	uint32_t ivst_addr[2];
	uint32_t ivst_len;
	uint32_t nlm_len;
};
#pragma pack(pop)

struct isp_dev_ivst_info {
	uint32_t bypass;
	uint32_t buf_sel;
};

struct isp_dev_noise_filter_info {
	uint32_t yrandom_bypass;
	uint32_t shape_mode;
	uint32_t filter_thr_mode;
	uint32_t yrandom_mode;
	uint32_t yrandom_seed[4];
	uint32_t takebit[8];
	uint32_t r_offset;
	uint32_t r_shift;
	uint32_t filter_thr;
	uint32_t cv_t[4];
	uint32_t cv_r[3];
	struct edge_pn_config noise_clip;
};

struct noise_filter_param {
	uint32_t noisefilter_shape_mode;
	uint32_t noisefilter_takeBit[8];
	uint32_t noisefilter_r_shift;
	uint32_t noisefilter_r_offset;
	uint32_t noisefilter_filter_thr;
	uint32_t noisefilter_filter_thr_m;
	uint32_t noisefilter_av_t1;
	uint32_t noisefilter_av_t2;
	uint32_t noisefilter_av_t3;
	uint32_t noisefilter_av_t4;
	uint32_t noisefilter_av_r1;
	uint32_t noisefilter_av_r2;
	uint32_t noisefilter_av_r3;
	uint32_t noisefilter_clip_p;
	uint32_t noisefilter_clip_n;
};

/*isp sub block: pgg*/
struct isp_dev_pre_glb_gain_info {
	uint32_t bypass;
	uint32_t gain;
};

/*isp sub block: ynr*/
struct isp_dev_ynr_info {
	uint32_t bypass;
	uint32_t lowlux_bypass;
	uint32_t nr_enable;
	uint32_t l_blf_en[3];
	uint32_t txt_th;
	uint32_t edge_th;
	uint32_t flat_th[7];
	uint32_t lut_th[7];
	uint32_t addback[9];
	uint32_t sub_th[9];
	uint32_t l_euroweight[3][3];
	uint32_t l_wf_index[3];
	uint32_t l0_lut_th0;
	uint32_t l0_lut_th1;
	uint32_t l1_txt_th0;
	uint32_t l1_txt_th1;
	uint32_t wlt_th[24];
	uint32_t freq_ratio[24];
	struct img_offset start_pos;
	struct img_offset center;
	uint32_t radius;
	uint32_t dist_interval;
	unsigned char sal_nr_str[8];
	unsigned char sal_offset[8];
	uint32_t edgeStep[8];
	uint32_t wlt_T[3];
	uint32_t ad_para[3];
	uint32_t ratio[3];
	uint32_t maxRadius;
};

struct ynr_param {
	uint32_t ydenoise_lowlux_bypass;
	uint32_t ydenoise_flat[7];
	uint32_t ydenoise_lut_thresh[7];
	uint32_t ydenoise_subthresh[9];
	uint32_t ydenoise_addback[9];
	uint32_t ydenoise_sedgethresh;
	uint32_t ydenoise_txtthresh;
	uint32_t ydenoise_l1_txt_thresh1;
	uint32_t ydenoise_l1_txt_thresh0;
	uint32_t ydenoise_l0_lut_thresh1;
	uint32_t ydenoise_l0_lut_thresh0;
	uint32_t ydenoise_l1_eurodist[3];
	uint32_t ydenoise_l3_wfindex;
	uint32_t ydenoise_l2_wfindex;
	uint32_t ydenoise_l1_wfindex;
	uint32_t ydenoise_l2_eurodist[3];
	uint32_t ydenoise_l3_eurodist[3];
	uint32_t ydenoise_wv_nr_enable;
	uint32_t ydenoise_l1_blf_enable;
	uint32_t ydenoise_l2_blf_enable;
	uint32_t ydenoise_l3_blf_enable;
	uint32_t wltt[24];
	uint32_t freqratio[24];
	uint32_t dist_interval;
	uint32_t ydenoise_radius;
	uint32_t ydenoise_imgcenterx;
	uint32_t ydenoise_imgcentery;
	unsigned char ydenoise_sal_nr_str[8];
	unsigned char ydenoise_sal_offset[8];
};

/*isp sub block: ynr*/
struct isp_rrgb {
	uint32_t r;
	uint32_t b;
	uint32_t gr;
	uint32_t gb;
};

struct isp_dev_pdaf_info {
	uint32_t  bypass;
	struct isp_img_size block_size;
	struct isp_img_rect win;
	uint32_t pattern_pixel_is_right[PDAF_PPI_NUM];
	uint32_t pattern_pixel_row[PDAF_PPI_NUM];
	uint32_t pattern_pixel_col[PDAF_PPI_NUM];
	uint32_t mode;
	uint32_t skip_num;
};

struct dev_dcam_vc2_control {
	uint32_t bypass;
	uint32_t vch2_vc;
	uint32_t vch2_data_type;
	uint32_t vch2_mode;
};

struct pdaf_addr_info {
	uint32_t addr_l;
	uint32_t addr_r;
};

struct pdaf_ppi_info {
	struct isp_img_size block_size;
	uint32_t pd_pos_size;
	uint32_t pattern_pixel_is_right[PDAF_PPI_NUM];
	uint32_t pattern_pixel_row[PDAF_PPI_NUM];
	uint32_t pattern_pixel_col[PDAF_PPI_NUM];
};

struct pdaf_roi_info {
	struct isp_img_rect win;
};

/*isp sub block: post_blc*/
struct isp_dev_post_blc_info {
	uint32_t bypass;
	uint32_t r_para;
	uint32_t b_para;
	uint32_t gr_para;
	uint32_t gb_para;
};

/*isp sub block: post_cdn*/
struct cdn_thruv {
	unsigned short thru0;
	unsigned short thru1;
	unsigned short thrv0;
	unsigned short thrv1;
};

struct isp_dev_post_cdn_info {
	uint32_t bypass;
	uint32_t downsample_bypass;
	uint32_t mode;
	uint32_t writeback_en;
	uint32_t uvjoint;
	uint32_t median_mode;
	uint32_t adapt_med_thr;
	uint32_t uvthr0;
	uint32_t uvthr1;
	struct cdn_thruv thr_uv;
	unsigned char r_segu[2][7];
	unsigned char r_segv[2][7];
	unsigned char r_distw[15][5];
};

struct post_cdn_thr {
	uint32_t uvthr0;
	uint32_t uvthr1;
	uint32_t thru0;
	uint32_t thru1;
	uint32_t thrv0;
	uint32_t thrv1;
};

struct post_cdn_rseg {
	uint32_t r_segu[2][7];
	uint32_t r_segv[2][7];
};

struct post_cdn_distw {
	uint32_t r_distw[15][5];
};

/*isp sub block: pstrz*/
struct isp_dev_posterize_info {
	uint32_t bypass;
	unsigned char posterize_level_bottom[POSTERIZE_NUM];
	unsigned char posterize_level_top[POSTERIZE_NUM];
	unsigned char posterize_level_out[POSTERIZE_NUM];
};

struct pstrz_level {
	int posterize_level_bottom[8];
	int posterize_level_top[8];
	int posterize_level_out[8];
};

/*isp sub block: rlsc*/
struct rlsc_init_node {
	uint32_t init_r0c0;
	uint32_t init_r0c1;
	uint32_t init_r1c0;
	uint32_t init_r1c1;
};

struct isp_dev_radial_lsc_info {
	uint32_t bypass;
	uint32_t radius_step;
	uint32_t buf_sel;
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
	uint32_t bypass;
	uint32_t global_gain;
	uint32_t r_gain;
	uint32_t g_gain;
	uint32_t b_gain;
	uint32_t reserv;
};

struct isp_dev_rgb_dither_info {
	uint32_t random_bypass;
	uint32_t random_mode;
	uint32_t seed;
	uint32_t range;
	uint32_t r_offset;
	uint32_t r_shift;
	uint32_t takebit[8];
};

struct rgbg_gain {
	uint32_t r_gain;
	uint32_t g_gain;
	uint32_t b_gain;
};

/*isp sub block: rgb2y*/
struct isp_rgb {
	uint32_t r;
	uint32_t g;
	uint32_t b;
};

struct isp_dev_rgb2y_info {
	uint32_t bypass;
	uint32_t bayer2y_mode;
	uint32_t bayer2y_chanel;
};

/*isp sub block: store*/
struct store_border {
	unsigned short up_border;
	unsigned short down_border;
	unsigned short left_border;
	unsigned short right_border;
};

struct isp_dev_feeder_info {
	uint32_t data_type;
};

struct isp_dev_store_info {
	uint32_t bypass;
	uint32_t endian;
	uint32_t subtract;
	uint32_t color_format;
	struct isp_img_size size;
	struct store_border border;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;
	unsigned short shadow_clr_sel;
	unsigned short shadow_clr;
	unsigned short store_res;
	unsigned short rd_ctrl;
};

struct isp_dev_store0_info {
	uint32_t bypass;
	uint32_t st_max_len_sel;
	uint32_t yuv_mode;
	uint32_t shadow_clr_sel;
	uint32_t st_y_axi_reorder_en;
	uint32_t st_uv_axi_reorder_en;
	struct isp_img_size size;
	uint32_t st_luma_addr[NR3_BUF_NUM];
	uint32_t st_chroma_addr[NR3_BUF_NUM];
	uint32_t st_pitch;
	uint32_t shadow_clr;
	uint32_t buf_sel;
};

struct isp_dev_store_preview_info {
	uint32_t bypass;
	uint32_t endian;
	uint32_t speed_2x;
	uint32_t mirror_en;
	uint32_t color_format;
	uint32_t max_len_sel;
	struct isp_img_size size;
	struct store_border border;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;
	unsigned short shadow_clr;
	unsigned short store_res;
	unsigned short rd_ctrl;
	unsigned short shadow_clr_sel;
};

struct isp_dev_store_video_info {
	uint32_t bypass;
	uint32_t endian;
	uint32_t speed_2x;
	uint32_t mirror_en;
	uint32_t color_format;
	uint32_t max_len_sel;
	struct isp_img_size size;
	struct store_border border;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;
	unsigned short shadow_clr;
	unsigned short store_res;
	unsigned short rd_ctrl;
	unsigned short shadow_clr_sel;
};

struct isp_dev_store_capture_info {
	uint32_t bypass;
	uint32_t endian;
	uint32_t speed_2x;
	uint32_t mirror_en;
	uint32_t color_format;
	uint32_t max_len_sel;
	struct isp_img_size size;
	struct store_border border;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;
	unsigned short shadow_clr;
	unsigned short store_res;
	unsigned short rd_ctrl;
	unsigned short shadow_clr_sel;
};

struct isp_dev_scaler0_pre_info {
	uint32_t isp_sacler_all_bypass;
	uint32_t isp_uv_sync_y;
	struct isp_img_size size;
};

struct isp_dev_scaler_pre_info {
	uint32_t bypass;
	struct isp_img_size size;
};

struct isp_dev_scaler_vid_info {
	uint32_t bypass;
	struct isp_img_size size;
};

struct isp_dev_scaler_cap_info {
	uint32_t bypass;
	struct isp_img_size size;
};

struct isp_dev_store_cce_info {
	uint32_t bypass;
	uint32_t max_len_sel;
	uint32_t speed_2x;
	uint32_t mirror_en;
	uint32_t color_format;
	uint32_t endian;
	struct isp_img_size size;
	struct store_border border;
	struct isp_addr_fs addr[NR3_BUF_NUM];
	struct isp_pitch_fs pitch;
	unsigned short shadow_clr;
	unsigned short store_res;
	unsigned short rd_ctrl;
	unsigned short shadow_clr_sel;
	uint32_t total_word;
	uint32_t up_border;
	uint32_t down_border;
	uint32_t buf_sel;
};

struct slice_overlap {
	uint32_t overlap_up;
	uint32_t overlap_down;
	uint32_t overlap_left;
	uint32_t overlap_right;
};

/*isp sub block: uvd*/
struct uvd_th {
	unsigned char th_h[2];
	unsigned char th_l[2];
};

struct isp_dev_uvd_info {
	uint32_t bypass;
	uint32_t chk_sum_clr_en;
	uint32_t lum_th_h_len;
	uint32_t lum_th_h;
	uint32_t lum_th_l_len;
	uint32_t lum_th_l;
	uint32_t chroma_min_h;
	uint32_t chroma_min_l;
	uint32_t chroma_max_h;
	uint32_t chroma_max_l;
	struct uvd_th u_th;
	struct uvd_th v_th;
	uint32_t ratio;
	uint32_t ratio_uv_min;
	uint32_t ratio_y_min[2];
	uint32_t ratio0;
	uint32_t ratio1;
	uint32_t y_th_l_len;
	uint32_t y_th_h_len;
	uint32_t uv_abs_th_len;
};

/*isp sub block: y_delay*/
struct isp_dev_ydelay_info {
	uint32_t bypass;
	uint32_t step;
};

/*isp sub block: y_gamma*/
struct isp_dev_ygamma_info {
	uint32_t bypass;
	uint32_t buf_sel;
	struct coordinate_xy nodes[ISP_PINGPANG_YUV_YGAMMA_NUM];
};

/*isp sub block: pre_cdn*/
struct isp_dev_yuv_precdn_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t median_writeback_en;
	uint32_t median_mode;
	uint32_t den_stren;
	uint32_t uv_joint;
	struct cdn_thruv median_thr_uv;
	uint32_t median_thr;
	uint32_t uv_thr;
	uint32_t y_thr;
	unsigned char r_segu[2][7];
	unsigned char r_segv[2][7];
	unsigned char r_segy[2][7];
	unsigned char r_distw[25];
};

struct pre_cdn_thr {
	uint32_t median_thr;
	uint32_t y_thr;
	uint32_t uv_thr;
	uint32_t median_thr_u[2];
	uint32_t median_thr_v[2];
};

struct pre_cdn_seg {
	uint32_t r_segu[2][7];
	uint32_t r_segv[2][7];
	uint32_t r_segy[2][7];
};

struct pre_cdn_dist {
	uint32_t r_distw[25];
};

struct isp_scaling_ratio {
	unsigned char vertical;
	unsigned char horizontal;
};

struct isp_statis_frame_output {
	uint32_t format;
	uint32_t buf_size;
	unsigned long phy_addr;
	unsigned long vir_addr;
};

enum isp_statis_valid_type {
	ISP_STATIS_VALID_AEM = (1 << 0),
	ISP_STATIS_VALID_AFM = (1 << 1),
	ISP_STATIS_VALID_AFL = (1 << 2),
	ISP_STATIS_VALID_PDAF = (1 << 3),
	ISP_STATIS_VALID_HIST = (1 << 4),
	ISP_STATIS_VALID_HIST2 = (1 << 5),
	ISP_STATIS_VALID_EBD = (1 << 6),
};

struct isp_statis_buf_input {
	uint32_t buf_size;
	uint32_t dcam_stat_buf_size;
	uint32_t buf_num;
	unsigned long phy_addr;
	unsigned long vir_addr;
	unsigned long addr_offset;
	uint32_t kaddr[2];
	unsigned long mfd;
	unsigned long dev_fd;
	uint32_t buf_property;
	uint32_t buf_flag;
	uint32_t statis_valid;
	uint32_t reserved[4];
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
	ISP_PDAF_BLOCK,
	ISP_EBD_BLOCK,
	ISP_DCAM_BLOCK_MAX,
	ISP_HIST_BLOCK,
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
	IRQ_PDAF_STATIS,
	IRQ_EBD_STATIS,
	IRQ_HIST_STATIS,
	IRQ_MAX_DONE,
};

struct sprd_isp_time {
	uint32_t sec;
	uint32_t usec;
};

struct isp_irq_info {
	uint32_t irq_type;
	uint32_t irq_flag;
	uint32_t format;
	uint32_t channel_id;
	uint32_t base_id;
	uint32_t img_id;
	uint32_t irq_id;
	uint32_t sensor_id;
	unsigned long yaddr;
	unsigned long uaddr;
	unsigned long vaddr;
	unsigned long yaddr_vir;
	unsigned long uaddr_vir;
	unsigned long vaddr_vir;
	uint32_t img_y_fd;
	uint32_t img_u_fd;
	uint32_t img_v_fd;
	unsigned long length;
	struct isp_img_size buf_size;
	struct sprd_isp_time time_stamp;
	struct isp_statis_frame_output isp_statis_output;
	uint32_t frm_index;
};

/* add for compile problem*/
struct isp_dev_lsc_info {
	uint32_t bypass;
	uint32_t grid_pitch;
	uint32_t grid_mode;
	uint32_t endian;
	uint32_t buf_addr[2];
	uint32_t buf_len;
};

/*awbm*/
struct awbm_rect_pos {
	uint32_t start_x[5];
	uint32_t start_y[5];
	uint32_t end_x[5];
	uint32_t end_y[5];
};

struct awbm_circle_pos {
	uint32_t x[5];
	uint32_t y[5];
	uint32_t r[5];
};

struct awbm_pixel_num {
	uint32_t pixel_num[5];
};

struct awbm_thr {
	uint32_t r_high;
	uint32_t r_low;
	uint32_t g_high;
	uint32_t g_low;
	uint32_t b_high;
	uint32_t b_low;
};

struct isp_dev_awb_info_v2 {
	/*AWBM*/
	uint32_t awbm_bypass;
	uint32_t mode;
	uint32_t skip_num;
	struct img_offset block_offset;
	struct isp_img_size block_size;
	uint32_t shift;
	uint32_t thr_bypass;
	struct awbm_rect_pos rect_pos;
	struct awbm_circle_pos circle_pos;
	struct awbm_rect_pos clctor_pos;
	struct awbm_pixel_num pix_num;
	struct awbm_thr thr;
	struct isp_img_size slice_size;
	uint32_t skip_num_clear;
	uint32_t position_sel;
	uint32_t mem_addr;
	/*AWBC*/
	uint32_t awbc_bypass;
	uint32_t alpha_bypass;
	uint32_t alpha_value;
	uint32_t buf_sel;
	struct awbc_param gain;
	struct awbc_rgb thrd;
	struct awbc_param gain_offset;
	struct awbc_param gain_buff;
	struct awbc_param gain_offset_buff;
};

struct isp_dev_pre_wavelet_info {
	uint32_t bypass;
	uint32_t radial_bypass;
	uint32_t gain_thrs0;
	uint32_t gain_thrs1;
	uint32_t bitshift0;
	uint32_t bitshift1;
	uint32_t offset;
	uint32_t nsr_slope;
	uint32_t lum_ratio;
	uint32_t center_pos_x;
	uint32_t center_pos_y;
	uint32_t delta_x2;
	uint32_t delta_y2;
	uint32_t r2_thr;
	uint32_t p_param1;
	uint32_t p_param2;
	uint32_t addback;
	uint32_t gain_max_thr;
	uint32_t pos_x;
	uint32_t pos_y;
	uint32_t lum_shink_level;
};

struct isp_dev_bdn_info {
	uint32_t bypass;
	uint32_t radial_bypass;
	uint32_t addback;
	uint32_t dis[10][2];
	uint32_t ran[10][8];
	uint32_t offset_x;
	uint32_t offset_y;
	uint32_t squ_x2;
	uint32_t squ_y2;
	uint32_t coef;
	uint32_t coef2;
	uint32_t start_pos_x;
	uint32_t start_pos_y;
	uint32_t offset;
	uint32_t dis_level;
	uint32_t ran_level;
};

struct isp_dev_nlm_info_v2 {
	uint32_t bypass;
	uint32_t imp_opt_bypass;
	uint32_t flat_opt_bypass;
	uint32_t buf_sel;
	unsigned char strength[5];
	unsigned char cnt[5];
	unsigned short thresh[5];
	uint32_t streng_th;
	uint32_t texture_dec;
	uint32_t is_flat;
	uint32_t addback;
	unsigned short lut_w[72];
	uint32_t vst_addr[2];
	uint32_t vst_len;
	uint32_t ivst_addr[2];
	uint32_t ivst_len;
	uint32_t nlm_addr[2];
	uint32_t nlm_len;
	uint32_t strength_level;

};

struct isp_dev_cmc8_info {
	uint32_t bypass;
	uint32_t buf_sel;
	uint32_t alpha;
	uint32_t alpha_bypass;
	struct cmc_matrix matrix;
	struct cmc_matrix matrix_buf;
};

struct isp_dev_rgb_gain2_info {
	uint32_t bypass;
	uint32_t r_gain;
	uint32_t g_gain;
	uint32_t b_gain;
	uint32_t r_offset;
	uint32_t g_offset;
	uint32_t b_offset;
};

struct isp_dev_ct_info {
	uint32_t bypass;
	uint32_t data_ptr[2];
	uint32_t size;
};

struct isp_dev_csc_info {
	uint32_t bypass;
	uint32_t red_centre_x;
	uint32_t red_centre_y;
	uint32_t blue_centre_x;
	uint32_t blue_centre_y;
	uint32_t red_x2_init;
	uint32_t red_y2_init;
	uint32_t blue_x2_init;
	uint32_t blue_y2_init;
	uint32_t red_threshold;
	uint32_t blue_threshold;
	uint32_t red_p1_param;
	uint32_t red_p2_param;
	uint32_t blue_p1_param;
	uint32_t blue_p2_param;
	uint32_t max_gain_thr;
	struct isp_img_size img_size;
	struct img_offset start_pos;
};

struct isp_dev_css_info {
	uint32_t bypass;
	uint32_t lh_chrom_th;
	unsigned char chrom_lower_th[7];
	unsigned char reserved0;
	unsigned char chrom_high_th[7];
	unsigned char reserved1;
	uint32_t lum_low_shift;
	uint32_t lum_hig_shift;
	unsigned char lh_ratio[8];
	unsigned char ratio[8];
	uint32_t lum_low_th;
	uint32_t lum_ll_th;
	uint32_t lum_hig_th;
	uint32_t lum_hh_th;
	uint32_t u_th_0_l;
	uint32_t u_th_0_h;
	uint32_t v_th_0_l;
	uint32_t v_th_0_h;
	uint32_t u_th_1_l;
	uint32_t u_th_1_h;
	uint32_t v_th_1_l;
	uint32_t v_th_1_h;
	uint32_t cutoff_th;
};

struct isp_dev_emboss_info {
	uint32_t y_bypass;
	uint32_t uv_bypass;
	uint32_t y_step;
	uint32_t uv_step;
};

struct isp_dev_yrandom_info {
	uint32_t yrandom_bypass;
	uint32_t seed;
	uint32_t offset;
	uint32_t shift;
	unsigned char takeBit[8];
};

struct isp_dev_prefilter_info {
	uint32_t bypass;
	uint32_t writeback;
	uint32_t thrd;
};

struct afm_subfilter {
	uint32_t average;
	uint32_t median;
};

struct afm_shift {
	uint32_t shift_spsmd;
	uint32_t shift_sobel5;
	uint32_t shift_sobel9;
};

struct afm_thrd_rgb {
	uint32_t sobel5_thr_min_red;
	uint32_t sobel5_thr_max_red;
	uint32_t sobel5_thr_min_green;
	uint32_t sobel5_thr_max_green;
	uint32_t sobel5_thr_min_blue;
	uint32_t sobel5_thr_max_blue;
	uint32_t sobel9_thr_min_red;
	uint32_t sobel9_thr_max_red;
	uint32_t sobel9_thr_min_green;
	uint32_t sobel9_thr_max_green;
	uint32_t sobel9_thr_min_blue;
	uint32_t sobel9_thr_max_blue;
	uint32_t spsmd_thr_min_red;
	uint32_t spsmd_thr_max_red;
	uint32_t spsmd_thr_min_green;
	uint32_t spsmd_thr_max_green;
	uint32_t spsmd_thr_min_blue;
	uint32_t spsmd_thr_max_blue;
};

struct isp_dev_yiq_aem_info {
	uint32_t ygamma_bypass;
	short gamma_xnode[10];
	short gamma_ynode[10];
	uint32_t gamma_node_idx[10];
	uint32_t aem_bypass;
	uint32_t aem_mode;
	uint32_t aem_skip_num;
	uint32_t offset_x;
	uint32_t offset_y;
	uint32_t width;
	uint32_t height;
};

struct isp_dev_yiq_aem_info_v2 {
	uint32_t ygamma_bypass;
	uint32_t gamma_xnode[10];
	uint32_t gamma_ynode[10];
	uint32_t gamma_node_idx[10];
	uint32_t aem_bypass;
	uint32_t aem_mode;
	uint32_t aem_skip_num;
	uint32_t offset_x;
	uint32_t offset_y;
	uint32_t avgshift;
	uint32_t width;
	uint32_t height;
};

struct isp_dev_yiq_afm_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t source_pos;
	uint32_t shift;
	uint32_t skip_num;
	uint32_t skip_num_clear;
	uint32_t format;
	uint32_t iir_bypass;
	struct isp_coord coord[25];
	short IIR_c[11];
	unsigned short reserved;
};

struct isp_dev_yiq_afm_info_v2 {
	uint32_t bypass;
	uint32_t mode;
	uint32_t shift;
	uint32_t skip_num;
	uint32_t skip_num_clear;
	uint32_t af_position;
	uint32_t format;
	uint32_t iir_bypass;
	struct isp_coord coord[25];
	int IIR_c[11];
};

struct isp_time {
	uint32_t sec;
	uint32_t usec;
};

struct isp_irq {
	uint32_t irq_val0;
	uint32_t irq_val1;
	uint32_t irq_val2;
	uint32_t irq_val3;
	uint32_t reserved;
	int32_t ret_val;
	struct isp_time time;
};

struct isp_edge_thrd {
	uint32_t detail;
	uint32_t smooth;
	uint32_t strength;
};

struct isp_capability {
	uint32_t isp_id;
	uint32_t index;
	void __user *property_param;
};

struct isp_cce_matrix_tab {
	unsigned short matrix[ISP_CCE_MATRIX_TAB_MAX];
	unsigned short reserved;
};

struct isp_cce_shift {
	uint32_t y_shift;
	uint32_t u_shift;
	uint32_t v_shift;
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
	uint32_t r[ISP_AWBM_ITEM];
	uint32_t g[ISP_AWBM_ITEM];
	uint32_t b[ISP_AWBM_ITEM];
};

struct isp_awbc_rgb {
	uint32_t r;
	uint32_t g;
	uint32_t b;
};

struct isp_raw_awbm_statistics {
	uint32_t num0;
	uint32_t num1;
	uint32_t num2;
	uint32_t num3;
	uint32_t num4;
	uint32_t num_t;
	uint32_t block_r;
	uint32_t block_g;
	uint32_t block_b;
};

struct isp_fetch_endian {
	uint32_t endian;
	uint32_t bit_recorder;
};

struct isp_bayer_mode {
	uint32_t nlc_bayer;
	uint32_t awbc_bayer;
	uint32_t wave_bayer;
	uint32_t cfa_bayer;
	uint32_t gain_bayer;
};

struct isp_cfa_thrd {
	uint32_t edge;
	uint32_t ctrl;
};

struct isp_pitch {
	uint32_t chn0;
	uint32_t chn1;
	uint32_t chn2;
};

struct isp_grgb_thrd {
	uint32_t edge;
	uint32_t diff;
};

struct isp_raw_aem_statistics {
	uint32_t r[ISP_RAW_AEM_ITEM];
	uint32_t g[ISP_RAW_AEM_ITEM];
	uint32_t b[ISP_RAW_AEM_ITEM];
};

struct isp_bpc_common {
	uint32_t pattern_type;
	uint32_t detect_thrd;
	uint32_t super_bad_thrd;
};

struct isp_bpc_thrd {
	uint32_t flat;
	uint32_t std;
	uint32_t texture;
};

struct isp_interrupt {
	uint32_t isp_id;
	uint32_t int_mode;
};

struct isp_reg_bits {
	unsigned long reg_addr;
	unsigned long reg_value;
};

struct isp_reg_param {
	unsigned long reg_param;
	uint32_t counts;
};

struct thrd_min_max {
	uint32_t min;
	uint32_t max;
};

struct isp_dev_rgb_afm_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t skip_num;
	uint32_t lum_stat_chn_sel;
	uint32_t iir_eb;
	uint32_t clk_gate_dis;
	uint32_t source_sel;
	uint32_t crop_eb;
	struct img_offset done_tile_num;
	struct isp_img_rect crop_size;
	struct isp_img_rect win;
	struct img_offset win_num;
	short iir_g0;
	short iir_g1;
	short iir_c[10];
	uint32_t channel_sel;
	uint32_t denoise_mode;
	uint32_t center_weight;
	uint32_t clip_en0;
	uint32_t clip_en1;
	uint32_t fv0_shift;
	uint32_t fv1_shift;
	struct thrd_min_max fv0_th;
	struct thrd_min_max fv1_th;
	short fv1_coeff[4][9];
};

struct af_enhanced_module_info {
	unsigned char chl_sel;
	unsigned char nr_mode;
	unsigned char center_weight;
	unsigned char clip_en[2];
	uint32_t max_th[2];
	uint32_t min_th[2];
	unsigned short fv_shift[2];
	short fv1_coeff[36];
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
	uint32_t sdw_done_int_cnt_num;
	uint32_t sdw_down_skip_cnt_num;
	uint32_t all_done_int_cnt_num;
	uint32_t all_done_skip_cnt_num;
};

struct skip_ctrl1_param {
	uint32_t vid_done_int_cnt_num;
	uint32_t vid_done_skip_cnt_num;
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
	uint32_t img_fd;
	uint32_t sensor_id;
	uint32_t hw_simu_flag;
};

struct isp_3dnr_const_param {
	uint32_t fusion_mode;
	uint32_t filter_switch;
	uint32_t y_pixel_noise_threshold;
	uint32_t u_pixel_noise_threshold;
	uint32_t v_pixel_noise_threshold;
	uint32_t y_pixel_noise_weight;
	uint32_t u_pixel_noise_weight;
	uint32_t v_pixel_noise_weight;
	uint32_t threshold_radial_variation_u_range_min;
	uint32_t threshold_radial_variation_u_range_max;
	uint32_t threshold_radial_variation_v_range_min;
	uint32_t threshold_radial_variation_v_range_max;
	uint32_t y_threshold_polyline_0;
	uint32_t y_threshold_polyline_1;
	uint32_t y_threshold_polyline_2;
	uint32_t y_threshold_polyline_3;
	uint32_t y_threshold_polyline_4;
	uint32_t y_threshold_polyline_5;
	uint32_t y_threshold_polyline_6;
	uint32_t y_threshold_polyline_7;
	uint32_t y_threshold_polyline_8;
	uint32_t u_threshold_polyline_0;
	uint32_t u_threshold_polyline_1;
	uint32_t u_threshold_polyline_2;
	uint32_t u_threshold_polyline_3;
	uint32_t u_threshold_polyline_4;
	uint32_t u_threshold_polyline_5;
	uint32_t u_threshold_polyline_6;
	uint32_t u_threshold_polyline_7;
	uint32_t u_threshold_polyline_8;
	uint32_t v_threshold_polyline_0;
	uint32_t v_threshold_polyline_1;
	uint32_t v_threshold_polyline_2;
	uint32_t v_threshold_polyline_3;
	uint32_t v_threshold_polyline_4;
	uint32_t v_threshold_polyline_5;
	uint32_t v_threshold_polyline_6;
	uint32_t v_threshold_polyline_7;
	uint32_t v_threshold_polyline_8;
	uint32_t y_intensity_gain_polyline_0;
	uint32_t y_intensity_gain_polyline_1;
	uint32_t y_intensity_gain_polyline_2;
	uint32_t y_intensity_gain_polyline_3;
	uint32_t y_intensity_gain_polyline_4;
	uint32_t y_intensity_gain_polyline_5;
	uint32_t y_intensity_gain_polyline_6;
	uint32_t y_intensity_gain_polyline_7;
	uint32_t y_intensity_gain_polyline_8;
	uint32_t u_intensity_gain_polyline_0;
	uint32_t u_intensity_gain_polyline_1;
	uint32_t u_intensity_gain_polyline_2;
	uint32_t u_intensity_gain_polyline_3;
	uint32_t u_intensity_gain_polyline_4;
	uint32_t u_intensity_gain_polyline_5;
	uint32_t u_intensity_gain_polyline_6;
	uint32_t u_intensity_gain_polyline_7;
	uint32_t u_intensity_gain_polyline_8;
	uint32_t v_intensity_gain_polyline_0;
	uint32_t v_intensity_gain_polyline_1;
	uint32_t v_intensity_gain_polyline_2;
	uint32_t v_intensity_gain_polyline_3;
	uint32_t v_intensity_gain_polyline_4;
	uint32_t v_intensity_gain_polyline_5;
	uint32_t v_intensity_gain_polyline_6;
	uint32_t v_intensity_gain_polyline_7;
	uint32_t v_intensity_gain_polyline_8;
	uint32_t gradient_weight_polyline_0;
	uint32_t gradient_weight_polyline_1;
	uint32_t gradient_weight_polyline_2;
	uint32_t gradient_weight_polyline_3;
	uint32_t gradient_weight_polyline_4;
	uint32_t gradient_weight_polyline_5;
	uint32_t gradient_weight_polyline_6;
	uint32_t gradient_weight_polyline_7;
	uint32_t gradient_weight_polyline_8;
	uint32_t gradient_weight_polyline_9;
	uint32_t gradient_weight_polyline_10;
	uint32_t y_pixel_src_weight[4];
	uint32_t u_pixel_src_weight[4];
	uint32_t v_pixel_src_weight[4];
	uint32_t u_threshold_factor[4];
	uint32_t v_threshold_factor[4];
	uint32_t u_divisor_factor[4];
	uint32_t v_divisor_factor[4];
	uint32_t r1_circle;
	uint32_t r2_circle;
	uint32_t r3_circle;
};

struct isp_3dnr_fast_me {
	uint32_t nr3_channel_sel;
	uint32_t nr3_project_mode;
};

struct isp_3dnr_tunning_param {
	struct isp_3dnr_fast_me fast_me;
	struct isp_3dnr_const_param blend_param;
};

#endif
