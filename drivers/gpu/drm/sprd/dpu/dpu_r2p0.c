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

#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include "sprd_dpu.h"
#include "sprd_corner.h"

#define DISPC_INT_FBC_PLD_ERR_MASK	BIT(8)
#define DISPC_INT_FBC_HDR_ERR_MASK	BIT(9)

#define DISPC_INT_MMU_INV_WR_MASK	BIT(19)
#define DISPC_INT_MMU_INV_RD_MASK	BIT(18)
#define DISPC_INT_MMU_VAOR_WR_MASK	BIT(17)
#define DISPC_INT_MMU_VAOR_RD_MASK	BIT(16)

#define XFBC8888_HEADER_SIZE(w, h) (ALIGN((w) * (h) / (8 * 8) / 2, 128))
#define XFBC8888_PAYLOAD_SIZE(w, h) (ALIGN(w, 8) * ALIGN(h, 8) * 4)
#define XFBC8888_BUFFER_SIZE(w, h) (XFBC8888_HEADER_SIZE(w, h) \
				+ XFBC8888_PAYLOAD_SIZE(w, h))

#define XFBC565_HEADER_SIZE(w, h) (ALIGN((w) * (h) / (16 * 8) / 2, 128))
#define XFBC565_PAYLOAD_SIZE(w, h) (w * h * 2)
#define XFBC565_BUFFER_SIZE(w, h) (XFBC565_HEADER_SIZE(w, h) \
				+ XFBC565_PAYLOAD_SIZE(w, h))

#define update_work	wb_work

#define SLP_BRIGHTNESS_THRESHOLD 0x20

struct layer_reg {
	u32 addr[4];
	u32 ctrl;
	u32 size;
	u32 pitch;
	u32 pos;
	u32 alpha;
	u32 ck;
	u32 pallete;
	u32 crop_start;
};

struct wb_region_reg {
	u32 pos;
	u32 size;
};

struct dpu_reg {
	u32 dpu_version;
	u32 dpu_ctrl;
	u32 dpu_cfg0;
	u32 dpu_cfg1;
	u32 dpu_cfg2;
	u32 dpu_secure;
	u32 reserved_0x0018_0x001C[2];
	u32 panel_size;
	u32 blend_size;
	u32 reserved_0x0028;
	u32 bg_color;
	struct layer_reg layers[8];
	u32 wb_base_addr;
	u32 wb_ctrl;
	u32 wb_cfg;
	u32 wb_pitch;
	struct wb_region_reg region[3];
	u32 reserved_0x01D8_0x01DC[2];
	u32 dpu_int_en;
	u32 dpu_int_clr;
	u32 dpu_int_sts;
	u32 dpu_int_raw;
	u32 dpi_ctrl;
	u32 dpi_h_timing;
	u32 dpi_v_timing;
	u32 reserved_0x01FC;
	u32 dpu_enhance_cfg;
	u32 reserved_0x0204_0x020C[3];
	u32 epf_epsilon;
	u32 epf_gain0_3;
	u32 epf_gain4_7;
	u32 epf_diff;
	u32 reserved_0x0220_0x023C[8];
	u32 hsv_lut_addr;
	u32 hsv_lut_wdata;
	u32 hsv_lut_rdata;
	u32 reserved_0x024C_0x027C[13];
	u32 cm_coef01_00;
	u32 cm_coef03_02;
	u32 cm_coef11_10;
	u32 cm_coef13_12;
	u32 cm_coef21_20;
	u32 cm_coef23_22;
	u32 reserved_0x0298_0x02BC[10];
	u32 slp_cfg0;
	u32 slp_cfg1;
	u32 reserved_0x02C8_0x02FC[14];
	u32 gamma_lut_addr;
	u32 gamma_lut_wdata;
	u32 gamma_lut_rdata;
	u32 reserved_0x030C_0x033C[13];
	u32 checksum_en;
	u32 checksum0_start_pos;
	u32 checksum0_end_pos;
	u32 checksum1_start_pos;
	u32 checksum1_end_pos;
	u32 checksum0_result;
	u32 checksum1_result;
	u32 reserved_0x035C;
	u32 dpu_sts[18];
	u32 reserved_0x03A8_0x03AC[2];
	u32 dpu_fbc_cfg0;
	u32 dpu_fbc_cfg1;
	u32 reserved_0x03B8_0x03EC[14];
	u32 rf_ram_addr;
	u32 rf_ram_rdata_low;
	u32 rf_ram_rdata_high;
	u32 reserved_0x03FC_0x07FC[257];
	u32 mmu_en;
	u32 mmu_update;
	u32 mmu_min_vpn;
	u32 mmu_vpn_range;
	u32 mmu_pt_addr;
	u32 mmu_default_page;
	u32 mmu_vaor_addr_rd;
	u32 mmu_vaor_addr_wr;
	u32 mmu_inv_addr_rd;
	u32 mmu_inv_addr_wr;
	u32 mmu_uns_addr_rd;
	u32 mmu_uns_addr_wr;
	u32 mmu_miss_cnt;
	u32 mmu_pt_update_qos;
	u32 mmu_version;
	u32 mmu_min_ppn1;
	u32 mmu_ppn_range1;
	u32 mmu_min_ppn2;
	u32 mmu_ppn_range2;
	u32 mmu_vpn_paor_rd;
	u32 mmu_vpn_paor_wr;
	u32 mmu_ppn_paor_rd;
	u32 mmu_ppn_paor_wr;
	u32 mmu_reg_au_manage;
	u32 mmu_page_rd_ch;
	u32 mmu_page_wr_ch;
	u32 mmu_read_page_cmd_cnt;
	u32 mmu_read_page_latency_cnt;
	u32 mmu_page_max_latency;
};

struct wb_region {
	u32 index;
	u16 pos_x;
	u16 pos_y;
	u16 size_w;
	u16 size_h;
};

struct enhance_module {
	u32 scl_en: 1;
	u32 epf_en: 1;
	u32 hsv_en: 1;
	u32 cm_en: 1;
	u32 slp_en: 1;
	u32 gamma_en: 1;
	u32 blp_en: 1;
};

struct scale_cfg {
	u32 in_w;
	u32 in_h;
};

struct epf_cfg {
	u16 epsilon0;
	u16 epsilon1;
	u8 gain0;
	u8 gain1;
	u8 gain2;
	u8 gain3;
	u8 gain4;
	u8 gain5;
	u8 gain6;
	u8 gain7;
	u8 max_diff;
	u8 min_diff;
};

