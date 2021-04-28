/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#include <cabc/cabc_definition.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include "sprd_bl.h"
#include "sprd_dpu.h"
#include "sprd_dvfs_dpu.h"
#include "dpu_r4p0_corner_param.h"
#include "dpu_enhance_param.h"

#define DISPC_INT_FBC_PLD_ERR_MASK	BIT(8)
#define DISPC_INT_FBC_HDR_ERR_MASK	BIT(9)

#define XFBC8888_HEADER_SIZE(w, h) (ALIGN((ALIGN((w), 16)) * \
				(ALIGN((h), 16)) / 16, 128))
#define XFBC8888_PAYLOAD_SIZE(w, h) (ALIGN((w), 16) * ALIGN((h), 16) * 4)
#define XFBC8888_BUFFER_SIZE(w, h) (XFBC8888_HEADER_SIZE(w, h) \
				+ XFBC8888_PAYLOAD_SIZE(w, h))

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
	u32 reserved_0x01C0_0x01DC[8];
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
	u32 slp_cfg2;
	u32 slp_cfg3;
	u32 slp_lut_addr;
	u32 slp_lut_wdata;
	u32 slp_lut_rdata;
	u32 threed_lut_addr;
	u32 threed_lut_wdata;
	u32 threed_lut_rdata;
	u32 reserved_0x02E8_0x02FC[6];
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
	u32 dpu_fbc_cfg2;
	u32 reserved_0x03BC;
	u32 slp_cfg4;
	u32 slp_cfg5;
	u32 slp_cfg6;
	u32 slp_cfg7;
	u32 slp_cfg8;
	u32 slp_cfg9;
	u32 slp_cfg10;
	u32 reserved_0x3DC_0x3EC[5];
	u32 rf_ram_addr;
	u32 rf_ram_rdata_low;
	u32 rf_ram_rdata_high;
	u32 reserved_0x03FC;
	u32 cabc_hist[32];
	u32 reserved_0x047C_0x04FC[32];
	u32 corner_config;
	u32 top_corner_lut_addr;
	u32 top_corner_lut_wdata;
	u32 top_corner_lut_rdata;
	u32 bot_corner_lut_addr;
	u32 bot_corner_lut_wdata;
	u32 bot_corner_lut_rdata;
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
	u32 ltm_en: 1;
	u32 slp_mask_en: 1;
	u32 cabc_en: 1;
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

struct threed_lut {
	uint32_t value[729];
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
	u16 brightness_step;
	u8 fst_max_bright_th;
	u8 fst_max_bright_th_step[5];
	u8 hist_exb_no;
	u8 hist_exb_percent;
	u16 mask_height;
	u8 fst_pth_index[4];
	u8 hist9_index[9];
	u8 glb_x[3];
	u16 glb_s[3];
	u16 limit_hclip;
	u16 limit_lclip;
	u16 limit_clip_step;
	u8 fast_ambient_th;
	u8 scene_change_percent_th;
	u8 local_weight;
	u8 fst_pth;
	u8 cabc_endv;
	u8 cabc_startv;
};

struct dpu_cfg1 {
	u8 arqos_low;
	u8 arqos_high;
	u8 awqos_low;
	u8 awqos_high;
};

static struct epf_cfg epf = {
	.epsilon0 = 30,
	.epsilon1 = 1000,
	.gain0 = -8,
	.gain1 = 8,
	.gain2 = 32,
	.gain3 = 160,
	.gain4 = 24,
	.gain5 = 8,
	.gain6 = 32,
	.gain7 = 160,
	.max_diff = 80,
	.min_diff = 40,
};

static struct dpu_cfg1 qos_cfg = {
	.arqos_low = 0x1,
	.arqos_high = 0x7,
	.awqos_low = 0x1,
	.awqos_high = 0x7,
};

enum {
	CABC_WORKING,
	CABC_STOPPING,
	CABC_DISABLED
};

static struct scale_cfg scale_copy;
static struct cm_cfg cm_copy;
static struct slp_cfg slp_copy;
static struct gamma_lut gamma_copy;
static struct threed_lut lut3d_copy;
static struct hsv_lut hsv_copy;
static struct epf_cfg epf_copy;
static struct epf_cfg sr_epf;
static bool sr_epf_ready;
static u32 enhance_en;

static DECLARE_WAIT_QUEUE_HEAD(wait_queue);
static bool panel_ready = true;
static bool need_scale;
static bool mode_changed;
static bool evt_update;
static bool evt_stop;
static int frame_no;
static bool cabc_bl_set;
static int cabc_disable = CABC_DISABLED;
static int cabc_step0 = 8;
static int cabc_step1 = 72;
static int cabc_step2 = 28;
static int cabc_scene_change_thr = 80;
static int cabc_min_backlight = 408;
static int cabc_bl_set_delay;
static struct cabc_para cabc_para;
static struct backlight_device *bl_dev;
static int wb_en;
static int max_vsync_count;
static int vsync_count;
static struct sprd_dpu_layer wb_layer;
static int wb_xfbc_en = 1;
static int corner_radius;
static struct device_node *g_np;
module_param(wb_xfbc_en, int, 0644);
module_param(max_vsync_count, int, 0644);
module_param(cabc_disable, int, 0644);
module_param(cabc_step0, int, 0644);
module_param(cabc_step1, int, 0644);
module_param(cabc_step2, int, 0644);
module_param(cabc_scene_change_thr, int, 0644);
module_param(cabc_bl_set_delay, int, 0644);
module_param(cabc_min_backlight, int, 0644);

static void dpu_sr_config(struct dpu_context *ctx);
static void dpu_enhance_reload(struct dpu_context *ctx);
static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_dpu_layer *hwlayer);
static int dpu_cabc_trigger(struct dpu_context *ctx);

static u32 dpu_get_version(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	return reg->dpu_version;
}

