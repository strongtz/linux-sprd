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

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <linux/component.h>
#include <linux/dma-buf.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>
#include <linux/sprd_iommu.h>

#include "sprd_drm.h"
#include "sprd_dpu.h"
#include "sprd_gem.h"
#include "sysfs/sysfs_display.h"

struct sprd_plane {
	struct drm_plane plane;
	struct drm_property *alpha_property;
	struct drm_property *blend_mode_property;
	struct drm_property *fbc_hsize_r_property;
	struct drm_property *fbc_hsize_y_property;
	struct drm_property *fbc_hsize_uv_property;
	struct drm_property *y2r_coef_property;
	struct drm_property *pallete_en_property;
	struct drm_property *pallete_color_property;
	u32 index;
};

struct sprd_plane_state {
	struct drm_plane_state state;
	u8 alpha;
	u8 blend_mode;
	u32 fbc_hsize_r;
	u32 fbc_hsize_y;
	u32 fbc_hsize_uv;
	u32 y2r_coef;
	u32 pallete_en;
	u32 pallete_color;
};

LIST_HEAD(dpu_core_head);
LIST_HEAD(dpu_clk_head);
LIST_HEAD(dpu_glb_head);

bool calibration_mode;
static unsigned long frame_count;
module_param(frame_count, ulong, 0444);

static int sprd_dpu_init(struct sprd_dpu *dpu);
static int sprd_dpu_uninit(struct sprd_dpu *dpu);

static int boot_mode_check(char *str)
{
	if (str != NULL && !strncmp(str, "cali", strlen("cali")))
		calibration_mode = true;
	else
		calibration_mode = false;
	return 0;
}
__setup("androidboot.mode=", boot_mode_check);

static inline struct sprd_plane *to_sprd_plane(struct drm_plane *plane)
{
	return container_of(plane, struct sprd_plane, plane);
}

static inline struct
sprd_plane_state *to_sprd_plane_state(const struct drm_plane_state *state)
{
	return container_of(state, struct sprd_plane_state, state);
}

static int sprd_dpu_iommu_map(struct device *dev,
				struct sprd_gem_obj *sprd_gem)
{
	struct dma_buf *dma_buf;
	struct sprd_iommu_map_data iommu_data = {};

	dma_buf = sprd_gem->base.import_attach->dmabuf;
	iommu_data.buf = dma_buf->priv;
	iommu_data.iova_size = dma_buf->size;
	iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;

	if (sprd_iommu_map(dev, &iommu_data)) {
		DRM_ERROR("failed to map iommu address\n");
		return -EINVAL;
	}

	sprd_gem->dma_addr = iommu_data.iova_addr;

	return 0;
}

static void sprd_dpu_iommu_unmap(struct device *dev,
				struct sprd_gem_obj *sprd_gem)
{
	struct sprd_iommu_unmap_data iommu_data = {};

	iommu_data.iova_size = sprd_gem->base.size;
	iommu_data.iova_addr = sprd_gem->dma_addr;
	iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;

	if (sprd_iommu_unmap(dev, &iommu_data))
		DRM_ERROR("failed to unmap iommu address\n");
}

static int of_get_logo_memory_info(struct sprd_dpu *dpu,
	struct device_node *np)
{
	struct device_node *node;
	struct resource r;
	int ret;
	struct dpu_context *ctx = &dpu->ctx;

	node = of_parse_phandle(np, "sprd,logo-memory", 0);
	if (!node) {
		DRM_INFO("no sprd,logo-memory specified\n");
		return 0;
	}

	ret = of_address_to_resource(node, 0, &r);
	of_node_put(node);
	if (ret) {
		DRM_ERROR("invalid logo reserved memory node!\n");
		return -EINVAL;
	}

	ctx->logo_addr = r.start;
	ctx->logo_size = resource_size(&r);

	return 0;
}

static int sprd_plane_prepare_fb(struct drm_plane *plane,
				struct drm_plane_state *new_state)
{
	struct drm_plane_state *curr_state = plane->state;
	struct drm_framebuffer *fb;
	struct drm_gem_object *obj;
	struct sprd_gem_obj *sprd_gem;
	struct sprd_dpu *dpu;
	int i;

	if ((curr_state->fb == new_state->fb) || !new_state->fb)
		return 0;

	fb = new_state->fb;
	dpu = crtc_to_dpu(new_state->crtc);

	if (!dpu->ctx.is_inited) {
		DRM_WARN("dpu has already powered off\n");
		return 0;
	}