struct hsv_entry {
	u16 hue;
	u16 sat;
};

struct hsv_lut {
	struct hsv_entry table[360];
};

struct gamma_entry {
	u16 r;
	u16 g;
	u16 b;
};

struct gamma_lut {
	u16 r[256];
	u16 g[256];
	u16 b[256];
};

struct cm_cfg {
	short coef00;
	short coef01;
	short coef02;
	short coef03;
	short coef10;
	short coef11;
	short coef12;
	short coef13;
	short coef20;
	short coef21;
	short coef22;
	short coef23;
};

struct slp_cfg {
	u8 brightness;
	u8 conversion_matrix;
	u8 brightness_step;
	u8 second_bright_factor;
	u8 first_percent_th;
	u8 first_max_bright_th;
};

static struct scale_cfg scale_copy;
static struct cm_cfg cm_copy;
static struct slp_cfg slp_copy;
static struct gamma_lut gamma_copy;
static struct hsv_lut hsv_copy;
static struct epf_cfg epf_copy;
static u32 enhance_en;

static DECLARE_WAIT_QUEUE_HEAD(wait_queue);
static bool panel_ready = true;
static bool need_scale;
static bool mode_changed;
static bool evt_update;
static bool evt_stop;
static int wb_en;
static int max_vsync_count;
static int vsync_count;
static u32 prev_y2r_coef;
static bool sprd_corner_support;
static int sprd_corner_radius;
static struct tasklet_struct dump_task;
static u32 dump_reg[8][4];
//static struct sprd_adf_hwlayer wb_layer;
//static struct wb_region region[3];
//static int wb_xfbc_en = 1;
//module_param(wb_xfbc_en, int, 0644);
//module_param(max_vsync_count, int, 0644);

static void dpu_sr_config(struct dpu_context *ctx);
static void dpu_enhance_reload(struct dpu_context *ctx);
static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_dpu_layer *hwlayer);
//static void dpu_write_back(struct dpu_context *ctx,
//		struct wb_region region[], u8 count);

static u32 dpu_get_version(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	return reg->dpu_version;
}

static bool dpu_check_raw_int(struct dpu_context *ctx, u32 mask)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 val;

	val = reg->dpu_int_raw;
	if (val & mask)
		return true;

	pr_err("dpu_int_raw:0x%x\n", val);
	return false;
}

static int dpu_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	int ret = 0;

	ret = of_property_read_u32(np, "sprd,corner-radius",
					&sprd_corner_radius);
	if (!ret) {
		sprd_corner_support = 1;
		ctx->corner_size = sprd_corner_radius;
		pr_info("round corner support, radius = %d.\n",
					sprd_corner_radius);
	}

	return 0;
}

static void dpu_corner_init(struct dpu_context *ctx)
{
	static bool corner_is_inited;

	if (!corner_is_inited && sprd_corner_support) {
		sprd_corner_hwlayer_init(ctx->vm.vactive, ctx->vm.hactive,
				sprd_corner_radius);

		/* change id value based on different cpu chip */
		corner_layer_top.index = 5;
		corner_layer_bottom.index = 6;
		corner_is_inited = 1;
	}

}

static u32 check_mmu_isr(struct dpu_context *ctx, u32 reg_val)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 mmu_mask = (DISPC_INT_MMU_VAOR_RD_MASK |
			DISPC_INT_MMU_VAOR_WR_MASK |
			DISPC_INT_MMU_INV_RD_MASK |
			DISPC_INT_MMU_INV_WR_MASK);
	u32 val = reg_val & mmu_mask;
	int i;

	if (val) {
		if (val & DISPC_INT_MMU_INV_RD_MASK)
			pr_err("iommu err: 0x%04x invalid read, addr: 0x%08x\n",
				val, reg->mmu_inv_addr_rd);
		if (val & DISPC_INT_MMU_INV_WR_MASK)
			pr_err("iommu err: 0x%04x invalid write, addr: 0x%08x\n",
				val, reg->mmu_inv_addr_wr);
		if (val & DISPC_INT_MMU_VAOR_RD_MASK)
			pr_err("iommu err: 0x%04x va out of range read, addr: 0x%08x\n",
				val, reg->mmu_vaor_addr_rd);
		if (val & DISPC_INT_MMU_VAOR_WR_MASK)
			pr_err("iommu err: 0x%04x va out of range write, addr: 0x%08x\n",
				val, reg->mmu_vaor_addr_wr);

		for (i = 0; i < 8; i++) {
			if (reg->layers[i].ctrl & 0x1) {
				/*
				 * if previous layer value hasn't printed, we
				 * should skip it this time to prevent from
				 * overwriting dump_reg value.
				 */
				if (dump_reg[i][0] == 0x0) {
					dump_reg[i][0] = reg->layers[i].ctrl;
					dump_reg[i][1] = reg->layers[i].addr[0];
					dump_reg[i][2] = reg->layers[i].addr[1];
					dump_reg[i][3] = reg->layers[i].addr[2];
				} else {
					pr_err("iommu err: skip dump layer.\n");
					return val;
				}
			}
		}

		tasklet_schedule(&dump_task);

		/* panic("iommu panic\n"); */
	}

	return val;
}

