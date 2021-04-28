/*
 *Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 */

#ifndef __SPRD_DPU_H__
#define __SPRD_DPU_H__

#include <linux/delay.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <video/videomode.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_vblank.h>
#include <uapi/drm/drm_mode.h>
#include "disp_lib.h"

#define DRM_MODE_BLEND_PREMULTI		2
#define DRM_MODE_BLEND_COVERAGE		1
#define DRM_MODE_BLEND_PIXEL_NONE	0

#define DISPC_INT_DONE_MASK		BIT(0)
#define DISPC_INT_TE_MASK		BIT(1)
#define DISPC_INT_ERR_MASK		BIT(2)
#define DISPC_INT_EDPI_TE_MASK		BIT(3)
#define DISPC_INT_UPDATE_DONE_MASK	BIT(4)
#define DISPC_INT_DPI_VSYNC_MASK	BIT(5)
#define DISPC_INT_WB_DONE_MASK		BIT(6)
#define DISPC_INT_WB_FAIL_MASK		BIT(7)

/* NOTE: this mask is not a realy dpu interrupt mask */
#define DISPC_INT_FENCE_SIGNAL_REQUEST	BIT(31)

enum {
	SPRD_DISPC_IF_DBI = 0,
	SPRD_DISPC_IF_DPI,
	SPRD_DISPC_IF_EDPI,
	SPRD_DISPC_IF_LIMIT
};

enum {
	SPRD_IMG_DATA_ENDIAN_B0B1B2B3 = 0,
	SPRD_IMG_DATA_ENDIAN_B3B2B1B0,
	SPRD_IMG_DATA_ENDIAN_B2B3B0B1,
	SPRD_IMG_DATA_ENDIAN_B1B0B3B2,
	SPRD_IMG_DATA_ENDIAN_LIMIT
};

enum {
	DISPC_CLK_ID_CORE = 0,
	DISPC_CLK_ID_DBI,
	DISPC_CLK_ID_DPI,
	DISPC_CLK_ID_MAX
};

enum {
	ENHANCE_CFG_ID_ENABLE,
	ENHANCE_CFG_ID_DISABLE,
	ENHANCE_CFG_ID_SCL,
	ENHANCE_CFG_ID_EPF,
	ENHANCE_CFG_ID_HSV,
	ENHANCE_CFG_ID_CM,
	ENHANCE_CFG_ID_SLP,
	ENHANCE_CFG_ID_GAMMA,
	ENHANCE_CFG_ID_LTM,
	ENHANCE_CFG_ID_CABC,
	ENHANCE_CFG_ID_SLP_LUT,
	ENHANCE_CFG_ID_LUT3D,
	ENHANCE_CFG_ID_SR_EPF,
	ENHANCE_CFG_ID_MAX
};

struct sprd_dpu_layer {
	u8 index;
	u8 planes;
	u32 addr[4];
	u32 pitch[4];
	s16 src_x;
	s16 src_y;
	s16 src_w;
	s16 src_h;
	s16 dst_x;
	s16 dst_y;
	u16 dst_w;
	u16 dst_h;
	u32 format;
	u32 alpha;
	u32 blending;
	u32 rotation;
	u32 xfbc;
	u32 height;
	u32 header_size_r;
	u32 header_size_y;
	u32 header_size_uv;
	u32 y2r_coef;
	u8 pallete_en;
	u32 pallete_color;
};

struct dpu_capability {
	u32 max_layers;
	const u32 *fmts_ptr;
	u32 fmts_cnt;
};

struct dpu_context;