	for (i = 0; i < fb->format->num_planes; i++) {
		obj = drm_gem_fb_get_obj(fb, i);
		sprd_gem = to_sprd_gem_obj(obj);
		if (sprd_gem->need_iommu)
			sprd_dpu_iommu_map(&dpu->dev, sprd_gem);
	}

	return 0;
}

static void sprd_plane_cleanup_fb(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct drm_plane_state *curr_state = plane->state;
	struct drm_framebuffer *fb;
	struct drm_gem_object *obj;
	struct sprd_gem_obj *sprd_gem;
	struct sprd_dpu *dpu;
	int i;
	static atomic_t logo2animation = { -1 };

	if ((curr_state->fb == old_state->fb) || !old_state->fb)
		return;

	fb = old_state->fb;
	dpu = crtc_to_dpu(old_state->crtc);

	if (!dpu->ctx.is_inited) {
		DRM_WARN("dpu has already powered off\n");
		return;
	}

	for (i = 0; i < fb->format->num_planes; i++) {
		obj = drm_gem_fb_get_obj(fb, i);
		sprd_gem = to_sprd_gem_obj(obj);
		if (sprd_gem->need_iommu)
			sprd_dpu_iommu_unmap(&dpu->dev, sprd_gem);
	}

	if (unlikely(atomic_inc_not_zero(&logo2animation)) &&
		dpu->ctx.logo_addr) {
		DRM_INFO("free logo memory addr:0x%lx size:0x%lx\n",
			dpu->ctx.logo_addr, dpu->ctx.logo_size);
		free_reserved_area(phys_to_virt(dpu->ctx.logo_addr),
			phys_to_virt(dpu->ctx.logo_addr + dpu->ctx.logo_size),
			-1, "logo");
	}
}

static int sprd_plane_atomic_check(struct drm_plane *plane,
				  struct drm_plane_state *state)
{
	DRM_DEBUG("%s()\n", __func__);

	return 0;
}

static void sprd_plane_atomic_update(struct drm_plane *plane,
				    struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = plane->state;
	struct drm_framebuffer *fb = plane->state->fb;
	struct drm_gem_object *obj;
	struct sprd_gem_obj *sprd_gem;
	struct sprd_plane *p = to_sprd_plane(plane);
	struct sprd_plane_state *s = to_sprd_plane_state(state);
	struct sprd_dpu *dpu = crtc_to_dpu(plane->state->crtc);
	struct sprd_dpu_layer *layer = &dpu->layers[p->index];
	int i;

	if (plane->state->crtc->state->active_changed) {
		DRM_DEBUG("resume or suspend, no need to update plane\n");
		return;
	}

	if (s->pallete_en) {
		layer->index = p->index;
		layer->dst_x = state->crtc_x;
		layer->dst_y = state->crtc_y;
		layer->dst_w = state->crtc_w;
		layer->dst_h = state->crtc_h;
		layer->alpha = s->alpha;
		layer->blending = s->blend_mode;
		layer->pallete_en = s->pallete_en;
		layer->pallete_color = s->pallete_color;
		dpu->pending_planes++;
		DRM_DEBUG("%s() pallete_color = %u, index = %u\n",
			__func__, layer->pallete_color, layer->index);
		return;
	}

	layer->index = p->index;
	layer->src_x = state->src_x >> 16;
	layer->src_y = state->src_y >> 16;
	layer->src_w = state->src_w >> 16;
	layer->src_h = state->src_h >> 16;
	layer->dst_x = state->crtc_x;
	layer->dst_y = state->crtc_y;
	layer->dst_w = state->crtc_w;
	layer->dst_h = state->crtc_h;
	layer->rotation = state->rotation;
	layer->planes = fb->format->num_planes;
	layer->format = fb->format->format;
	layer->alpha = s->alpha;
	layer->blending = s->blend_mode;
	layer->xfbc = fb->modifier;
	layer->header_size_r = s->fbc_hsize_r;
	layer->header_size_y = s->fbc_hsize_y;
	layer->header_size_uv = s->fbc_hsize_uv;
	layer->y2r_coef = s->y2r_coef;
	layer->pallete_en = s->pallete_en;
	layer->pallete_color = s->pallete_color;

	DRM_DEBUG("%s() alpha = %u, blending = %u, rotation = %u, y2r_coef = %u\n",
		  __func__, layer->alpha, layer->blending, layer->rotation, s->y2r_coef);

	DRM_DEBUG("%s() xfbc = %u, hsize_r = %u, hsize_y = %u, hsize_uv = %u\n",
		  __func__, layer->xfbc, layer->header_size_r,
		  layer->header_size_y, layer->header_size_uv);

	for (i = 0; i < layer->planes; i++) {
		obj = drm_gem_fb_get_obj(fb, i);
		sprd_gem = to_sprd_gem_obj(obj);
		layer->addr[i] = sprd_gem->dma_addr + fb->offsets[i];
		layer->pitch[i] = fb->pitches[i];
	}

	dpu->pending_planes++;
}