static u32 dpu_isr(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 int_mask = 0;
	u32 reg_val;

	reg_val = reg->dpu_int_sts;

	/* disable err interrupt */
	if (reg_val & DISPC_INT_ERR_MASK)
		int_mask |= DISPC_INT_ERR_MASK;

	/* dpu update done isr */
	if (reg_val & DISPC_INT_UPDATE_DONE_MASK) {
		evt_update = true;
		wake_up_interruptible_all(&wait_queue);
	}

	/* dpu vsync isr */
	if (reg_val & DISPC_INT_DPI_VSYNC_MASK) {
		/* write back feature */
		if (vsync_count == max_vsync_count && wb_en) {
			//dpu_write_back(ctx, region, 1);
			schedule_work(&ctx->update_work);
		}
		vsync_count++;
	}

	/* dpu stop done isr */
	if (reg_val & DISPC_INT_DONE_MASK) {
		evt_stop = true;
		wake_up_interruptible_all(&wait_queue);
	}

	/* dpu write back done isr */
	if (reg_val & DISPC_INT_WB_DONE_MASK) {
		wb_en = false;
		/*
		 * The write back is a time-consuming operation. If there is a
		 * flip occurs before write back done, the write back buffer is
		 * no need to display. Or the new frame will be covered by the
		 * write back buffer, which is not we wanted.
		 */
		if (vsync_count > max_vsync_count) {
			dpu_clean_all(ctx);
			//dpu_layer(ctx, &wb_layer);
			schedule_work(&ctx->update_work);
			/*reg_val |= DISPC_INT_FENCE_SIGNAL_REQUEST;*/
		}
		pr_debug("wb done\n");
	}

	/* dpu write back error isr */
	if (reg_val & DISPC_INT_WB_FAIL_MASK) {
		pr_err("dpu write back fail\n");
		/*give a new chance to write back*/
		wb_en = true;
		vsync_count = 0;
	}

	/* dpu ifbc payload error isr */
	if (reg_val & DISPC_INT_FBC_PLD_ERR_MASK) {
		int_mask |= DISPC_INT_FBC_PLD_ERR_MASK;
		pr_err("dpu ifbc payload error\n");
	}

	/* dpu ifbc header error isr */
	if (reg_val & DISPC_INT_FBC_HDR_ERR_MASK) {
		int_mask |= DISPC_INT_FBC_HDR_ERR_MASK;
		pr_err("dpu ifbc header error\n");
	}

	int_mask |= check_mmu_isr(ctx, reg_val);

	reg->dpu_int_clr = reg_val;
	reg->dpu_int_en &= ~int_mask;

	return reg_val;
}

static int dpu_wait_stop_done(struct dpu_context *ctx)
{
	int rc;

	if (ctx->is_stopped)
		return 0;

	/* wait for stop done interrupt */
	rc = wait_event_interruptible_timeout(wait_queue, evt_stop,
					       msecs_to_jiffies(500));
	evt_stop = false;

	ctx->is_stopped = true;

	if (!rc) {
		/* time out */
		pr_err("dpu wait for stop done time out!\n");
		return -1;
	}

	return 0;
}

static int dpu_wait_update_done(struct dpu_context *ctx)
{
	int rc;

	/* clear the event flag before wait */
	evt_update = false;

	/* wait for reg update done interrupt */
	rc = wait_event_interruptible_timeout(wait_queue, evt_update,
					       msecs_to_jiffies(500));

	if (!rc) {
		/* time out */
		pr_err("dpu wait for reg update done time out!\n");
		return -1;
	}

	return 0;
}

static void dpu_stop(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	if (ctx->if_type == SPRD_DISPC_IF_DPI)
		reg->dpu_ctrl |= BIT(1);

	dpu_wait_stop_done(ctx);

	pr_info("dpu stop\n");
}

static void dpu_run(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_ctrl |= BIT(0);

	ctx->is_stopped = false;

	pr_info("dpu run\n");

	if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		/*
		 * If the panel read GRAM speed faster than
		 * DSI write GRAM speed, it will display some
		 * mass on screen when backlight on. So wait
		 * a TE period after flush the GRAM.
		 */
		if (!panel_ready) {
			dpu_wait_stop_done(ctx);
			/* wait for TE again */
			mdelay(20);
			panel_ready = true;
		}
	}
}

#if 0
static void dpu_write_back(struct dpu_context *ctx,
		struct wb_region region[], u8 count)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u16 w, h;

	if (count == 0)
		return;

	w = reg->blend_size & 0xFFFF;
	h = reg->blend_size >> 16;

	wb_layer.dst_w = w;
	wb_layer.dst_h = h;
	wb_layer.compression = wb_xfbc_en;
	wb_layer.header_size_r = XFBC8888_HEADER_SIZE(w, h);
	wb_layer.pitch[0] = ALIGN(w, 8) * 4;

	reg->region[0].pos = 0;
	reg->region[0].size = (w >> 3) | ((h >> 3) << 16);
	reg->wb_ctrl = BIT(0);
	reg->wb_pitch = ALIGN(w, 8);

	if (wb_xfbc_en) {
		reg->wb_cfg = (2 << 1) | BIT(0);
		reg->wb_base_addr = wb_layer.iova_plane[0] +
				wb_layer.header_size_r;
	} else {
		reg->wb_cfg = 0;
		reg->wb_base_addr = wb_layer.iova_plane[0];
	}
}

static void writeback_update_handler(struct work_struct *data)
{
	int ret;
	struct dpu_context *ctx =
		container_of(data, struct dpu_context, update_work);
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	ret = down_trylock(&ctx->refresh_lock);
	if (ret != 1) {
		reg->dpu_ctrl |= BIT(2);
		dpu_wait_update_done(ctx);
		up(&ctx->refresh_lock);
	} else
		pr_debug("cannot acquire lock for wb_lock\n");
}

static int dpu_write_back_config(struct dpu_context *ctx)
{
	int ret;
	static int need_config = 1;
	struct panel_info *panel = ctx->panel;
	u32 wb_addr_v;
	size_t wb_buf_size;
	struct sprd_dpu *dpu =
		(struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);

	if (!need_config) {
		pr_debug("write back has configed\n");
		return 0;
	}

	wb_buf_size = XFBC8888_BUFFER_SIZE(panel->width, panel->height);
	ret = sprd_dpu_wb_buf_alloc(dpu, ION_HEAP_ID_MASK_FB,
					&wb_buf_size, &(wb_addr_v));
	if (ret)
		return -1;

	wb_layer.hwlayer_id = 7;
	wb_layer.n_planes = 1;
	wb_layer.alpha = 0xff;
	wb_layer.format = DRM_FORMAT_ABGR8888;
	wb_layer.iova_plane[0] = wb_addr_v;

	max_vsync_count = 0;
	need_config = 0;

	pr_info("wb_xfbc_en = %d\n", wb_xfbc_en);
	INIT_WORK(&ctx->update_work, writeback_update_handler);

	return 0;
}
#endif

static void dump_layer_task_func(unsigned long data)
{
	int i;

	for (i = 7; i >= 0; i--) {
		if (dump_reg[i][0] & 0x1)
			pr_info("iommu err: layer%d ctrl 0x%08x addr 0x%08x 0x%08x 0x%08x\n",
				i, dump_reg[i][0], dump_reg[i][1],
				dump_reg[i][2], dump_reg[i][3]);

		dump_reg[i][0] = 0;
	}
}

static void dump_layer_task_init(void)
{
	static bool is_inited;

	if (is_inited)
		return;

	is_inited = true;
	tasklet_init(&dump_task, dump_layer_task_func, 0);
}

