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

#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include "sprd_dpu.h"
#include "sprd_corner.h"

#define DISPC_BRIGHTNESS           (0x00 << 16)
#define DISPC_CONTRAST             (0x100 << 0)
#define DISPC_OFFSET_U             (0x80 << 16)
#define DISPC_SATURATION_U         (0x100 << 0)
#define DISPC_OFFSET_V             (0x80 << 16)
#define DISPC_SATURATION_V         (0x100 << 0)

#define DISPC_INT_MMU_INV_WR_MASK	BIT(19)
#define DISPC_INT_MMU_INV_RD_MASK	BIT(18)
#define DISPC_INT_MMU_VAOR_WR_MASK	BIT(17)
#define DISPC_INT_MMU_VAOR_RD_MASK	BIT(16)

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

struct dpu_reg {
	u32 dpu_version;
	u32 dpu_ctrl;
	u32 dpu_size;
	u32 dpu_rstn;
	u32 dpu_secure;
	u32 dpu_qos;
	u32 reserved_0x0018;
	u32 bg_color;
	struct layer_reg layers[6];
	u32 wb_base_addr;
	u32 wb_ctrl;
	u32 wb_pitch;
	u32 reserved_0x014c;
	u32 y2r_ctrl;
	u32 y2r_y_param;
	u32 y2r_u_param;
	u32 y2r_v_param;
	u32 dpu_int_en;
	u32 dpu_int_clr;
	u32 dpu_int_sts;
	u32 dpu_int_raw;
	u32 dpi_ctrl;
	u32 dpi_h_timing;
	u32 dpi_v_timing;
	u32 dpi_sts0;
	u32 dpi_sts1;
	u32 dpu_sts0;
	u32 dpu_sts1;
	u32 dpu_sts2;
	u32 dpu_sts3;
	u32 dpu_sts4;
	u32 dpu_sts5;
	u32 dpu_checksum_en;
	u32 dpu_checksum0_start_pos;
	u32 dpu_checksum0_end_pos;
	u32 dpu_checksum1_start_pos;
	u32 dpu_checksum1_end_pos;
	u32 dpu_checksum0_result;
	u32 dpu_checksum1_result;
};

struct mmu_reg {
	uint32_t mmu_en;
	uint32_t mmu_update;
	uint32_t mmu_min_vpn;
	uint32_t mmu_vpn_range;
	uint32_t mmu_pt_addr;
	uint32_t mmu_default_page;

	uint32_t mmu_vaor_addr_rd;
	uint32_t mmu_vaor_addr_wr;
	uint32_t mmu_inv_addr_rd;
	uint32_t mmu_inv_addr_wr;
};

static DECLARE_WAIT_QUEUE_HEAD(wait_queue);
static bool panel_ready = true;
static bool evt_update;
static bool evt_stop;
static int wb_en;
static int max_vsync_count;
static int vsync_count;
static bool sprd_corner_support;
static int sprd_corner_radius;
static int flip_cnt;
static bool wb_config;
static int wb_disable;
static struct sprd_dpu_layer wb_layer;

static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_clean_lite(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_dpu_layer *hwlayer);
static void dpu_write_back(struct dpu_context *ctx, int enable);
static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_dpu_layer *hwlayer);

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

	ret = of_property_read_u32(np, "sprd,wb-disable", &wb_disable);
	if (wb_disable)
		pr_info("dpu_lite_r1p0 wb disabled\n");

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
		corner_layer_top.index = 4;
		corner_layer_bottom.index = 5;
		corner_is_inited = 1;
	}
}

static void dpu_dump(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	pr_info("layer0 addr0 = 0x%08x, addr1 = 0x%08x",
		reg->layers[0].addr[0], reg->layers[0].addr[1]);
	pr_info("layer1 addr0 = 0x%08x, addr1 = 0x%08x",
		reg->layers[1].addr[0], reg->layers[1].addr[1]);
	pr_info("layer2 addr0 = 0x%08x, addr1 = 0x%08x",
		reg->layers[2].addr[0], reg->layers[2].addr[1]);
	pr_info("layer3 addr0 = 0x%08x, addr1 = 0x%08x",
		reg->layers[3].addr[0], reg->layers[3].addr[1]);
}