static void sprd_plane_atomic_disable(struct drm_plane *plane,
				     struct drm_plane_state *old_state)
{
	struct sprd_plane *p = to_sprd_plane(plane);

	/*
	 * NOTE:
	 * The dpu->core->flip() will disable all the planes each time.
	 * So there is no need to impliment the atomic_disable() function.
	 * But this function can not be removed, because it will change
	 * to call atomic_update() callback instead. Which will cause
	 * kernel panic in sprd_plane_atomic_update().
	 *
	 * We do nothing here but just print a debug log.
	 */
	DRM_DEBUG("%s() layer_id = %u\n", __func__, p->index);
}

static void sprd_plane_reset(struct drm_plane *plane)
{
	struct sprd_plane *p = to_sprd_plane(plane);
	struct sprd_plane_state *s;

	DRM_INFO("%s()\n", __func__);

	if (plane->state) {
		__drm_atomic_helper_plane_destroy_state(plane->state);

		s = to_sprd_plane_state(plane->state);
		memset(s, 0, sizeof(*s));
	} else {
		s = kzalloc(sizeof(*s), GFP_KERNEL);
		if (!s)
			return;
		plane->state = &s->state;
	}

	s->state.plane = plane;
	s->state.zpos = p->index;
	s->alpha = 255;
	s->blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
}

static struct drm_plane_state *
sprd_plane_atomic_duplicate_state(struct drm_plane *plane)
{
	struct sprd_plane_state *s;
	struct sprd_plane_state *old_state = to_sprd_plane_state(plane->state);

	DRM_DEBUG("%s()\n", __func__);

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &s->state);

	WARN_ON(s->state.plane != plane);

	s->alpha = old_state->alpha;
	s->blend_mode = old_state->blend_mode;
	s->fbc_hsize_r = old_state->fbc_hsize_r;
	s->fbc_hsize_y = old_state->fbc_hsize_y;
	s->fbc_hsize_uv = old_state->fbc_hsize_uv;
	s->y2r_coef = old_state->y2r_coef;
	s->pallete_en = old_state->pallete_en;
	s->pallete_color = old_state->pallete_color;

	return &s->state;
}

static void sprd_plane_atomic_destroy_state(struct drm_plane *plane,
					    struct drm_plane_state *state)
{
	DRM_DEBUG("%s()\n", __func__);

	__drm_atomic_helper_plane_destroy_state(state);
	kfree(to_sprd_plane_state(state));
}