static int dpu_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	int ret = 0;
	struct device_node *qos_np = NULL;

	g_np = np;

	ret = of_property_read_u32(np, "sprd,corner-radius",
					&corner_radius);
	if (!ret)
		pr_info("round corner support, radius = %d.\n",
					corner_radius);

	qos_np = of_parse_phandle(np, "sprd,qos", 0);
	if (!qos_np)
		pr_warn("can't find dpu qos cfg node\n");

	ret = of_property_read_u8(qos_np, "arqos-low",
					&qos_cfg.arqos_low);
	if (ret)
		pr_warn("read arqos-low failed, use default\n");

	ret = of_property_read_u8(qos_np, "arqos-high",
					&qos_cfg.arqos_high);
	if (ret)
		pr_warn("read arqos-high failed, use default\n");

	ret = of_property_read_u8(qos_np, "awqos-low",
					&qos_cfg.awqos_low);
	if (ret)
		pr_warn("read awqos_low failed, use default\n");

	ret = of_property_read_u8(qos_np, "awqos-high",
					&qos_cfg.awqos_high);
	if (ret)
		pr_warn("read awqos-high failed, use default\n");

	return ret;
}

static void dpu_corner_init(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int i;

	reg->corner_config = (corner_radius << 24) |
				(corner_radius << 8);

	for (i = 0; i < corner_radius; i++) {
		reg->top_corner_lut_addr = i;
		reg->top_corner_lut_wdata = corner_param[corner_radius][i];
		reg->bot_corner_lut_addr = i;
		reg->bot_corner_lut_wdata =
			corner_param[corner_radius][corner_radius - i - 1];
	}

	reg->corner_config |= (TOP_CORNER_EN | BOT_CORNER_EN);
}