static int dpu_init(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 size;

	/* set bg color */
	reg->bg_color = 0;

	/* set dpu output size */
	size = (ctx->vm.vactive << 16) | ctx->vm.hactive;
	reg->panel_size = size;
	reg->blend_size = size;

	reg->dpu_cfg0 = BIT(4) | BIT(5);
	prev_y2r_coef = 3;

	reg->dpu_cfg1 = 0x004466da;
	reg->dpu_cfg2 = 0;

	if (ctx->is_stopped)
		dpu_clean_all(ctx);

	reg->mmu_en = 0;
	reg->mmu_min_ppn1 = 0;
	reg->mmu_ppn_range1 = 0xffff;
	reg->mmu_min_ppn2 = 0;
	reg->mmu_ppn_range2 = 0xffff;
	reg->mmu_vpn_range = 0x1ffff;

	reg->dpu_int_clr = 0xffff;

	dpu_enhance_reload(ctx);

	//dpu_write_back_config(ctx);

	dpu_corner_init(ctx);

	dump_layer_task_init();

	return 0;
}

static void dpu_uninit(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_int_en = 0;
	reg->dpu_int_clr = 0xff;

	panel_ready = false;
}

enum {
	DPU_LAYER_FORMAT_YUV422_2PLANE,
	DPU_LAYER_FORMAT_YUV420_2PLANE,
	DPU_LAYER_FORMAT_YUV420_3PLANE,
	DPU_LAYER_FORMAT_ARGB8888,
	DPU_LAYER_FORMAT_RGB565,
	DPU_LAYER_FORMAT_XFBC_ARGB8888 = 8,
	DPU_LAYER_FORMAT_XFBC_RGB565,
	DPU_LAYER_FORMAT_MAX_TYPES,
};

enum {
	DPU_LAYER_ROTATION_0,
	DPU_LAYER_ROTATION_90,
	DPU_LAYER_ROTATION_180,
	DPU_LAYER_ROTATION_270,
	DPU_LAYER_ROTATION_0_M,
	DPU_LAYER_ROTATION_90_M,
	DPU_LAYER_ROTATION_180_M,
	DPU_LAYER_ROTATION_270_M,
};

static u32 to_dpu_rotation(u32 angle)
{
	u32 rot = DPU_LAYER_ROTATION_0;

	switch (angle) {
	case 0:
	case DRM_MODE_ROTATE_0:
		rot = DPU_LAYER_ROTATION_0;
		break;
	case DRM_MODE_ROTATE_90:
		rot = DPU_LAYER_ROTATION_90;
		break;
	case DRM_MODE_ROTATE_180:
		rot = DPU_LAYER_ROTATION_180;
		break;
	case DRM_MODE_ROTATE_270:
		rot = DPU_LAYER_ROTATION_270;
		break;
	case DRM_MODE_REFLECT_Y:
		rot = DPU_LAYER_ROTATION_180_M;
		break;
	case (DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_90):
		rot = DPU_LAYER_ROTATION_90_M;
		break;
	case DRM_MODE_REFLECT_X:
		rot = DPU_LAYER_ROTATION_0_M;
		break;
	case (DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_90):
		rot = DPU_LAYER_ROTATION_270_M;
		break;
	default:
		pr_err("rotation convert unsupport angle (drm)= 0x%x\n", angle);
		break;
	}

	return rot;
}

static u32 dpu_img_ctrl(u32 format, u32 blending, u32 compression, u32 rotation)
{
	int reg_val = 0;

	/* layer enable */
	reg_val |= BIT(0);

	switch (format) {
	case DRM_FORMAT_BGRA8888:
		/* BGRA8888 -> ARGB8888 */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		/* RGBA8888 -> ABGR8888 */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
	case DRM_FORMAT_ABGR8888:
		/* rb switch */
		reg_val |= BIT(10);
	case DRM_FORMAT_ARGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_XBGR8888:
		/* rb switch */
		reg_val |= BIT(10);
	case DRM_FORMAT_XRGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_BGR565:
		/* rb switch */
		reg_val |= BIT(10);
	case DRM_FORMAT_RGB565:
		if (compression)
			/* XFBC-RGB565 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_RGB565 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_RGB565 << 4);
		break;
	case DRM_FORMAT_NV12:
		/* 2-Lane: Yuv420 */
		reg_val |= DPU_LAYER_FORMAT_YUV420_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_NV21:
		/* 2-Lane: Yuv420 */
		reg_val |= DPU_LAYER_FORMAT_YUV420_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	case DRM_FORMAT_NV16:
		/* 2-Lane: Yuv422 */
		reg_val |= DPU_LAYER_FORMAT_YUV422_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	case DRM_FORMAT_NV61:
		/* 2-Lane: Yuv422 */
		reg_val |= DPU_LAYER_FORMAT_YUV422_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_YUV420:
		reg_val |= DPU_LAYER_FORMAT_YUV420_3PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_YVU420:
		reg_val |= DPU_LAYER_FORMAT_YUV420_3PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	default:
		pr_err("error: invalid format %c%c%c%c\n", format,
						format >> 8,
						format >> 16,
						format >> 24);
		break;
	}

	switch (blending) {
	case DRM_MODE_BLEND_PIXEL_NONE:
		/* don't do blending, maybe RGBX */
		/* alpha mode select - layer alpha */
		reg_val |= BIT(2);
		break;
	case DRM_MODE_BLEND_COVERAGE:
		/* alpha mode select - combo alpha */
		reg_val |= BIT(3);
		/*Normal mode*/
		reg_val &= (~BIT(16));
		break;
	case DRM_MODE_BLEND_PREMULTI:
		/* alpha mode select - combo alpha */
		reg_val |= BIT(3);
		/*Pre-mult mode*/
		reg_val |= BIT(16);
		break;
	default:
		/* alpha mode select - layer alpha */
		reg_val |= BIT(2);
		break;
	}

	rotation = to_dpu_rotation(rotation);
	reg_val |= (rotation & 0x7) << 20;

	return reg_val;
}

static int check_layer_y2r_coef(struct sprd_dpu_layer layers[], u8 count)
{
	int i;

	for (i = (count - 1); i >= 0; i--) {
		switch (layers[i].format) {
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
		case DRM_FORMAT_NV16:
		case DRM_FORMAT_NV61:
		case DRM_FORMAT_YUV420:
		case DRM_FORMAT_YVU420:
			if (layers[i].y2r_coef == prev_y2r_coef)
				return -1;

			/* need to config dpu y2r coef */
			prev_y2r_coef = layers[i].y2r_coef;
			return prev_y2r_coef;
		default:
			break;
		}
	}

	/* not find yuv layer */
	return -1;
}