static int sprd_plane_atomic_set_property(struct drm_plane *plane,
					  struct drm_plane_state *state,
					  struct drm_property *property,
					  u64 val)
{
	struct sprd_plane *p = to_sprd_plane(plane);
	struct sprd_plane_state *s = to_sprd_plane_state(state);

	DRM_DEBUG("%s() name = %s, val = %llu\n",
		  __func__, property->name, val);

	if (property == p->alpha_property)
		s->alpha = val;
	else if (property == p->blend_mode_property)
		s->blend_mode = val;
	else if (property == p->fbc_hsize_r_property)
		s->fbc_hsize_r = val;
	else if (property == p->fbc_hsize_y_property)
		s->fbc_hsize_y = val;
	else if (property == p->fbc_hsize_uv_property)
		s->fbc_hsize_uv = val;
	else if (property == p->y2r_coef_property)
		s->y2r_coef = val;
	else if (property == p->pallete_en_property)
		s->pallete_en = val;
	else if (property == p->pallete_color_property)
		s->pallete_color = val;
	else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static int sprd_plane_atomic_get_property(struct drm_plane *plane,
					  const struct drm_plane_state *state,
					  struct drm_property *property,
					  u64 *val)
{
	struct sprd_plane *p = to_sprd_plane(plane);
	const struct sprd_plane_state *s = to_sprd_plane_state(state);

	DRM_DEBUG("%s() name = %s\n", __func__, property->name);

	if (property == p->alpha_property)
		*val = s->alpha;
	else if (property == p->blend_mode_property)
		*val = s->blend_mode;
	else if (property == p->fbc_hsize_r_property)
		*val = s->fbc_hsize_r;
	else if (property == p->fbc_hsize_y_property)
		*val = s->fbc_hsize_y;
	else if (property == p->fbc_hsize_uv_property)
		*val = s->fbc_hsize_uv;
	else if (property == p->y2r_coef_property)
		*val = s->y2r_coef;
	else if (property == p->pallete_en_property)
		*val = s->pallete_en;
	else if (property == p->pallete_color_property)
		*val = s->pallete_color;
	else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static int sprd_plane_create_properties(struct sprd_plane *p, int index)
{
	struct drm_property *prop;
	static const struct drm_prop_enum_list blend_mode_enum_list[] = {
		{ DRM_MODE_BLEND_PIXEL_NONE, "None" },
		{ DRM_MODE_BLEND_PREMULTI, "Pre-multiplied" },
		{ DRM_MODE_BLEND_COVERAGE, "Coverage" },
	};

	/* create rotation property */
	drm_plane_create_rotation_property(&p->plane,
					   DRM_MODE_ROTATE_0,
					   DRM_MODE_ROTATE_MASK |
					   DRM_MODE_REFLECT_MASK);

	/* create zpos property */
	drm_plane_create_zpos_immutable_property(&p->plane, index);

	/* create layer alpha property */
	prop = drm_property_create_range(p->plane.dev, 0, "alpha", 0, 255);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 255);
	p->alpha_property = prop;

	/* create blend mode property */
	prop = drm_property_create_enum(p->plane.dev, DRM_MODE_PROP_ENUM,
					"pixel blend mode",
					blend_mode_enum_list,
					ARRAY_SIZE(blend_mode_enum_list));
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop,
				   DRM_MODE_BLEND_PIXEL_NONE);
	p->blend_mode_property = prop;

	/* create fbc header size property */
	prop = drm_property_create_range(p->plane.dev, 0,
			"FBC header size RGB", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->fbc_hsize_r_property = prop;

	prop = drm_property_create_range(p->plane.dev, 0,
			"FBC header size Y", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->fbc_hsize_y_property = prop;

	prop = drm_property_create_range(p->plane.dev, 0,
			"FBC header size UV", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->fbc_hsize_uv_property = prop;

	/* create y2r coef property */
	prop = drm_property_create_range(p->plane.dev, 0,
			"YUV2RGB coef", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->y2r_coef_property = prop;

	/* create pallete enable property */
	prop = drm_property_create_range(p->plane.dev, 0,
			"pallete enable", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->pallete_en_property = prop;

	/* create pallete color property */
	prop = drm_property_create_range(p->plane.dev, 0,
			"pallete color", 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	drm_object_attach_property(&p->plane.base, prop, 0);
	p->pallete_color_property = prop;

	return 0;
}

static const struct drm_plane_helper_funcs sprd_plane_helper_funcs = {
	.prepare_fb = sprd_plane_prepare_fb,
	.cleanup_fb = sprd_plane_cleanup_fb,
	.atomic_check = sprd_plane_atomic_check,
	.atomic_update = sprd_plane_atomic_update,
	.atomic_disable = sprd_plane_atomic_disable,
};

static const struct drm_plane_funcs sprd_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = sprd_plane_reset,
	.atomic_duplicate_state = sprd_plane_atomic_duplicate_state,
	.atomic_destroy_state = sprd_plane_atomic_destroy_state,
	.atomic_set_property = sprd_plane_atomic_set_property,
	.atomic_get_property = sprd_plane_atomic_get_property,
};

static struct drm_plane *sprd_plane_init(struct drm_device *drm,
					struct sprd_dpu *dpu)
{
	struct drm_plane *primary = NULL;
	struct sprd_plane *p = NULL;
	struct dpu_capability cap = {};
	int err, i;

	if (dpu->core && dpu->core->capability)
		dpu->core->capability(&dpu->ctx, &cap);

	dpu->layers = devm_kcalloc(drm->dev, cap.max_layers,
				  sizeof(struct sprd_dpu_layer), GFP_KERNEL);
	if (!dpu->layers)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < cap.max_layers; i++) {

		p = devm_kzalloc(drm->dev, sizeof(*p), GFP_KERNEL);
		if (!p)
			return ERR_PTR(-ENOMEM);

		err = drm_universal_plane_init(drm, &p->plane, 1,
					       &sprd_plane_funcs, cap.fmts_ptr,
					       cap.fmts_cnt, NULL,
					       DRM_PLANE_TYPE_PRIMARY, NULL);
		if (err) {
			DRM_ERROR("fail to init primary plane\n");
			return ERR_PTR(err);
		}

		drm_plane_helper_add(&p->plane, &sprd_plane_helper_funcs);

		sprd_plane_create_properties(p, i);

		p->index = i;
		if (i == 0)
			primary = &p->plane;
	}

	if (p)
		DRM_INFO("dpu plane init ok\n");

	return primary;
}

static void sprd_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;

	DRM_INFO("%s() set mode: %s\n", __func__, dpu->mode->name);

	/*
	 * TODO:
	 * Currently, low simulator resolution only support
	 * DPI mode, support for EDPI in the future.
	 */
	if (mode->type & DRM_MODE_TYPE_BUILTIN) {
		dpu->ctx.if_type = SPRD_DISPC_IF_DPI;
		return;
	}

	if ((dpu->mode->hdisplay == dpu->mode->htotal) ||
	    (dpu->mode->vdisplay == dpu->mode->vtotal))
		dpu->ctx.if_type = SPRD_DISPC_IF_EDPI;
	else
		dpu->ctx.if_type = SPRD_DISPC_IF_DPI;

	if (dpu->core && dpu->core->modeset) {
		if (crtc->state->mode_changed) {
			struct drm_mode_modeinfo umode;

			drm_mode_convert_to_umode(&umode, mode);
			dpu->core->modeset(&dpu->ctx, &umode);
		}
	}
}

static enum drm_mode_status sprd_crtc_mode_valid(struct drm_crtc *crtc,
					const struct drm_display_mode *mode)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_INFO("%s() mode: "DRM_MODE_FMT"\n", __func__, DRM_MODE_ARG(mode));

	if (mode->type & DRM_MODE_TYPE_DEFAULT)
		dpu->mode = (struct drm_display_mode *)mode;

	if (mode->type & DRM_MODE_TYPE_PREFERRED) {
		dpu->mode = (struct drm_display_mode *)mode;
		drm_display_mode_to_videomode(dpu->mode, &dpu->ctx.vm);
	}

	if (mode->type & DRM_MODE_TYPE_BUILTIN)
		dpu->mode = (struct drm_display_mode *)mode;

	return MODE_OK;
}

static void sprd_crtc_atomic_enable(struct drm_crtc *crtc,
				   struct drm_crtc_state *old_state)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);
	static bool is_enabled = true;

	DRM_INFO("%s()\n", __func__);

	/*
	 * add if condition to avoid resume dpu for SR feature.
	 */
	if (crtc->state->mode_changed && !crtc->state->active_changed)
		return;

	if (is_enabled)
		is_enabled = false;
	else
		pm_runtime_get_sync(dpu->dev.parent);

	sprd_dpu_init(dpu);

	enable_irq(dpu->ctx.irq);

	sprd_iommu_restore(&dpu->dev);
}