struct dpu_core_ops {
	int (*parse_dt)(struct dpu_context *ctx,
			struct device_node *np);
	u32 (*version)(struct dpu_context *ctx);
	int (*init)(struct dpu_context *ctx);
	void (*uninit)(struct dpu_context *ctx);
	void (*run)(struct dpu_context *ctx);
	void (*stop)(struct dpu_context *ctx);
	void (*disable_vsync)(struct dpu_context *ctx);
	void (*enable_vsync)(struct dpu_context *ctx);
	u32 (*isr)(struct dpu_context *ctx);
	void (*ifconfig)(struct dpu_context *ctx);
	void (*write_back)(struct dpu_context *ctx, u8 count, bool debug);
	void (*flip)(struct dpu_context *ctx,
		     struct sprd_dpu_layer layers[], u8 count);
	int (*capability)(struct dpu_context *ctx,
			struct dpu_capability *cap);
	void (*bg_color)(struct dpu_context *ctx, u32 color);
	void (*enhance_set)(struct dpu_context *ctx, u32 id, void *param);
	void (*enhance_get)(struct dpu_context *ctx, u32 id, void *param);
	int (*modeset)(struct dpu_context *ctx,
			struct drm_mode_modeinfo *mode);
	bool (*check_raw_int)(struct dpu_context *ctx, u32 mask);
};

struct dpu_clk_ops {
	int (*parse_dt)(struct dpu_context *ctx,
			struct device_node *np);
	int (*init)(struct dpu_context *ctx);
	int (*uinit)(struct dpu_context *ctx);
	int (*enable)(struct dpu_context *ctx);
	int (*disable)(struct dpu_context *ctx);
	int (*update)(struct dpu_context *ctx, int clk_id, int val);
};

struct dpu_glb_ops {
	int (*parse_dt)(struct dpu_context *ctx,
			struct device_node *np);
	void (*enable)(struct dpu_context *ctx);
	void (*disable)(struct dpu_context *ctx);
	void (*reset)(struct dpu_context *ctx);
	void (*power)(struct dpu_context *ctx, int enable);
};

struct dpu_context {
	unsigned long base;
	u32 base_offset[2];
	const char *version;
	u32 corner_size;
	int irq;
	u8 if_type;
	u8 id;
	bool is_inited;
	bool is_stopped;
	bool disable_flip;
	struct videomode vm;
	struct semaphore refresh_lock;
	struct work_struct wb_work;
	struct tasklet_struct dvfs_task;
	u32 wb_addr_p;
	irqreturn_t (*dpu_isr)(int irq, void *data);
	wait_queue_head_t te_wq;
	bool te_check_en;
	bool evt_te;
	unsigned long logo_addr;
	unsigned long logo_size;
	struct work_struct cabc_work;
	struct work_struct cabc_bl_update;
};

struct sprd_dpu {
	struct device dev;
	struct drm_crtc crtc;
	struct dpu_context ctx;
	struct dpu_core_ops *core;
	struct dpu_clk_ops *clk;
	struct dpu_glb_ops *glb;
	struct drm_display_mode *mode;
	struct sprd_dpu_layer *layers;
	u8 pending_planes;
};

extern struct list_head dpu_core_head;
extern struct list_head dpu_clk_head;
extern struct list_head dpu_glb_head;
extern bool calibration_mode;

static inline struct sprd_dpu *crtc_to_dpu(struct drm_crtc *crtc)
{
	return crtc ? container_of(crtc, struct sprd_dpu, crtc) : NULL;
}

#define dpu_core_ops_register(entry) \
	disp_ops_register(entry, &dpu_core_head)
#define dpu_clk_ops_register(entry) \
	disp_ops_register(entry, &dpu_clk_head)
#define dpu_glb_ops_register(entry) \
	disp_ops_register(entry, &dpu_glb_head)

#define dpu_core_ops_attach(str) \
	disp_ops_attach(str, &dpu_core_head)
#define dpu_clk_ops_attach(str) \
	disp_ops_attach(str, &dpu_clk_head)
#define dpu_glb_ops_attach(str) \
	disp_ops_attach(str, &dpu_glb_head)

int sprd_dpu_run(struct sprd_dpu *dpu);
int sprd_dpu_stop(struct sprd_dpu *dpu);

#endif