static void dpu_clean_all(struct dpu_context *ctx)
{
	int i;
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	for (i = 0; i < 8; i++)
		reg->layers[i].ctrl = 0;
}

static void dpu_bgcolor(struct dpu_context *ctx, u32 color)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);

	reg->bg_color = color;

	dpu_clean_all(ctx);

	if ((ctx->if_type == SPRD_DISPC_IF_DPI) && !ctx->is_stopped) {
		reg->dpu_ctrl |= BIT(2);
		dpu_wait_update_done(ctx);
	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		reg->dpu_ctrl |= BIT(0);
		ctx->is_stopped = false;
	}
}

static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_dpu_layer *hwlayer)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct layer_reg *layer;
	u32 addr, size, offset, wd, i, ctrl;

	layer = &reg->layers[hwlayer->index];
	offset = (hwlayer->dst_x & 0xffff) | ((hwlayer->dst_y) << 16);

	if (hwlayer->pallete_en) {
		size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);
		layer->pos = offset;
		layer->size = size;
		layer->alpha = hwlayer->alpha;
		layer->pallete = hwlayer->pallete_color;

		/* pallete layer enable */
		layer->ctrl = 0x1005;

		pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
			hwlayer->dst_x, hwlayer->dst_y,
			hwlayer->dst_w, hwlayer->dst_h);
		return;
	}

	if (hwlayer->src_w && hwlayer->src_h)
		size = (hwlayer->src_w & 0xffff) | ((hwlayer->src_h) << 16);
	else
		size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);

	for (i = 0; i < hwlayer->planes; i++) {
		addr = hwlayer->addr[i];

		/* dpu r2p0 just support xfbc-rgb */
		if (hwlayer->xfbc)
			addr += hwlayer->header_size_r;

		if (addr % 16)
			pr_err("layer addr[%d] is not 16 bytes align, it's 0x%08x\n",
				i, addr);
		layer->addr[i] = addr;
	}

	layer->pos = offset;
	layer->size = size;
	layer->crop_start = (hwlayer->src_y << 16) | hwlayer->src_x;
	layer->alpha = hwlayer->alpha;

	wd = drm_format_plane_cpp(hwlayer->format, 0);
	if (wd == 0) {
		pr_err("layer[%d] bytes per pixel is invalid\n", hwlayer->index);
		return;
	}

	if (hwlayer->planes == 3)
		/* UV pitch is 1/2 of Y pitch*/
		layer->pitch = (hwlayer->pitch[0] / wd) |
				(hwlayer->pitch[0] / wd << 15);
	else
		layer->pitch = hwlayer->pitch[0] / wd;

	ctrl = dpu_img_ctrl(hwlayer->format, hwlayer->blending,
		hwlayer->xfbc, hwlayer->rotation);

	/*
	 * if layer0 blend mode is premult mode, and layer alpha value
	 * is 0xff, use layer alpha.
	 */
	if (hwlayer->index == 0 &&
		(hwlayer->blending == DRM_MODE_BLEND_PREMULTI) &&
		(hwlayer->alpha == 0xff)) {
		ctrl &= ~BIT(3);
		ctrl |= BIT(2);
	}

	layer->ctrl = ctrl;

	pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
				hwlayer->dst_x, hwlayer->dst_y,
				hwlayer->dst_w, hwlayer->dst_h);
	pr_debug("start_x = %d, start_y = %d, start_w = %d, start_h = %d\n",
				hwlayer->src_x, hwlayer->src_y,
				hwlayer->src_w, hwlayer->src_h);
}

static void dpu_scaling(struct dpu_context *ctx,
			struct sprd_dpu_layer layers[], u8 count)
{
	int i;
	struct sprd_dpu_layer *top_layer;

	if (mode_changed) {
		top_layer = &layers[count - 1];
		pr_debug("------------------------------------\n");
		for (i = 0; i < count; i++) {
			pr_debug("layer[%d] : %dx%d --- (%d)\n", i,
				layers[i].dst_w, layers[i].dst_h,
				scale_copy.in_w);
		}

		if  (top_layer->dst_w <= scale_copy.in_w) {
			dpu_sr_config(ctx);
			mode_changed = false;

			pr_info("do scaling enhace: 0x%x, top layer(%dx%d)\n",
				enhance_en, top_layer->dst_w,
				top_layer->dst_h);
		}
	}
}

static void dpu_flip(struct dpu_context *ctx,
		     struct sprd_dpu_layer layers[], u8 count)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int i;
	int y2r_coef;

	/*
	 * Make sure the dpu is in stop status. DPU_R2P0 has no shadow
	 * registers in EDPI mode. So the config registers can only be
	 * updated in the rising edge of DPU_RUN bit.
	 */
	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);

	/* set Y2R conversion coef */
	y2r_coef = check_layer_y2r_coef(layers, count);
	if (y2r_coef >= 0) {
		/* write dpu_cfg0 register after dpu is in idle status */
		if (ctx->if_type == SPRD_DISPC_IF_DPI)
			dpu_stop(ctx);

		reg->dpu_cfg0 &= ~(0x7 << 4);
		reg->dpu_cfg0 |= (y2r_coef << 4);
	}

	/* reset the bgcolor to black */
	reg->bg_color = 0;

	/* disable all the layers */
	dpu_clean_all(ctx);

	/* to check if dpu need scaling the frame for SR */
	dpu_scaling(ctx, layers, count);

	/* start configure dpu layers */
	for (i = 0; i < count; i++)
		dpu_layer(ctx, &layers[i]);

	/* special case for round corner */
	if (sprd_corner_support) {
		dpu_layer(ctx, &corner_layer_top);
		dpu_layer(ctx, &corner_layer_bottom);
	}

	/* update trigger and wait */
	if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		if (!ctx->is_stopped) {
			reg->dpu_ctrl |= BIT(2);
			dpu_wait_update_done(ctx);
		} else if (y2r_coef >= 0) {
			reg->dpu_ctrl |= BIT(0);
			ctx->is_stopped = false;
			pr_info("dpu start\n");
		}

		reg->dpu_int_en |= DISPC_INT_ERR_MASK;

	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		reg->dpu_ctrl |= BIT(0);

		ctx->is_stopped = false;
	}

	/*
	 * If the following interrupt was disabled in isr,
	 * re-enable it.
	 */
	reg->dpu_int_en |= DISPC_INT_FBC_PLD_ERR_MASK |
			   DISPC_INT_FBC_HDR_ERR_MASK |
			   DISPC_INT_MMU_VAOR_RD_MASK |
			   DISPC_INT_MMU_VAOR_WR_MASK |
			   DISPC_INT_MMU_INV_RD_MASK |
			   DISPC_INT_MMU_INV_WR_MASK;
}