static void sprd_crtc_wait_last_commit_complete(struct drm_crtc *crtc)
{
	struct drm_crtc_commit *commit;
	int ret, i = 0;

	spin_lock(&crtc->commit_lock);
	list_for_each_entry(commit, &crtc->commit_list, commit_entry) {
		i++;
		/* skip the first entry, that's the current commit */
		if (i == 2)
			break;
	}
	if (i == 2)
		drm_crtc_commit_get(commit);
	spin_unlock(&crtc->commit_lock);

	if (i != 2)
		return;

	ret = wait_for_completion_interruptible_timeout(&commit->cleanup_done,
							HZ);
	if (ret == 0)
		DRM_WARN("wait last commit completion timed out\n");

	drm_crtc_commit_put(commit);
}

static void sprd_crtc_atomic_disable(struct drm_crtc *crtc,
				    struct drm_crtc_state *old_state)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);
	struct drm_device *drm = dpu->crtc.dev;

	DRM_INFO("%s()\n", __func__);

	/* add if condition to avoid suspend dpu for SR feature */
	if (crtc->state->mode_changed && !crtc->state->active_changed)
		return;

	sprd_crtc_wait_last_commit_complete(crtc);

	disable_irq(dpu->ctx.irq);

	sprd_dpu_uninit(dpu);

	pm_runtime_put(dpu->dev.parent);

	spin_lock_irq(&drm->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&drm->event_lock);
}

static int sprd_crtc_atomic_check(struct drm_crtc *crtc,
				 struct drm_crtc_state *state)
{
	DRM_DEBUG("%s()\n", __func__);

	return 0;
}