static u32 check_mmu_isr(struct dpu_context *ctx, u32 reg_val)
{
	struct mmu_reg *reg = (struct mmu_reg *)(ctx->base + 0x800);
	u32 mmu_mask = DISPC_INT_MMU_VAOR_RD_MASK |
			DISPC_INT_MMU_VAOR_WR_MASK |
			DISPC_INT_MMU_INV_RD_MASK |
			DISPC_INT_MMU_INV_WR_MASK;
	u32 val = reg_val & mmu_mask;

	if (val) {
		pr_err("--- iommu interrupt err: 0x%04x ---\n", val);

		pr_err("iommu invalid read error, addr: 0x%08x\n",
			reg->mmu_inv_addr_rd);
		pr_err("iommu invalid write error, addr: 0x%08x\n",
			reg->mmu_inv_addr_wr);
		pr_err("iommu va out of range read error, addr: 0x%08x\n",
			reg->mmu_vaor_addr_rd);
		pr_err("iommu va out of range write error, addr: 0x%08x\n",
			reg->mmu_vaor_addr_wr);
		pr_err("BUG: iommu failure at %s:%d/%s()!\n",
			__FILE__, __LINE__, __func__);

		dpu_dump(ctx);

		/* panic("iommu panic\n"); */
	}

	return val;
}

static u32 dpu_isr(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 reg_val, int_mask = 0;

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
		if ((vsync_count == max_vsync_count) && wb_en)
			dpu_write_back(ctx, true);
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
		wb_en = false;
		if (vsync_count > max_vsync_count) {
			dpu_layer(ctx, &wb_layer);
			dpu_clean_lite(ctx);
		}
		dpu_write_back(ctx, false);
	}

	/* dpu write back error isr */
	if (reg_val & DISPC_INT_WB_FAIL_MASK) {
		pr_err("dpu write back fail\n");
		/* give a new chance for write back */
		if (max_vsync_count > 0) {
			wb_en = true;
			vsync_count = 0;
		}
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

static void dpu_write_back(struct dpu_context *ctx, int enable)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->wb_base_addr = ctx->wb_addr_p;
	reg->wb_pitch = ALIGN(ctx->vm.hactive, 16);

	if (enable)
		reg->wb_ctrl = 1;
	else
		reg->wb_ctrl = 0;

	 schedule_work(&ctx->wb_work);
}

static void dpu_wb_work_func(struct work_struct *data)
{
	int ret;
	struct dpu_context *ctx =
		container_of(data, struct dpu_context, wb_work);
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	ret = down_trylock(&ctx->refresh_lock);
	if (!ctx->is_inited) {
		pr_err("dpu is suspended\n");
		if (ret != 1)
			up(&ctx->refresh_lock);
		return;
	}

	if (ret != 1) {
		reg->dpu_ctrl |= BIT(5);
		dpu_wait_update_done(ctx);
		up(&ctx->refresh_lock);
	} else
		pr_debug("cannot acquire lock for wb_lock\n");
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

	if (wb_disable) {
		pr_debug("write back disabled\n");
		return -1;
	}
	if (!need_config) {
		pr_err("write back info has configed\n");
		return 0;
	}

	wb_en = 0;
	max_vsync_count = 0;
	vsync_count = 0;

	wb_buf_size = ALIGN(ctx->vm.hactive, 16) * ctx->vm.vactive * 4;
	ret = dpu_wb_buf_alloc(dpu, wb_buf_size, &ctx->wb_addr_p);

	if (ret) {
		max_vsync_count = 0;
		return -1;
	}

	wb_layer.index = 5;
	wb_layer.planes = 1;
	wb_layer.alpha = 0xff;
	wb_layer.dst_w = ctx->vm.hactive;
	wb_layer.dst_h = ctx->vm.vactive;
	wb_layer.format = DRM_FORMAT_ABGR8888;
	wb_layer.pitch[0] = ALIGN(ctx->vm.hactive, 16) * 4;
	wb_layer.addr[0] = ctx->wb_addr_p;

	max_vsync_count = 3;

	need_config = 0;
	return 0;
}