static void dpu_dpi_init(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 int_mask = 0;

	if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		/* use dpi as interface */
		reg->dpu_cfg0 &= ~BIT(0);

		/* disable Halt function for SPRD DSI */
		reg->dpi_ctrl &= ~BIT(16);

		/* select te from external pad */
		reg->dpi_ctrl |= BIT(10);

		/* set dpi timing */
		reg->dpi_h_timing = (ctx->vm.hsync_len << 0) |
				    (ctx->vm.hback_porch << 8) |
				    (ctx->vm.hfront_porch << 20);
		reg->dpi_v_timing = (ctx->vm.vsync_len << 0) |
				    (ctx->vm.vback_porch << 8) |
				    (ctx->vm.vfront_porch << 20);
		if (ctx->vm.vsync_len + ctx->vm.vback_porch < 32)
			pr_warn("Warning: (vsync + vbp) < 32, "
				"underflow risk!\n");

		/* enable dpu update done INT */
		int_mask |= DISPC_INT_UPDATE_DONE_MASK;
		/* enable dpu DONE  INT */
		int_mask |= DISPC_INT_DONE_MASK;
		/* enable dpu dpi vsync */
		int_mask |= DISPC_INT_DPI_VSYNC_MASK;
		/* enable dpu TE INT */
		int_mask |= DISPC_INT_TE_MASK;
		/* enable underflow err INT */
		int_mask |= DISPC_INT_ERR_MASK;
		/* enable write back done INT */
		int_mask |= DISPC_INT_WB_DONE_MASK;
		/* enable write back fail INT */
		int_mask |= DISPC_INT_WB_FAIL_MASK;

	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		/* use edpi as interface */
		reg->dpu_cfg0 |= BIT(0);

		/* use external te */
		reg->dpi_ctrl |= BIT(10);

		/* enable te */
		reg->dpi_ctrl |= BIT(8);

		/* enable stop DONE INT */
		int_mask |= DISPC_INT_DONE_MASK;
		/* enable TE INT */
		int_mask |= DISPC_INT_TE_MASK;
	}

	/* enable ifbc payload error INT */
	int_mask |= DISPC_INT_FBC_PLD_ERR_MASK;
	/* enable ifbc header error INT */
	int_mask |= DISPC_INT_FBC_HDR_ERR_MASK;
	/* enable iommu va out of range read error INT */
	int_mask |= DISPC_INT_MMU_VAOR_RD_MASK;
	/* enable iommu va out of range write error INT */
	int_mask |= DISPC_INT_MMU_VAOR_WR_MASK;
	/* enable iommu invalid read error INT */
	int_mask |= DISPC_INT_MMU_INV_RD_MASK;
	/* enable iommu invalid write error INT */
	int_mask |= DISPC_INT_MMU_INV_WR_MASK;

	reg->dpu_int_en = int_mask;
}

static void enable_vsync(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_int_en |= DISPC_INT_DPI_VSYNC_MASK;
}

static void disable_vsync(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_int_en &= ~DISPC_INT_DPI_VSYNC_MASK;
}