static void sprd_crtc_atomic_begin(struct drm_crtc *crtc,
				  struct drm_crtc_state *old_state)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_DEBUG("%s()\n", __func__);

	down(&dpu->ctx.refresh_lock);

	memset(dpu->layers, 0, sizeof(*dpu->layers) * dpu->pending_planes);

	dpu->pending_planes = 0;
}

static void sprd_crtc_atomic_flush(struct drm_crtc *crtc,
				  struct drm_crtc_state *old_state)

{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);
	struct drm_device *drm = dpu->crtc.dev;

	DRM_DEBUG("%s()\n", __func__);

	if (dpu->core && dpu->core->flip &&
	    dpu->pending_planes && !dpu->ctx.disable_flip) {
		dpu->core->flip(&dpu->ctx, dpu->layers, dpu->pending_planes);
		frame_count++;
	}

	up(&dpu->ctx.refresh_lock);

	spin_lock_irq(&drm->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&drm->event_lock);
}

static int sprd_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_DEBUG("%s()\n", __func__);

	if (dpu->core && dpu->core->enable_vsync)
		dpu->core->enable_vsync(&dpu->ctx);

	return 0;
}

static void sprd_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);

	DRM_DEBUG("%s()\n", __func__);

	if (dpu->core && dpu->core->disable_vsync)
		dpu->core->disable_vsync(&dpu->ctx);
}

static int sprd_crtc_create_properties(struct drm_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc_to_dpu(crtc);
	struct drm_device *drm = dpu->crtc.dev;
	struct drm_property *prop;
	struct drm_property_blob *blob;
	size_t blob_size;

	blob_size = strlen(dpu->ctx.version) + 1;

	blob = drm_property_create_blob(dpu->crtc.dev, blob_size,
			dpu->ctx.version);
	if (IS_ERR(blob)) {
		DRM_ERROR("drm_property_create_blob dpu version failed\n");
		return PTR_ERR(blob);
	}

	/* create dpu version property */
	prop = drm_property_create(drm,
		DRM_MODE_PROP_IMMUTABLE | DRM_MODE_PROP_BLOB,
		"dpu version", 0);
	if (!prop) {
		DRM_ERROR("drm_property_create dpu version failed\n");
		return -ENOMEM;
	}
	drm_object_attach_property(&crtc->base, prop, blob->base.id);

	/* create corner size property */
	prop = drm_property_create(drm,
		DRM_MODE_PROP_IMMUTABLE | DRM_MODE_PROP_RANGE,
		"corner size", 0);
	if (!prop) {
		DRM_ERROR("drm_property_create corner size failed\n");
		return -ENOMEM;
	}
	drm_object_attach_property(&crtc->base, prop, dpu->ctx.corner_size);

	return 0;
}

static const struct drm_crtc_helper_funcs sprd_crtc_helper_funcs = {
	.mode_set_nofb	= sprd_crtc_mode_set_nofb,
	.mode_valid	= sprd_crtc_mode_valid,
	.atomic_check	= sprd_crtc_atomic_check,
	.atomic_begin	= sprd_crtc_atomic_begin,
	.atomic_flush	= sprd_crtc_atomic_flush,
	.atomic_enable	= sprd_crtc_atomic_enable,
	.atomic_disable	= sprd_crtc_atomic_disable,
};

static const struct drm_crtc_funcs sprd_crtc_funcs = {
	.destroy	= drm_crtc_cleanup,
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.reset		= drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state	= drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_crtc_destroy_state,
	.enable_vblank	= sprd_crtc_enable_vblank,
	.disable_vblank	= sprd_crtc_disable_vblank,
};

static int sprd_crtc_init(struct drm_device *drm, struct drm_crtc *crtc,
			 struct drm_plane *primary)
{
	struct device_node *port;
	int err;

	/*
	 * set crtc port so that drm_of_find_possible_crtcs call works
	 */
	port = of_parse_phandle(drm->dev->of_node, "ports", 0);
	if (!port) {
		DRM_ERROR("find 'ports' phandle of %s failed\n",
			  drm->dev->of_node->full_name);
		return -EINVAL;
	}
	of_node_put(port);
	crtc->port = port;

	err = drm_crtc_init_with_planes(drm, crtc, primary, NULL,
					&sprd_crtc_funcs, NULL);
	if (err) {
		DRM_ERROR("failed to init crtc.\n");
		return err;
	}

	drm_mode_crtc_set_gamma_size(crtc, 256);

	drm_crtc_helper_add(crtc, &sprd_crtc_helper_funcs);