static int dpu_init(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 size;

	/* set bg color */
	reg->bg_color = 0;

	/* enable dithering */
	reg->dpu_ctrl |= BIT(6);

	/* enable DISPC Power Control */
	reg->dpu_ctrl |= BIT(7);

	/* clear update down register*/
	reg->dpu_int_clr |= BIT(4);

	/* set dpu output size */
	size = (ctx->vm.vactive << 16) | ctx->vm.hactive;
	reg->dpu_size = size;
	reg->dpu_qos = 0x411f0;
	reg->y2r_ctrl = 1;
	reg->y2r_y_param = DISPC_BRIGHTNESS | DISPC_CONTRAST;
	reg->y2r_u_param = DISPC_OFFSET_U | DISPC_SATURATION_U;
	reg->y2r_v_param = DISPC_OFFSET_V | DISPC_SATURATION_V;

	if (ctx->is_stopped)
		dpu_clean_all(ctx);

	reg->dpu_int_clr = 0xffff;

	if (!ctx->wb_work.func)
		INIT_WORK(&ctx->wb_work, dpu_wb_work_func);

	dpu_corner_init(ctx);

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
	DPU_LAYER_FORMAT_ARGB8888,
	DPU_LAYER_FORMAT_RGB565,
};

static u32 dpu_img_ctrl(u32 format, u32 blending)
{
	int reg_val = 0;

	/* layer enable */
	reg_val |= BIT(0);

	switch (format) {
	case DRM_FORMAT_BGRA8888:
		/* BGRA8888 -> ARGB8888 */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		/* RGBA8888 -> ABGR8888 */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
	case DRM_FORMAT_ABGR8888:
		/* rb switch */
		reg_val |= BIT(15);
	case DRM_FORMAT_ARGB8888:
		reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_XBGR8888:
		/* rb switch */
		reg_val |= BIT(15);
	case DRM_FORMAT_XRGB8888:
		reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_BGR565:
		/* rb switch */
		reg_val |= BIT(15);
	case DRM_FORMAT_RGB565:
		reg_val |= (DPU_LAYER_FORMAT_RGB565 << 4);
		break;
	case DRM_FORMAT_NV12:
		/*2-Lane: Yuv420 */
		reg_val |= DPU_LAYER_FORMAT_YUV420_2PLANE << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_NV21:
		/*2-Lane: Yuv420 */
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
		/* blending mode select - normal mode */
		reg_val &= (~BIT(16));
		break;
	case DRM_MODE_BLEND_PREMULTI:
		if (format == DRM_FORMAT_BGR565 ||
		    format == DRM_FORMAT_RGB565 ||
		    format == DRM_FORMAT_XRGB8888 ||
		    format == DRM_FORMAT_XBGR8888 ||
		    format == DRM_FORMAT_RGBX8888 ||
		    format == DRM_FORMAT_BGRX8888) {
			/* When the format is rgb565 or
			 * rgbx888, pixel alpha is zero.
			 * Layer alpha should be configured
			 * as block alpha.
			 */
			reg_val |= BIT(2);
		}
		/* blending mode select - pre-mult mode */
		reg_val |= BIT(16);
		break;
	default:
		/* alpha mode select - layer alpha */
		reg_val |= BIT(2);
		break;
	}

	return reg_val;
}