static u32 dpu_isr(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 reg_val, int_mask = 0;

	reg_val = reg->dpu_int_sts;
	reg->dpu_int_clr = reg_val;

	/* disable err interrupt */
	if (reg_val & DISPC_INT_ERR_MASK)
		int_mask |= DISPC_INT_ERR_MASK;

	/* dpu update done isr */
	if (reg_val & DISPC_INT_UPDATE_DONE_MASK) {
		/* dpu dvfs feature */
		tasklet_schedule(&ctx->dvfs_task);

		evt_update = true;
		wake_up_interruptible_all(&wait_queue);
	}

	/* dpu vsync isr */
	if (reg_val & DISPC_INT_DPI_VSYNC_MASK) {
		/* write back feature */
		if ((vsync_count == max_vsync_count) && wb_en)
			schedule_work(&ctx->wb_work);

		/* cabc work */
		if ((cabc_disable != CABC_DISABLED) && (vsync_count >= 9) &&
		    (vsync_count % 2 == 0) && (vsync_count < 151)) {
			schedule_work(&ctx->cabc_work);
		}

		/* cabc update backlight */
		if (cabc_bl_set)
			schedule_work(&ctx->cabc_bl_update);

		vsync_count++;
	}

	/* dpu stop done isr */
	if (reg_val & DISPC_INT_DONE_MASK) {
		evt_stop = true;
		wake_up_interruptible_all(&wait_queue);
	}

	/* dpu write back done isr */
	if (reg_val & DISPC_INT_WB_DONE_MASK) {
		/*
		 * The write back is a time-consuming operation. If there is a
		 * flip occurs before write back done, the write back buffer is
		 * no need to display. Otherwise the new frame will be covered
		 * by the write back buffer, which is not what we wanted.
		 */
		if ((vsync_count > max_vsync_count) && wb_en) {
			wb_en = false;
			schedule_work(&ctx->wb_work);
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

	/* dpu afbc payload error isr */
	if (reg_val & DISPC_INT_FBC_PLD_ERR_MASK) {
		int_mask |= DISPC_INT_FBC_PLD_ERR_MASK;
		pr_err("dpu afbc payload error\n");
	}

	/* dpu afbc header error isr */
	if (reg_val & DISPC_INT_FBC_HDR_ERR_MASK) {
		int_mask |= DISPC_INT_FBC_HDR_ERR_MASK;
		pr_err("dpu afbc header error\n");
	}

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

static void dpu_cabc_work_func(struct work_struct *data)
{
	struct dpu_context *ctx =
		container_of(data, struct dpu_context, cabc_work);
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	down(&ctx->refresh_lock);
	dpu_cabc_trigger(ctx);

	reg->dpu_ctrl |= BIT(2);
	dpu_wait_update_done(ctx);

	up(&ctx->refresh_lock);
}

static void dpu_cabc_bl_update_func(struct work_struct *data)
{
	struct sprd_backlight *bl = bl_get_data(bl_dev);

	msleep(cabc_bl_set_delay);
	if (bl_dev) {
		if (cabc_disable == CABC_WORKING) {
			cabc_para.cur_bl = bl_dev->props.brightness *
				(bl->max_level - bl->min_level) / 255;

			bl->cabc_en = true;
			bl->cabc_level = cabc_para.bl_fix *
					cabc_para.cur_bl / 1020;
			bl->cabc_refer_level = cabc_para.cur_bl;
			sprd_cabc_backlight_update(bl_dev);
		} else
			backlight_update_status(bl_dev);
	}

	cabc_bl_set = false;
}

static void dpu_wb_trigger(struct dpu_context *ctx,
								u8 count, bool debug)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 vcnt;

	int mode_width  = reg->blend_size & 0xFFFF;
	int mode_height = reg->blend_size >> 16;

	wb_layer.dst_w = mode_width;
	wb_layer.dst_h = mode_height;
	wb_layer.xfbc = wb_xfbc_en;
	wb_layer.pitch[0] = ALIGN(mode_width, 16) * 4;
	wb_layer.header_size_r = XFBC8888_HEADER_SIZE(mode_width,
					mode_height) / 128;

	reg->wb_pitch = ALIGN((mode_width), 16);

	wb_layer.xfbc = wb_xfbc_en;

	if (wb_xfbc_en && !debug)
		reg->wb_cfg = (wb_layer.header_size_r << 16) | BIT(0);
	else
		reg->wb_cfg = 0;

	reg->wb_base_addr = ctx->wb_addr_p;

	vcnt = (reg->dpu_sts[0] & 0x1FFF);
	/*
	 * Due to AISC design problem, after the wb enable, Dpu
	 * update register operation must be connected immediately.
	 * There can be no vsync interrupts between them.
	 */
	if (vcnt * 100 / mode_height < 70) {
		if (debug)
			/* writeback debug trigger */
			reg->wb_ctrl = BIT(1);
		else
			reg->wb_ctrl |= BIT(0);

		/* update trigger */
		reg->dpu_ctrl |= BIT(2);

		dpu_wait_update_done(ctx);

		pr_debug("write back trigger\n");
	} else
		vsync_count = 0;
}

static void dpu_wb_flip(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	dpu_clean_all(ctx);
	dpu_layer(ctx, &wb_layer);

	reg->dpu_ctrl |= BIT(2);
	dpu_wait_update_done(ctx);

	pr_debug("write back flip\n");
}

static void dpu_wb_work_func(struct work_struct *data)
{
	struct dpu_context *ctx =
		container_of(data, struct dpu_context, wb_work);

	down(&ctx->refresh_lock);

	if (!ctx->is_inited) {
		up(&ctx->refresh_lock);
		pr_err("dpu is not initialized\n");
		return;
	}

	if (ctx->disable_flip) {
		up(&ctx->refresh_lock);
		pr_warn("dpu flip is disabled\n");
		return;
	}

	if (wb_en && (vsync_count > max_vsync_count))
		dpu_wb_trigger(ctx, 1, false);
	else if (!wb_en)
		dpu_wb_flip(ctx);

	up(&ctx->refresh_lock);
}

static int dpu_wb_buf_alloc(struct sprd_dpu *dpu, size_t size,
			 u32 *paddr)
{
	struct device_node *node;
	u64 size64;
	struct resource r;

	node = of_parse_phandle(dpu->dev.of_node,
					"sprd,wb-memory", 0);
	if (!node) {
		pr_err("no sprd,wb-memory specified\n");
		return -EINVAL;
	}

	if (of_address_to_resource(node, 0, &r)) {
		pr_err("invalid wb reserved memory node!\n");
		return -EINVAL;
	}

	*paddr = r.start;
	size64 = resource_size(&r);

	if (size64 < size) {
		pr_err("unable to obtain enough wb memory\n");
		return -ENOMEM;
	}

	return 0;
}

static int dpu_write_back_config(struct dpu_context *ctx)
{
	int ret;
	static int need_config = 1;
	size_t wb_buf_size;
	struct sprd_dpu *dpu =
		(struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);

	if (!need_config) {
		pr_debug("write back has configed\n");
		return 0;
	}

	wb_buf_size = XFBC8888_BUFFER_SIZE(dpu->mode->hdisplay,
						dpu->mode->vdisplay);
	pr_info("use wb_reserved memory for writeback, size:0x%zx\n", wb_buf_size);
	ret = dpu_wb_buf_alloc(dpu, wb_buf_size, &ctx->wb_addr_p);
	if (ret) {
		max_vsync_count = 0;
		return -1;
	}

	wb_layer.index = 7;
	wb_layer.planes = 1;
	wb_layer.alpha = 0xff;
	wb_layer.format = DRM_FORMAT_ABGR8888;
	wb_layer.addr[0] = ctx->wb_addr_p;

	max_vsync_count = 4;
	need_config = 0;

	INIT_WORK(&ctx->wb_work, dpu_wb_work_func);

	return 0;
}

static void dpu_dvfs_task_func(unsigned long data)
{
	struct dpu_context *ctx = (struct dpu_context *)data;
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct sprd_dpu_layer layer, layers[8];
	int i, j, max_x, max_y, min_x, min_y;
	int layer_en, max, maxs[8], count = 0;
	u32 dvfs_freq;

	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		return;
	}

	/*
	 * Count the current total number of active layers
	 * and the corresponding pos_x, pos_y, size_x and size_y.
	 */
	for (i = 0; i < ARRAY_SIZE(reg->layers); i++) {
		layer_en = reg->layers[i].ctrl & BIT(0);
		if (layer_en) {
			layers[count].dst_x = reg->layers[i].pos & 0xffff;
			layers[count].dst_y = reg->layers[i].pos >> 16;
			layers[count].dst_w = reg->layers[i].size & 0xffff;
			layers[count].dst_h = reg->layers[i].size >> 16;
			count++;
		}
	}

	/*
	 * Calculate the number of overlaps between each
	 * layer with other layers, not include itself.
	 */
	for (i = 0; i < count; i++) {
		layer.dst_x = layers[i].dst_x;
		layer.dst_y = layers[i].dst_y;
		layer.dst_w = layers[i].dst_w;
		layer.dst_h = layers[i].dst_h;
		maxs[i] = 1;

		for (j = 0; j < count; j++) {
			if (layer.dst_x + layer.dst_w > layers[j].dst_x &&
				layers[j].dst_x + layers[j].dst_w > layer.dst_x &&
				layer.dst_y + layer.dst_h > layers[j].dst_y &&
				layers[j].dst_y + layers[j].dst_h > layer.dst_y &&
				i != j) {
				max_x = max(layers[i].dst_x, layers[j].dst_x);
				max_y = max(layers[i].dst_y, layers[j].dst_y);
				min_x = min(layers[i].dst_x + layers[i].dst_w,
					layers[j].dst_x + layers[j].dst_w);
				min_y = min(layers[i].dst_y + layers[i].dst_h,
					layers[j].dst_y + layers[j].dst_h);

				layer.dst_x = max_x;
				layer.dst_y = max_y;
				layer.dst_w = min_x - max_x;
				layer.dst_h = min_y - max_y;

				maxs[i]++;
			}
		}
	}

	/* take the maximum number of overlaps */
	max = maxs[0];
	for (i = 1; i < count; i++) {
		if (maxs[i] > max)
			max = maxs[i];
	}

	/*
	 * Determine which frequency to use based on the
	 * maximum number of overlaps.
	 * Every IP here may be different, so need to modify it
	 * according to the actual dpu core clock.
	 */
	if (max <= 3)
		dvfs_freq = 307200000;
	else
		dvfs_freq = 384000000;

	dpu_dvfs_notifier_call_chain(&dvfs_freq);
}

static void dpu_dvfs_task_init(struct dpu_context *ctx)
{
	static int need_config = 1;

	if (!need_config)
		return;

	need_config = 0;
	tasklet_init(&ctx->dvfs_task, dpu_dvfs_task_func,
			(unsigned long)ctx);
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
	init_cabc(ctx->vm.vactive, ctx->vm.hactive);

	reg->dpu_cfg0 = 0;
	reg->dpu_cfg1 = (qos_cfg.awqos_high << 12) |
		(qos_cfg.awqos_low << 8) |
		(qos_cfg.arqos_high << 4) |
		(qos_cfg.arqos_low) | BIT(18) | BIT(22);
	reg->dpu_cfg2 = 0x14002;

	if (ctx->is_stopped)
		dpu_clean_all(ctx);

	reg->dpu_int_clr = 0xffff;

	dpu_enhance_reload(ctx);

	dpu_write_back_config(ctx);

	if (corner_radius)
		dpu_corner_init(ctx);

	dpu_dvfs_task_init(ctx);
	INIT_WORK(&ctx->cabc_work, dpu_cabc_work_func);
	INIT_WORK(&ctx->cabc_bl_update, dpu_cabc_bl_update_func);

	frame_no = 0;

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
	DPU_LAYER_FORMAT_XFBC_YUV420,
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

static u32 dpu_img_ctrl(u32 format, u32 blending, u32 compression, u32 y2r_coef,
		u32 rotation)
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
		reg_val |= BIT(12);
	case DRM_FORMAT_RGB565:
		if (compression)
			/* XFBC-RGB565 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_RGB565 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_RGB565 << 4);
		break;
	case DRM_FORMAT_NV12:
		if (compression)
			/*2-Lane: Yuv420 */
			reg_val |= DPU_LAYER_FORMAT_XFBC_YUV420 << 4;
		else
			reg_val |= DPU_LAYER_FORMAT_YUV420_2PLANE << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_NV21:
		if (compression)
			/*2-Lane: Yuv420 */
			reg_val |= DPU_LAYER_FORMAT_XFBC_YUV420 << 4;
		else
			reg_val |= DPU_LAYER_FORMAT_YUV420_2PLANE << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	case DRM_FORMAT_NV16:
		/*2-Lane: Yuv422 */
		reg_val |= DPU_LAYER_FORMAT_YUV422_2PLANE << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	case DRM_FORMAT_NV61:
		/*2-Lane: Yuv422 */
		reg_val |= DPU_LAYER_FORMAT_YUV422_2PLANE << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_YUV420:
		reg_val |= DPU_LAYER_FORMAT_YUV420_3PLANE << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
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
		/* blending mode select - normal mode */
		reg_val &= (~BIT(16));
		break;
	case DRM_MODE_BLEND_PREMULTI:
		/* alpha mode select - combo alpha */
		reg_val |= BIT(3);
		/* blending mode select - pre-mult mode */
		reg_val |= BIT(16);
		break;
	default:
		/* alpha mode select - layer alpha */
		reg_val |= BIT(2);
		break;
	}

	reg_val |= y2r_coef << 28;
	rotation = to_dpu_rotation(rotation);
	reg_val |= (rotation & 0x7) << 20;

	return reg_val;
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
	u32 size, offset, wd;
	int i;

	layer = &reg->layers[hwlayer->index];
	offset = (hwlayer->dst_x & 0xffff) | ((hwlayer->dst_y) << 16);

	if (hwlayer->pallete_en) {
		size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);
		layer->pos = offset;
		layer->size = size;
		layer->alpha = hwlayer->alpha;
		layer->pallete = hwlayer->pallete_color;

		/* pallete layer enable */
		layer->ctrl = 0x2005;

		pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d, pallete:%d\n",
			hwlayer->dst_x, hwlayer->dst_y,
			hwlayer->dst_w, hwlayer->dst_h, layer->pallete);
		return;
	}

	if (hwlayer->src_w && hwlayer->src_h)
		size = (hwlayer->src_w & 0xffff) | ((hwlayer->src_h) << 16);
	else
		size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);

	for (i = 0; i < hwlayer->planes; i++) {
		if (hwlayer->addr[i] % 16)
			pr_err("layer addr[%d] is not 16 bytes align, it's 0x%08x\n",
				i, hwlayer->addr[i]);
		layer->addr[i] = hwlayer->addr[i];
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

	layer->ctrl = dpu_img_ctrl(hwlayer->format, hwlayer->blending,
		hwlayer->xfbc, hwlayer->y2r_coef, hwlayer->rotation);

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

	vsync_count = 0;
	if (max_vsync_count > 0 && count > 1)
		wb_en = true;

	/*
	 * Make sure the dpu is in stop status. DPU_R4P0 has no shadow
	 * registers in EDPI mode. So the config registers can only be
	 * updated in the rising edge of DPU_RUN bit.
	 */
	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);
	if (cabc_disable != CABC_DISABLED)
		dpu_cabc_trigger(ctx);

	/* reset the bgcolor to black */
	reg->bg_color = 0;

	/* disable all the layers */
	dpu_clean_all(ctx);

	/* to check if dpu need scaling the frame for SR */
	dpu_scaling(ctx, layers, count);

	/* start configure dpu layers */
	for (i = 0; i < count; i++)
		dpu_layer(ctx, &layers[i]);

	/* update trigger and wait */
	if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		if (!ctx->is_stopped) {
			reg->dpu_ctrl |= BIT(2);
			dpu_wait_update_done(ctx);
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
			   DISPC_INT_FBC_HDR_ERR_MASK;
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

static void dpu_dpi_init(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 int_mask = 0;

	if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		/* use dpi as interface */
		reg->dpu_cfg0 &= ~BIT(0);

		/* disable Halt function for SPRD DSI */
		reg->dpi_ctrl &= ~BIT(16);

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

	reg->dpu_int_en = int_mask;
}

static void enable_vsync(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_int_en |= DISPC_INT_DPI_VSYNC_MASK;
}

static void disable_vsync(struct dpu_context *ctx)
{
	//struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	//reg->dpu_int_en &= ~DISPC_INT_DPI_VSYNC_MASK;
}

static void dpu_enhance_backup(u32 id, void *param)
{
	u32 *p;

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p = param;
		enhance_en |= *p;
		pr_info("enhance module enable backup: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_DISABLE:
		p = param;
		if (*p & BIT(1)) {
			if ((enhance_en & BIT(0)) && sr_epf_ready) {
				*p &= ~BIT(1);
				pr_info("enhance backup epf shouldn't be disabled\n");
			}
		}
		enhance_en &= ~(*p);
		pr_info("enhance module disable backup: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_SCL:
		memcpy(&scale_copy, param, sizeof(scale_copy));
		if (!(enhance_en & BIT(4)))
			enhance_en |= BIT(1);
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
	case ENHANCE_CFG_ID_LTM:
		enhance_en |= BIT(6);
		pr_info("enhance ltm backup\n");
	case ENHANCE_CFG_ID_SLP:
		memcpy(&slp_copy, param, sizeof(slp_copy));
		if (!cabc_disable) {
			slp_copy.cabc_startv = 0;
			slp_copy.cabc_endv = 255;
		}
		enhance_en |= BIT(4);
		pr_info("enhance slp backup\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		memcpy(&gamma_copy, param, sizeof(gamma_copy));
		enhance_en |= BIT(5) | BIT(10);
		pr_info("enhance gamma backup\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		memcpy(&epf_copy, param, sizeof(epf_copy));
		enhance_en |= BIT(1);
		pr_info("enhance epf backup\n");
		break;
	case ENHANCE_CFG_ID_LUT3D:
		memcpy(&lut3d_copy, param, sizeof(lut3d_copy));
		enhance_en |= BIT(9);
		pr_info("enhance lut3d backup\n");
		break;
	case ENHANCE_CFG_ID_SR_EPF:
		memcpy(&sr_epf, param, sizeof(sr_epf));
		/* valid range of gain3 is [128,255]; */
		if (sr_epf.gain3 == 0) {
			/* eye comfort and super resolution are enabled*/
			if (!(enhance_en & BIT(2)) && (enhance_en & BIT(0))) {
				enhance_en &= ~BIT(1);
				pr_info("enhance[ID_SR_EPF] backup disable epf\n");
			}
			sr_epf_ready = 0;
		} else {
			sr_epf_ready = 1;
			pr_info("enhance[ID_SR_EPF] epf backup\n");
		}
		break;
	default:
		break;
	}
}

static void dpu_enhance_set(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct scale_cfg *scale;
	struct cm_cfg cm;
	struct slp_cfg *slp;
	struct gamma_lut *gamma;
	struct threed_lut *lut3d;
	struct hsv_lut *hsv;
	struct epf_cfg *epf_slp;
	bool dpu_stopped;
	u32 *p;
	int i;

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
		if (*p & BIT(1)) {
			if ((enhance_en & BIT(0)) && sr_epf_ready) {
				*p &= ~BIT(1);
				dpu_epf_set(reg, &sr_epf);
				pr_info("enhance epf shouldn't be disabled\n");
			}
		}
		reg->dpu_enhance_cfg &= ~(*p);
		pr_info("enhance module disable: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_SCL:
		memcpy(&scale_copy, param, sizeof(scale_copy));
		scale = &scale_copy;
		reg->blend_size = (scale->in_h << 16) | scale->in_w;
		reg->epf_epsilon = (epf.epsilon1 << 16) | epf.epsilon0;
		reg->epf_gain0_3 = (epf.gain3 << 24) | (epf.gain2 << 16) |
				(epf.gain1 << 8) | epf.gain0;
		reg->epf_gain4_7 = (epf.gain7 << 24) | (epf.gain6 << 16) |
				(epf.gain5 << 8) | epf.gain4;
		reg->epf_diff = (epf.max_diff << 8) | epf.min_diff;
		reg->dpu_enhance_cfg |= (BIT(0) | BIT(1));
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
		memcpy(&cm, &cm_copy, sizeof(struct cm_cfg));
		if (cabc_para.gain) {
			cm.coef00 = (cm.coef00 * cabc_para.gain) / 0x400;
			cm.coef11 = (cm.coef11 * cabc_para.gain) / 0x400;
			cm.coef22 = (cm.coef22 * cabc_para.gain) / 0x400;
		}

		reg->cm_coef01_00 = (cm.coef01 << 16) | cm.coef00;
		reg->cm_coef03_02 = (cm.coef03 << 16) | cm.coef02;
		reg->cm_coef11_10 = (cm.coef11 << 16) | cm.coef10;
		reg->cm_coef13_12 = (cm.coef13 << 16) | cm.coef12;
		reg->cm_coef21_20 = (cm.coef21 << 16) | cm.coef20;
		reg->cm_coef23_22 = (cm.coef23 << 16) | cm.coef22;
		reg->dpu_enhance_cfg |= BIT(3);
		pr_info("enhance cm set\n");
		break;
	case ENHANCE_CFG_ID_LTM:
		enhance_en |= BIT(6);
		pr_info("enhance ltm set\n");
	case ENHANCE_CFG_ID_SLP:
		memcpy(&slp_copy, param, sizeof(slp_copy));
		if (!cabc_disable) {
			slp_copy.cabc_startv = 0;
			slp_copy.cabc_endv = 255;
		}
		slp = &slp_copy;
		reg->slp_cfg0 = (slp->brightness_step << 0)|
			((slp->brightness & 0x7f) << 16);
		reg->slp_cfg1 = ((slp->fst_max_bright_th & 0x7f) << 21) |
			((slp->fst_max_bright_th_step[0] & 0x7f) << 14) |
			((slp->fst_max_bright_th_step[1] & 0x7f) << 7) |
			((slp->fst_max_bright_th_step[2] & 0x7f) << 0);
		reg->slp_cfg2 = ((slp->fst_max_bright_th_step[3] & 0x7f) << 25) |
			((slp->fst_max_bright_th_step[4] & 0x7f) << 18) |
			((slp->hist_exb_no & 0x3) << 16) |
			((slp->hist_exb_percent & 0x7f) << 9);
		reg->slp_cfg3 = ((slp->mask_height & 0xfff) << 19) |
			((slp->fst_pth_index[0] & 0xf) << 15) |
			((slp->fst_pth_index[1] & 0xf) << 11) |
			((slp->fst_pth_index[2] & 0xf) << 7) |
			((slp->fst_pth_index[3] & 0xf) << 3);
		reg->slp_cfg4 = (slp->hist9_index[0] << 24) |
			(slp->hist9_index[1] << 16) | (slp->hist9_index[2] << 8) |
			(slp->hist9_index[3] << 0);
		reg->slp_cfg5 = (slp->hist9_index[4] << 24) |
			(slp->hist9_index[5] << 16) | (slp->hist9_index[6] << 8) |
			(slp->hist9_index[7] << 0);
		reg->slp_cfg6 = (slp->hist9_index[8] << 24) |
			(slp->glb_x[0] << 16) | (slp->glb_x[1] << 8) |
			(slp->glb_x[2] << 0);
		reg->slp_cfg7 = ((slp->glb_s[0] & 0x1ff) << 23) |
			((slp->glb_s[1] & 0x1ff) << 14) |
			((slp->glb_s[2] & 0x1ff) << 5);
		reg->slp_cfg8 = ((slp->limit_hclip & 0x1ff) << 23) |
			((slp->limit_lclip & 0x1ff) << 14) |
			((slp->limit_clip_step & 0x1fff) << 0);
		reg->slp_cfg9 = ((slp->fast_ambient_th & 0x7f) << 25) |
			(slp->scene_change_percent_th << 17) |
			((slp->local_weight & 0xf) << 13) |
			((slp->fst_pth & 0x7f) << 6);
		reg->slp_cfg10 = (slp->cabc_endv << 8) | (slp->cabc_startv << 0);
		enhance_en |= BIT(4);
		reg->dpu_enhance_cfg = enhance_en;
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
		reg->dpu_enhance_cfg |= BIT(5) | BIT(10);
		pr_info("enhance gamma set\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		memcpy(&epf_copy, param, sizeof(epf_copy));
		if (((enhance_en & BIT(4)) &&
			(slp_copy.brightness > SLP_BRIGHTNESS_THRESHOLD)) ||
			!(enhance_en & BIT(0)) || !sr_epf_ready) {
			epf_slp = &epf_copy;
			pr_info("enhance epf set\n");
		} else {
			epf_slp = &sr_epf;
			pr_info("enhance epf(sr) set\n");
		}

		dpu_epf_set(reg, epf_slp);
		reg->dpu_enhance_cfg |= BIT(1);
		break;
	case ENHANCE_CFG_ID_SR_EPF:
		memcpy(&sr_epf, param, sizeof(sr_epf));
		/* valid range of gain3 is [128,255]; */
		if (sr_epf.gain3 == 0) {
			sr_epf_ready = 0;

			if ((enhance_en & BIT(2)) && (enhance_en & BIT(0))) {
				epf_slp = &epf_copy;
				dpu_epf_set(reg, epf_slp);
				reg->dpu_enhance_cfg |= BIT(1);
				pr_info("enhance[ID_SR_EPF] epf set\n");
				break;
			} else if (enhance_en & BIT(0)) {
				reg->dpu_enhance_cfg &= ~BIT(1);
				pr_info("enhance[ID_SR_EPF] disable epf\n");
				break;
			}
			return;
		}

		sr_epf_ready = 1;
		if ((enhance_en & BIT(0)) && (!(enhance_en & BIT(4)) ||
			(slp_copy.brightness <= SLP_BRIGHTNESS_THRESHOLD))) {
			epf_slp = &sr_epf;
			dpu_epf_set(reg, epf_slp);
			reg->dpu_enhance_cfg |= BIT(1);
			pr_info("enhance[ID_SR_EPF] epf(sr) set\n");
			break;
		}

		pr_info("enhance[ID_SR_EPF] epf(sr) set delay\n");
		return;
	case ENHANCE_CFG_ID_LUT3D:
		memcpy(&lut3d_copy, param, sizeof(lut3d_copy));
		lut3d = &lut3d_copy;
		dpu_stopped = ctx->is_stopped;

		dpu_stop(ctx);
		for (i = 0; i < 729; i++) {
			reg->threed_lut_addr = i;
			ndelay(1);
			reg->threed_lut_wdata = lut3d->value[i];
			pr_debug("0x%x:0x%x\n", i, lut3d->value[i]);
		}
		reg->dpu_enhance_cfg |= BIT(9);
		if ((ctx->if_type == SPRD_DISPC_IF_DPI) && !dpu_stopped)
			dpu_run(ctx);
		pr_info("enhance lut3d set\n");
		enhance_en = reg->dpu_enhance_cfg;
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
	struct threed_lut *lut3d;
	u32 *p32;
	int i, val;

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
	case ENHANCE_CFG_ID_LTM:
	case ENHANCE_CFG_ID_SLP:
		slp = param;
		val = reg->slp_cfg0;
		slp->brightness = (val >> 16) & 0x7f;
		slp->brightness_step = (val >> 0) & 0xffff;

		val = reg->slp_cfg1;
		slp->fst_max_bright_th = (val >> 21) & 0x7f;
		slp->fst_max_bright_th_step[0] = (val >> 14) & 0x7f;
		slp->fst_max_bright_th_step[1] = (val >> 7) & 0x7f;
		slp->fst_max_bright_th_step[2] = (val >> 0) & 0x7f;

		val = reg->slp_cfg2;
		slp->fst_max_bright_th_step[3] = (val >> 25) & 0x7f;
		slp->fst_max_bright_th_step[4] = (val >> 18) & 0x7f;
		slp->hist_exb_no = (val >> 16) & 0x3;
		slp->hist_exb_percent = (val >> 9) & 0x7f;

		val = reg->slp_cfg3;
		slp->mask_height = (val >> 19) & 0xfff;
		slp->fst_pth_index[0] = (val >> 15) & 0xf;
		slp->fst_pth_index[1] = (val >> 11) & 0xf;
		slp->fst_pth_index[2] = (val >> 7) & 0xf;
		slp->fst_pth_index[3] = (val >> 3) & 0xf;

		val = reg->slp_cfg4;
		slp->hist9_index[0] = (val >> 24) & 0xff;
		slp->hist9_index[1] = (val >> 16) & 0xff;
		slp->hist9_index[2] = (val >> 8) & 0xff;
		slp->hist9_index[3] = (val >> 0) & 0xff;

		val = reg->slp_cfg5;
		slp->hist9_index[4] = (val >> 24) & 0xff;
		slp->hist9_index[5] = (val >> 16) & 0xff;
		slp->hist9_index[6] = (val >> 8) & 0xff;
		slp->hist9_index[7] = (val >> 0) & 0xff;

		val = reg->slp_cfg6;
		slp->hist9_index[8] = (val >> 24) & 0xff;
		slp->glb_x[0] = (val >> 16) & 0xff;
		slp->glb_x[1] = (val >> 8) & 0xff;
		slp->glb_x[2] = (val >> 0) & 0xff;

		val = reg->slp_cfg7;
		slp->glb_s[0] = (val >> 23) & 0x1ff;
		slp->glb_s[1] = (val >> 14) & 0x1ff;
		slp->glb_s[2] = (val >> 5) & 0x1ff;

		val = reg->slp_cfg8;
		slp->limit_hclip = (val >> 23) & 0x1ff;
		slp->limit_lclip = (val >> 14) & 0x1ff;
		slp->limit_clip_step = (val >> 0) & 0x1fff;

		val = reg->slp_cfg9;
		slp->fast_ambient_th = (val >> 25) & 0x7f;
		slp->scene_change_percent_th = (val >> 17) & 0xff;
		slp->local_weight = (val >> 13) & 0xf;
		slp->fst_pth = (val >> 6) & 0x7f;

		val = reg->slp_cfg10;
		slp->cabc_endv = (val >> 8) & 0xff;
		slp->cabc_startv = (val >> 0) & 0xff;
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
	case ENHANCE_CFG_ID_SLP_LUT:
		dpu_stop(ctx);
		p32 = param;
		for (i = 0; i < 256; i++) {
			reg->slp_lut_addr = i;
			udelay(1);
			*p32++ = reg->slp_lut_rdata;
		}
		dpu_run(ctx);
		pr_info("enhance slp lut get\n");
		break;
	case ENHANCE_CFG_ID_LUT3D:
		lut3d = param;
		dpu_stop(ctx);
		for (i = 0; i < 729; i++) {
			reg->threed_lut_addr = i;
			udelay(1);
			lut3d->value[i] = reg->threed_lut_rdata;
			pr_debug("0x%02x: 0x%x\n", i, lut3d->value[i]);
		}
		dpu_run(ctx);
		pr_info("enhance lut3d get\n");
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
	struct threed_lut *lut3d;
	int i;

	for (i = 0; i < 256; i++) {
		reg->slp_lut_addr = i;
		udelay(1);
		reg->slp_lut_wdata = slp_lut[i];
	}
	pr_info("enhance slp lut reload\n");

	if (enhance_en & BIT(0)) {
		scale = &scale_copy;
		reg->blend_size = (scale->in_h << 16) | scale->in_w;
		pr_info("enhance scaling from %ux%u to %ux%u\n", scale->in_w,
			scale->in_h, ctx->vm.hactive, ctx->vm.vactive);
	}

	if (enhance_en & BIT(1)) {
		if (((enhance_en & BIT(4)) &&
			(slp_copy.brightness > SLP_BRIGHTNESS_THRESHOLD)) ||
			!(enhance_en & BIT(0)) || !sr_epf_ready) {
			epf = &epf_copy;
			pr_info("enhance epf reload\n");
		} else {
			epf = &sr_epf;
			pr_info("enhance epf(sr) reload\n");
		}

		dpu_epf_set(reg, epf);
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
		reg->slp_cfg0 = (slp->brightness_step << 0) |
			((slp->brightness & 0x7f) << 16);
		reg->slp_cfg1 = ((slp->fst_max_bright_th & 0x7f) << 21) |
			((slp->fst_max_bright_th_step[0] & 0x7f) << 14) |
			((slp->fst_max_bright_th_step[1] & 0x7f) << 7) |
			((slp->fst_max_bright_th_step[2] & 0x7f) << 0);
		reg->slp_cfg2 = \
			((slp->fst_max_bright_th_step[3] & 0x7f) << 25) |
			((slp->fst_max_bright_th_step[4] & 0x7f) << 18) |
			((slp->hist_exb_no & 0x3) << 16) |
			((slp->hist_exb_percent & 0x7f) << 9);
		reg->slp_cfg3 = ((slp->mask_height & 0xfff) << 19) |
			((slp->fst_pth_index[0] & 0xf) << 15) |
			((slp->fst_pth_index[1] & 0xf) << 11) |
			((slp->fst_pth_index[2] & 0xf) << 7) |
			((slp->fst_pth_index[3] & 0xf) << 3);
		reg->slp_cfg4 = (slp->hist9_index[0] << 24) |
			(slp->hist9_index[1] << 16) | (slp->hist9_index[2] << 8) |
			(slp->hist9_index[3] << 0);
		reg->slp_cfg5 = (slp->hist9_index[4] << 24) |
			(slp->hist9_index[5] << 16) |
			(slp->hist9_index[6] << 8) |
			(slp->hist9_index[7] << 0);
		reg->slp_cfg6 = (slp->hist9_index[8] << 24) |
			(slp->glb_x[0] << 16) | (slp->glb_x[1] << 8) |
			(slp->glb_x[2] << 0);
		reg->slp_cfg7 = ((slp->glb_s[0] & 0x1ff) << 23) |
			((slp->glb_s[1] & 0x1ff) << 14) |
			((slp->glb_s[2] & 0x1ff) << 5);
		reg->slp_cfg9 = ((slp->fast_ambient_th & 0x7f) << 25) |
			(slp->scene_change_percent_th << 17) |
			((slp->local_weight & 0xf) << 13) |
			((slp->fst_pth & 0x7f) << 6);
		reg->slp_cfg10 = (slp->cabc_endv << 8) |
			(slp->cabc_startv << 0);
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

	if (enhance_en & BIT(6)) {
		slp = &slp_copy;
		reg->slp_cfg8 = ((slp->limit_hclip & 0x1ff) << 23) |
			((slp->limit_lclip & 0x1ff) << 14) |
			((slp->limit_clip_step & 0x1fff) << 0);
		pr_info("enhance ltm reload\n");
	}

	if (enhance_en & BIT(9)) {
		lut3d = &lut3d_copy;
		for (i = 0; i < 729; i++) {
			reg->threed_lut_addr = i;
			udelay(1);
			reg->threed_lut_wdata = lut3d->value[i];
			pr_debug("0x%02x:0x%x\n", i, lut3d->value[i]);
		}
		pr_info("enhance lut3d reload\n");
	}

	reg->dpu_enhance_cfg = enhance_en;
}

static void dpu_sr_config(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->blend_size = (scale_copy.in_h << 16) | scale_copy.in_w;
	if (need_scale) {
		/* SLP is disabled mode or bypass mode */
		if ((slp_copy.brightness <= SLP_BRIGHTNESS_THRESHOLD) ||
			!(enhance_en & BIT(4))) {

		/*
		 * valid range of gain3 is [128,255];dpu_scaling maybe
		 * called before epf_copy is assinged a value
		 */
			if (sr_epf.gain3 > 0) {
				dpu_epf_set(reg, &sr_epf);
				enhance_en |= BIT(1);
			}
		}
		enhance_en |= BIT(0);
		reg->dpu_enhance_cfg = enhance_en;
	} else {
		if (enhance_en & BIT(6))
			dpu_epf_set(reg, &epf_copy);
		else
			enhance_en &= ~(BIT(1));

		enhance_en &= ~(BIT(0));
		reg->dpu_enhance_cfg = enhance_en;
	}
}

static int dpu_cabc_trigger(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct cm_cfg cm;
	int i;
	struct device_node *backlight_node;
	static unsigned long vsp_pmu_addr;

	if (cabc_disable) {
		if (cabc_disable == CABC_STOPPING) {
			memset(&cabc_para, 0, sizeof(cabc_para));
			memcpy(&cm, &cm_copy, sizeof(struct cm_cfg));
			reg->cm_coef01_00 = (cm.coef01 << 16) | cm.coef00;
			reg->cm_coef03_02 = (cm.coef03 << 16) | cm.coef02;
			reg->cm_coef11_10 = (cm.coef11 << 16) | cm.coef10;
			reg->cm_coef13_12 = (cm.coef13 << 16) | cm.coef12;
			reg->cm_coef21_20 = (cm.coef21 << 16) | cm.coef20;
			reg->cm_coef23_22 = (cm.coef23 << 16) | cm.coef22;

			cabc_bl_set = true;

			cabc_disable = CABC_DISABLED;
		}
		return 0;
	}

	if (frame_no == 0) {
		if (!bl_dev) {
			backlight_node = of_parse_phandle(g_np,
						 "sprd,backlight", 0);
			if (backlight_node) {
				bl_dev =
				of_find_backlight_by_node(backlight_node);
				of_node_put(backlight_node);
			} else {
				pr_warn("dpu backlight node not found\n");
			}
		}

		vsp_pmu_addr = (unsigned long)ioremap_nocache(0x327E00BC, 4);

		reg->dpu_enhance_cfg |= BIT(3);
		reg->dpu_enhance_cfg |= BIT(8);
		enhance_en |= BIT(3);
		enhance_en |= BIT(8);

		slp_copy.cabc_startv = 0;
		slp_copy.cabc_endv = 255;
		reg->slp_cfg10 = (slp_copy.cabc_endv << 8) |
			(slp_copy.cabc_startv << 0);

		frame_no++;
	} else {
		for (i = 0; i < 32; i++) {
			cabc_para.cabc_hist[i] = reg->cabc_hist[i];
			udelay(1);
		}

		if ((*(unsigned int *)vsp_pmu_addr) & 0x0000ff00)
			cabc_para.is_VSP_working = false;
		else
			cabc_para.is_VSP_working = true;

		step_set(cabc_step0, cabc_step1, cabc_step2,
			cabc_scene_change_thr, cabc_min_backlight);
		cabc_trigger(&cabc_para, frame_no);

		memcpy(&cm, &cm_copy, sizeof(struct cm_cfg));
		if (cm.coef00 == 0 && cm.coef11 == 0 &&
			cm.coef22 == 0) {
			cm.coef00 = cm.coef11 = cm.coef22 = cabc_para.gain;
		} else {
			cm.coef00 = (cm.coef00 * cabc_para.gain) / 0x400;
			cm.coef11 = (cm.coef11 * cabc_para.gain) / 0x400;
			cm.coef22 = (cm.coef22 * cabc_para.gain) / 0x400;
		}
		reg->cm_coef01_00 = (cm.coef01 << 16) | cm.coef00;
		reg->cm_coef03_02 = (cm.coef03 << 16) | cm.coef02;
		reg->cm_coef11_10 = (cm.coef11 << 16) | cm.coef10;
		reg->cm_coef13_12 = (cm.coef13 << 16) | cm.coef12;
		reg->cm_coef21_20 = (cm.coef21 << 16) | cm.coef20;
		reg->cm_coef23_22 = (cm.coef23 << 16) | cm.coef22;

		cabc_bl_set = true;

		if (frame_no == 1)
			frame_no++;
	}
	return 0;
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
	DRM_FORMAT_YUV420,
};

static int dpu_capability(struct dpu_context *ctx,
			struct dpu_capability *cap)
{
	if (!cap)
		return -EINVAL;

	cap->max_layers = 4;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);

	return 0;
}

static struct dpu_core_ops dpu_r4p0_ops = {
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
	.write_back = dpu_wb_trigger,
};

static struct ops_entry entry = {
	.ver = "dpu-r4p0",
	.ops = &dpu_r4p0_ops,
};

static int __init dpu_core_register(void)
{
	return dpu_core_ops_register(&entry);
}

subsys_initcall(dpu_core_register);