	sprd_crtc_create_properties(crtc);

	DRM_INFO("%s() ok\n", __func__);
	return 0;
}

int sprd_dpu_run(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	down(&ctx->refresh_lock);

	if (!ctx->is_inited) {
		DRM_ERROR("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}

	if (!ctx->is_stopped) {
		up(&ctx->refresh_lock);
		return 0;
	}

	if (dpu->core && dpu->core->run)
		dpu->core->run(ctx);

	up(&ctx->refresh_lock);

	drm_crtc_vblank_on(&dpu->crtc);

	return 0;
}

int sprd_dpu_stop(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	down(&ctx->refresh_lock);

	if (!ctx->is_inited) {
		DRM_ERROR("dpu is not initialized\n");
		up(&ctx->refresh_lock);
		return -EINVAL;
	}

	if (ctx->is_stopped) {
		up(&ctx->refresh_lock);
		return 0;
	}

	if (dpu->core && dpu->core->stop)
		dpu->core->stop(ctx);

	up(&ctx->refresh_lock);

	drm_crtc_handle_vblank(&dpu->crtc);
	drm_crtc_vblank_off(&dpu->crtc);

	return 0;
}

static int sprd_dpu_init(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	down(&ctx->refresh_lock);

	if (dpu->ctx.is_inited) {
		up(&ctx->refresh_lock);
		return 0;
	}

	if (dpu->glb && dpu->glb->power)
		dpu->glb->power(ctx, true);
	if (dpu->glb && dpu->glb->enable)
		dpu->glb->enable(ctx);

	if (ctx->is_stopped && dpu->glb && dpu->glb->reset)
		dpu->glb->reset(ctx);

	if (dpu->clk && dpu->clk->init)
		dpu->clk->init(ctx);
	if (dpu->clk && dpu->clk->enable)
		dpu->clk->enable(ctx);

	if (dpu->core && dpu->core->init)
		dpu->core->init(ctx);
	if (dpu->core && dpu->core->ifconfig)
		dpu->core->ifconfig(ctx);

	ctx->is_inited = true;

	up(&ctx->refresh_lock);

	return 0;
}

static int sprd_dpu_uninit(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	down(&ctx->refresh_lock);

	if (!dpu->ctx.is_inited) {
		up(&ctx->refresh_lock);
		return 0;
	}

	if (dpu->core && dpu->core->uninit)
		dpu->core->uninit(ctx);
	if (dpu->clk && dpu->clk->disable)
		dpu->clk->disable(ctx);
	if (dpu->glb && dpu->glb->disable)
		dpu->glb->disable(ctx);
	if (dpu->glb && dpu->glb->power)
		dpu->glb->power(ctx, false);

	ctx->is_inited = false;

	up(&ctx->refresh_lock);

	return 0;
}

static irqreturn_t sprd_dpu_isr(int irq, void *data)
{
	struct sprd_dpu *dpu = data;
	struct dpu_context *ctx = &dpu->ctx;
	u32 int_mask = 0;

	if (dpu->core && dpu->core->isr)
		int_mask = dpu->core->isr(ctx);

	if (int_mask & DISPC_INT_TE_MASK) {
		if (ctx->te_check_en) {
			ctx->evt_te = true;
			wake_up_interruptible_all(&ctx->te_wq);
		}
	}

	if (int_mask & DISPC_INT_ERR_MASK)
		DRM_WARN("Warning: dpu underflow!\n");

	if ((int_mask & DISPC_INT_DPI_VSYNC_MASK) && ctx->is_inited)
		drm_crtc_handle_vblank(&dpu->crtc);

	return IRQ_HANDLED;
}

static int sprd_dpu_irq_request(struct sprd_dpu *dpu)
{
	int err;
	int irq_num;

	irq_num = irq_of_parse_and_map(dpu->dev.of_node, 0);
	if (!irq_num) {
		DRM_ERROR("error: dpu parse irq num failed\n");
		return -EINVAL;
	}
	DRM_INFO("dpu irq_num = %d\n", irq_num);

	irq_set_status_flags(irq_num, IRQ_NOAUTOEN);
	err = devm_request_irq(&dpu->dev, irq_num, sprd_dpu_isr,
					0, "DISPC", dpu);
	if (err) {
		DRM_ERROR("error: dpu request irq failed\n");
		return -EINVAL;
	}
	dpu->ctx.irq = irq_num;
	dpu->ctx.dpu_isr = sprd_dpu_isr;

	return 0;
}

static int sprd_dpu_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct sprd_drm *sprd = drm->dev_private;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct drm_plane *plane;
	int err;

	DRM_INFO("%s()\n", __func__);

	plane = sprd_plane_init(drm, dpu);
	if (IS_ERR_OR_NULL(plane)) {
		err = PTR_ERR(plane);
		return err;
	}

	err = sprd_crtc_init(drm, &dpu->crtc, plane);
	if (err)
		return err;

	sprd_dpu_irq_request(dpu);

	sprd->dpu_dev = dev;

	return 0;
}

static void sprd_dpu_unbind(struct device *dev, struct device *master,
	void *data)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);

	DRM_INFO("%s()\n", __func__);

	drm_crtc_cleanup(&dpu->crtc);
}