static void dpu_clean_all(struct dpu_context *ctx)
{
	int i;
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	for (i = 0; i < 6; i++)
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
		reg->dpu_ctrl |= BIT(5);
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
	u32 addr, size, offset, wd;
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
		addr = hwlayer->addr[i];

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

	layer->pitch = hwlayer->pitch[0] / wd;
	layer->ctrl = dpu_img_ctrl(hwlayer->format, hwlayer->blending);

	pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
				hwlayer->dst_x, hwlayer->dst_y,
				hwlayer->dst_w, hwlayer->dst_h);
	pr_debug("start_x = %d, start_y = %d, start_w = %d, start_h = %d\n",
				hwlayer->src_x, hwlayer->src_y,
				hwlayer->src_w, hwlayer->src_h);
}

static void dpu_clean_lite(struct dpu_context *ctx)
{
	int i;
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	for (i = 0; i < 5; i++)
		reg->layers[i].ctrl = 0;

}

static void dpu_flip(struct dpu_context *ctx,
		     struct sprd_dpu_layer layers[], u8 count)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int i, ret;

	vsync_count = 0;
	if (!wb_config && !wb_disable) {
		flip_cnt++;
		if (flip_cnt == 2) {
			ret = dpu_write_back_config(ctx);
			if (!ret)
				wb_config = true;
		}
	}
	if (max_vsync_count && count > 1)
		wb_en = true;
	else
		wb_en = false;

	/*
	 * Make sure the dpu is in stop status. In EDPI mode, the shadow
	 * registers can only be updated in the rising edge of DPU_RUN bit.
	 * And actually run when TE signal occurred.
	 */
	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);

	/* reset the bgcolor to black */
	reg->bg_color = 0;

	/* disable all the layers */
	dpu_clean_all(ctx);

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
			reg->dpu_ctrl |= BIT(5);
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
	reg->dpu_int_en |= DISPC_INT_MMU_VAOR_RD_MASK |
				DISPC_INT_MMU_VAOR_WR_MASK |
				DISPC_INT_MMU_INV_RD_MASK |
				DISPC_INT_MMU_INV_WR_MASK;
}

static void dpu_dpi_init(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 int_mask = 0;

	if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		/*use dpi as interface */
		reg->dpu_ctrl &= ~(BIT(1) | BIT(2));

		/* disable Halt function for SPRD DSI */
		reg->dpi_ctrl &= ~BIT(16);

		/* select te from external pad */
		reg->dpi_ctrl |= BIT(10);

		/* dpu pixel data width is 24 bit*/
		reg->dpi_ctrl |= BIT(7);

		/* set dpi timing */
		reg->dpi_h_timing = (ctx->vm.hsync_len << 0) |
				    (ctx->vm.hback_porch << 8) |
				    (ctx->vm.hfront_porch << 20);
		reg->dpi_v_timing = (ctx->vm.vsync_len << 0) |
				    (ctx->vm.vback_porch << 8) |
				    (ctx->vm.vfront_porch << 20);
		if (ctx->vm.vsync_len + ctx->vm.vback_porch < 32)
			pr_warn("Warning: (vsync + vbp) < 16, "
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
		/*use edpi as interface */
		reg->dpu_ctrl |= BIT(2);

		/* use external te */
		reg->dpi_ctrl |= BIT(10);

		/* enable te */
		reg->dpi_ctrl |= BIT(8);

		/* dpu pixel data width is 24 bit*/
		reg->dpi_ctrl |= BIT(7);

		/* enable stop DONE INT */
		int_mask |= DISPC_INT_DONE_MASK;
		/* enable TE INT */
		int_mask |= DISPC_INT_TE_MASK;
	}

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

static const u32 primary_fmts[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565, DRM_FORMAT_BGR565,
	DRM_FORMAT_NV12, DRM_FORMAT_NV21,
	DRM_FORMAT_NV16, DRM_FORMAT_NV61,
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

static struct dpu_core_ops dpu_lite_r1p0_ops = {
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
	.check_raw_int = dpu_check_raw_int,
};

static struct ops_entry entry = {
	.ver = "dpu-lite-r1p0",
	.ops = &dpu_lite_r1p0_ops,
};

static int __init dpu_core_register(void)
{
	return dpu_core_ops_register(&entry);
}

subsys_initcall(dpu_core_register);