static void dpu_enhance_backup(u32 id, void *param)
{
	u32 *p;

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p = param;
		enhance_en |= *p;
		pr_info("enhance enable backup: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_DISABLE:
		p = param;
		enhance_en &= ~(*p);
		if (*p & BIT(1))
			memset(&epf_copy, 0, sizeof(epf_copy));
		pr_info("enhance disable backup: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_SCL:
		memcpy(&scale_copy, param, sizeof(scale_copy));
		enhance_en |= BIT(0);
		pr_info("enhance scaling backup\n");
		break;
	case ENHANCE_CFG_ID_HSV:
		memcpy(&hsv_copy, param, sizeof(hsv_copy));
		enhance_en |= BIT(2);
		pr_info("enhance hsv backup\n");
		break;
	case ENHANCE_CFG_ID_CM:
		memcpy(&cm_copy, param, sizeof(cm_copy));
		enhance_en |= BIT(3);
		pr_info("enhance cm backup\n");
		break;
	case ENHANCE_CFG_ID_SLP:
		memcpy(&slp_copy, param, sizeof(slp_copy));
		enhance_en |= BIT(4);
		pr_info("enhance slp backup\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		memcpy(&gamma_copy, param, sizeof(gamma_copy));
		enhance_en |= BIT(5);
		pr_info("enhance gamma backup\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		memcpy(&epf_copy, param, sizeof(epf_copy));
		if ((slp_copy.brightness > SLP_BRIGHTNESS_THRESHOLD) ||
			(enhance_en & BIT(0))) {
			enhance_en |= BIT(1);
			pr_info("enhance epf backup\n");
		}
		break;
	default:
		break;
	}
}

static void dpu_epf_set(struct dpu_reg *reg, struct epf_cfg *epf)
{
	reg->epf_epsilon = (epf->epsilon1 << 16) | epf->epsilon0;
	reg->epf_gain0_3 = (epf->gain3 << 24) | (epf->gain2 << 16) |
			   (epf->gain1 << 8) | epf->gain0;
	reg->epf_gain4_7 = (epf->gain7 << 24) | (epf->gain6 << 16) |
			   (epf->gain5 << 8) | epf->gain4;
	reg->epf_diff = (epf->max_diff << 8) | epf->min_diff;
}

static void dpu_enhance_set(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct scale_cfg *scale;
	struct cm_cfg *cm;
	struct slp_cfg *slp;
	struct gamma_lut *gamma;
	struct hsv_lut *hsv;
	struct epf_cfg *epf;
	u32 *p, i;

	if (!ctx->is_inited) {
		dpu_enhance_backup(id, param);
		return;
	}

	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p = param;
		reg->dpu_enhance_cfg |= *p;
		pr_info("enhance module enable: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_DISABLE:
		p = param;
		reg->dpu_enhance_cfg &= ~(*p);
		if (*p & BIT(1))
			memset(&epf_copy, 0, sizeof(epf_copy));
		pr_info("enhance module disable: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_SCL:
		memcpy(&scale_copy, param, sizeof(scale_copy));
		scale = &scale_copy;
		reg->blend_size = (scale->in_h << 16) | scale->in_w;
		reg->dpu_enhance_cfg |= BIT(0);
		pr_info("enhance scaling: %ux%u\n", scale->in_w, scale->in_h);
		break;
	case ENHANCE_CFG_ID_HSV:
		memcpy(&hsv_copy, param, sizeof(hsv_copy));
		hsv = &hsv_copy;
		for (i = 0; i < 360; i++) {
			reg->hsv_lut_addr = i;
			udelay(1);
			reg->hsv_lut_wdata = (hsv->table[i].sat << 16) |
						hsv->table[i].hue;
		}
		reg->dpu_enhance_cfg |= BIT(2);
		pr_info("enhance hsv set\n");
		break;
	case ENHANCE_CFG_ID_CM:
		memcpy(&cm_copy, param, sizeof(cm_copy));
		cm = &cm_copy;
		reg->cm_coef01_00 = (cm->coef01 << 16) | cm->coef00;
		reg->cm_coef03_02 = (cm->coef03 << 16) | cm->coef02;
		reg->cm_coef11_10 = (cm->coef11 << 16) | cm->coef10;
		reg->cm_coef13_12 = (cm->coef13 << 16) | cm->coef12;
		reg->cm_coef21_20 = (cm->coef21 << 16) | cm->coef20;
		reg->cm_coef23_22 = (cm->coef23 << 16) | cm->coef22;
		reg->dpu_enhance_cfg |= BIT(3);
		pr_info("enhance cm set\n");
		break;
	case ENHANCE_CFG_ID_SLP:
		memcpy(&slp_copy, param, sizeof(slp_copy));
		slp = &slp_copy;
		reg->slp_cfg0 = (slp->second_bright_factor << 24) |
				(slp->brightness_step << 16) |
				(slp->conversion_matrix << 8) |
				slp->brightness;
		reg->slp_cfg1 = (slp->first_max_bright_th << 8) |
				slp->first_percent_th;
		reg->dpu_enhance_cfg |= BIT(4);
		pr_info("enhance slp set\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		memcpy(&gamma_copy, param, sizeof(gamma_copy));
		gamma = &gamma_copy;
		for (i = 0; i < 256; i++) {
			reg->gamma_lut_addr = i;
			udelay(1);
			reg->gamma_lut_wdata = (gamma->r[i] << 20) |
						(gamma->g[i] << 10) |
						gamma->b[i];
			pr_debug("0x%02x: r=%u, g=%u, b=%u\n", i,
				gamma->r[i], gamma->g[i], gamma->b[i]);
		}
		reg->dpu_enhance_cfg |= BIT(5);
		pr_info("enhance gamma set\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		memcpy(&epf_copy, param, sizeof(epf_copy));
		if ((slp_copy.brightness > SLP_BRIGHTNESS_THRESHOLD) ||
			(enhance_en & BIT(0))) {
			epf = &epf_copy;
			dpu_epf_set(reg, epf);
			reg->dpu_enhance_cfg |= BIT(1);
			pr_info("enhance epf set\n");
			break;
		}
		return;
	default:
		break;
	}

	if ((ctx->if_type == SPRD_DISPC_IF_DPI) && !ctx->is_stopped) {
		reg->dpu_ctrl |= BIT(2);
		dpu_wait_update_done(ctx);
	} else if ((ctx->if_type == SPRD_DISPC_IF_EDPI) && panel_ready) {
		/*
		 * In EDPI mode, we need to wait panel initializatin
		 * completed. Otherwise, the dpu enhance settings may
		 * start before panel initialization.
		 */
		reg->dpu_ctrl |= BIT(0);
		ctx->is_stopped = false;
	}

	enhance_en = reg->dpu_enhance_cfg;
}

static void dpu_enhance_get(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct scale_cfg *scale;
	struct epf_cfg *ep;
	struct slp_cfg *slp;
	struct gamma_lut *gamma;
	u32 *p32, i, val;

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p32 = param;
		*p32 = reg->dpu_enhance_cfg;
		pr_info("enhance module enable get\n");
		break;
	case ENHANCE_CFG_ID_SCL:
		scale = param;
		val = reg->blend_size;
		scale->in_w = val & 0xffff;
		scale->in_h = val >> 16;
		pr_info("enhance scaling get\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		ep = param;

		val = reg->epf_epsilon;
		ep->epsilon0 = val;
		ep->epsilon1 = val >> 16;

		val = reg->epf_gain0_3;
		ep->gain0 = val;
		ep->gain1 = val >> 8;
		ep->gain2 = val >> 16;
		ep->gain3 = val >> 24;

		val = reg->epf_gain4_7;
		ep->gain4 = val;
		ep->gain5 = val >> 8;
		ep->gain6 = val >> 16;
		ep->gain7 = val >> 24;

		val = reg->epf_diff;
		ep->min_diff = val;
		ep->max_diff = val >> 8;
		pr_info("enhance epf get\n");
		break;
	case ENHANCE_CFG_ID_HSV:
		dpu_stop(ctx);
		p32 = param;
		for (i = 0; i < 360; i++) {
			reg->hsv_lut_addr = i;
			udelay(1);
			*p32++ = reg->hsv_lut_rdata;
		}
		dpu_run(ctx);
		pr_info("enhance hsv get\n");
		break;
	case ENHANCE_CFG_ID_CM:
		p32 = param;
		*p32++ = reg->cm_coef01_00;
		*p32++ = reg->cm_coef03_02;
		*p32++ = reg->cm_coef11_10;
		*p32++ = reg->cm_coef13_12;
		*p32++ = reg->cm_coef21_20;
		*p32++ = reg->cm_coef23_22;
		pr_info("enhance cm get\n");
		break;
	case ENHANCE_CFG_ID_SLP:
		slp = param;

		val = reg->slp_cfg0;
		slp->brightness = val;
		slp->conversion_matrix = val >> 8;
		slp->brightness_step = val >> 16;
		slp->second_bright_factor = val >> 24;

		val = reg->slp_cfg1;
		slp->first_percent_th = val;
		slp->first_max_bright_th = val >> 8;
		pr_info("enhance slp get\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		dpu_stop(ctx);
		gamma = param;
		for (i = 0; i < 256; i++) {
			reg->gamma_lut_addr = i;
			udelay(1);
			val = reg->gamma_lut_rdata;
			gamma->r[i] = (val >> 20) & 0x3FF;
			gamma->g[i] = (val >> 10) & 0x3FF;
			gamma->b[i] = val & 0x3FF;
			pr_debug("0x%02x: r=%u, g=%u, b=%u\n", i,
				gamma->r[i], gamma->g[i], gamma->b[i]);
		}
		dpu_run(ctx);
		pr_info("enhance gamma get\n");
		break;
	default:
		break;
	}
}

static void dpu_enhance_reload(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct scale_cfg *scale;
	struct cm_cfg *cm;
	struct slp_cfg *slp;
	struct gamma_lut *gamma;
	struct hsv_lut *hsv;
	struct epf_cfg *epf;
	int i;

	if (enhance_en & BIT(0)) {
		scale = &scale_copy;
		reg->blend_size = (scale->in_h << 16) | scale->in_w;
		pr_info("enhance scaling from %ux%u to %ux%u\n", scale->in_w,
			scale->in_h, ctx->vm.hactive, ctx->vm.vactive);
	}

	if (enhance_en & BIT(1)) {
		epf = &epf_copy;
		dpu_epf_set(reg, epf);
		pr_info("enhance epf reload\n");
	}

	if (enhance_en & BIT(2)) {
		hsv = &hsv_copy;
		for (i = 0; i < 360; i++) {
			reg->hsv_lut_addr = i;
			udelay(1);
			reg->hsv_lut_wdata = (hsv->table[i].sat << 16) |
						hsv->table[i].hue;
		}
		pr_info("enhance hsv reload\n");
	}

	if (enhance_en & BIT(3)) {
		cm = &cm_copy;
		reg->cm_coef01_00 = (cm->coef01 << 16) | cm->coef00;
		reg->cm_coef03_02 = (cm->coef03 << 16) | cm->coef02;
		reg->cm_coef11_10 = (cm->coef11 << 16) | cm->coef10;
		reg->cm_coef13_12 = (cm->coef13 << 16) | cm->coef12;
		reg->cm_coef21_20 = (cm->coef21 << 16) | cm->coef20;
		reg->cm_coef23_22 = (cm->coef23 << 16) | cm->coef22;
		pr_info("enhance cm reload\n");
	}

	if (enhance_en & BIT(4)) {
		slp = &slp_copy;
		reg->slp_cfg0 = (slp->second_bright_factor << 24) |
				(slp->brightness_step << 16) |
				(slp->conversion_matrix << 8) |
				slp->brightness;
		reg->slp_cfg1 = (slp->first_max_bright_th << 8) |
				slp->first_percent_th;
		pr_info("enhance slp reload\n");
	}

	if (enhance_en & BIT(5)) {
		gamma = &gamma_copy;
		for (i = 0; i < 256; i++) {
			reg->gamma_lut_addr = i;
			udelay(1);
			reg->gamma_lut_wdata = (gamma->r[i] << 20) |
						(gamma->g[i] << 10) |
						gamma->b[i];
			pr_debug("0x%02x: r=%u, g=%u, b=%u\n", i,
				gamma->r[i], gamma->g[i], gamma->b[i]);
		}
		pr_info("enhance gamma reload\n");
	}

	reg->dpu_enhance_cfg = enhance_en;
}

static void dpu_sr_config(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->blend_size = (scale_copy.in_h << 16) | scale_copy.in_w;
	if (need_scale) {
		/* SLP is disabled mode or bypass mode */
		if (slp_copy.brightness <= SLP_BRIGHTNESS_THRESHOLD) {

		/*
		 * valid range of gain3 is [128,255];dpu_scaling maybe
		 * called before epf_copy is assinged a value
		 */
			if (epf_copy.gain3 > 0) {
				dpu_epf_set(reg, &epf_copy);
				enhance_en |= BIT(1);
			}
		}
		enhance_en |= BIT(0);
		reg->dpu_enhance_cfg = enhance_en;
	} else {
		if (slp_copy.brightness <= SLP_BRIGHTNESS_THRESHOLD)
			enhance_en &= ~(BIT(1));

		enhance_en &= ~(BIT(0));
		reg->dpu_enhance_cfg = enhance_en;
	}
}


static int dpu_modeset(struct dpu_context *ctx,
		struct drm_mode_modeinfo *mode)
{
	scale_copy.in_w = mode->hdisplay;
	scale_copy.in_h = mode->vdisplay;

	if ((mode->hdisplay != ctx->vm.hactive) ||
		(mode->vdisplay != ctx->vm.vactive))
		need_scale = true;
	else
		need_scale = false;

	mode_changed = true;
	pr_info("begin switch to %u x %u\n", mode->hdisplay, mode->vdisplay);

	return 0;
}

static const u32 primary_fmts[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565, DRM_FORMAT_BGR565,
	DRM_FORMAT_NV12, DRM_FORMAT_NV21,
	DRM_FORMAT_NV16, DRM_FORMAT_NV61,
	DRM_FORMAT_YUV420, DRM_FORMAT_YVU420,
};

static int dpu_capability(struct dpu_context *ctx,
			struct dpu_capability *cap)
{
	if (!cap)
		return -EINVAL;

	cap->max_layers = 6;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);

	return 0;
}

static struct dpu_core_ops dpu_r2p0_ops = {
	.parse_dt = dpu_parse_dt,
	.version = dpu_get_version,
	.init = dpu_init,
	.uninit = dpu_uninit,
	.run = dpu_run,
	.stop = dpu_stop,
	.isr = dpu_isr,
	.ifconfig = dpu_dpi_init,
	.capability = dpu_capability,
	.flip = dpu_flip,
	.bg_color = dpu_bgcolor,
	.enable_vsync = enable_vsync,
	.disable_vsync = disable_vsync,
	.enhance_set = dpu_enhance_set,
	.enhance_get = dpu_enhance_get,
	.modeset = dpu_modeset,
	.check_raw_int = dpu_check_raw_int,
};

static struct ops_entry entry = {
	.ver = "dpu-r2p0",
	.ops = &dpu_r2p0_ops,
};

static int __init dpu_core_register(void)
{
	return dpu_core_ops_register(&entry);
}

subsys_initcall(dpu_core_register);