static const struct component_ops dpu_component_ops = {
	.bind = sprd_dpu_bind,
	.unbind = sprd_dpu_unbind,
};

static int sprd_dpu_device_create(struct sprd_dpu *dpu,
				struct device *parent)
{
	int err;

	dpu->dev.class = display_class;
	dpu->dev.parent = parent;
	dpu->dev.of_node = parent->of_node;
	dev_set_name(&dpu->dev, "dispc%d", dpu->ctx.id);
	dev_set_drvdata(&dpu->dev, dpu);

	err = device_register(&dpu->dev);
	if (err)
		DRM_ERROR("dpu device register failed\n");

	return err;
}

static int sprd_dpu_context_init(struct sprd_dpu *dpu,
				struct device_node *np)
{
	u32 temp;
	struct resource r;
	struct dpu_context *ctx = &dpu->ctx;

	if (dpu->core && dpu->core->parse_dt)
		dpu->core->parse_dt(&dpu->ctx, np);
	if (dpu->clk && dpu->clk->parse_dt)
		dpu->clk->parse_dt(&dpu->ctx, np);
	if (dpu->glb && dpu->glb->parse_dt)
		dpu->glb->parse_dt(&dpu->ctx, np);

	if (of_property_read_bool(np, "sprd,initial-stop-state")) {
		DRM_WARN("DPU is not initialized before entering kernel\n");
		dpu->ctx.is_stopped = true;
	}

	if (!of_property_read_u32(np, "sprd,dev-id", &temp))
		ctx->id = temp;

	if (of_address_to_resource(np, 0, &r)) {
		DRM_ERROR("parse dt base address failed\n");
		return -ENODEV;
	}
	ctx->base = (unsigned long)ioremap_nocache(r.start,
					resource_size(&r));
	if (ctx->base == 0) {
		DRM_ERROR("ioremap base address failed\n");
		return -EFAULT;
	}

	of_get_logo_memory_info(dpu, np);

	sema_init(&ctx->refresh_lock, 1);

	return 0;
}

static int sprd_dpu_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sprd_dpu *dpu;
	const char *str;
	int ret;

	if (calibration_mode) {
		DRM_WARN("Calibration Mode! Don't register sprd dpu driver\n");
		return -ENODEV;
	}

	dpu = devm_kzalloc(&pdev->dev, sizeof(*dpu), GFP_KERNEL);
	if (!dpu)
		return -ENOMEM;

	if (!of_property_read_string(np, "sprd,ip", &str)) {
		dpu->core = dpu_core_ops_attach(str);
		dpu->ctx.version = str;
	} else
		DRM_WARN("sprd,ip was not found\n");

	if (!of_property_read_string(np, "sprd,soc", &str)) {
		dpu->clk = dpu_clk_ops_attach(str);
		dpu->glb = dpu_glb_ops_attach(str);
	} else
		DRM_WARN("sprd,soc was not found\n");

	ret = sprd_dpu_context_init(dpu, np);
	if (ret)
		return ret;

	sprd_dpu_device_create(dpu, &pdev->dev);
	sprd_dpu_sysfs_init(&dpu->dev);
	platform_set_drvdata(pdev, dpu);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return component_add(&pdev->dev, &dpu_component_ops);
}

static int sprd_dpu_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dpu_component_ops);
	return 0;
}

static const struct of_device_id dpu_match_table[] = {
	{ .compatible = "sprd,display-processor",},
	{},
};

static struct platform_driver sprd_dpu_driver = {
	.probe = sprd_dpu_probe,
	.remove = sprd_dpu_remove,
	.driver = {
		.name = "sprd-dpu-drv",
		.of_match_table = dpu_match_table,
	},
};
module_platform_driver(sprd_dpu_driver);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("SPRD Display Controller Driver");
MODULE_LICENSE("GPL v2");
