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

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <linux/component.h>
#include <linux/of_platform.h>
#include <video/of_display_timing.h>

#include "sprd_dpu.h"

struct dummy_connector {
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_display_mode mode;
};

#define encoder_to_dummy(encoder) \
	container_of(encoder, struct dummy_connector, encoder)
#define connector_to_dummy(connector) \
	container_of(connector, struct dummy_connector, connector)

__weak int sprd_dpu_run(struct sprd_dpu *dpu) { return 0; }
__weak int sprd_dpu_stop(struct sprd_dpu *dpu) { return 0; }

static void sprd_dummy_encoder_enable(struct drm_encoder *encoder)
{

	DRM_INFO("%s()\n", __func__);

	/* special case for dpu crtc */
	if (strcmp(encoder->crtc->name, "dummy-crtc")) {
		struct sprd_dpu *dpu = crtc_to_dpu(encoder->crtc);

		sprd_dpu_run(dpu);
	}
}

static void sprd_dummy_encoder_disable(struct drm_encoder *encoder)
{
	DRM_INFO("%s()\n", __func__);

	/* special case for dpu crtc */
	if (strcmp(encoder->crtc->name, "dummy-crtc")) {
		struct sprd_dpu *dpu = crtc_to_dpu(encoder->crtc);

		sprd_dpu_stop(dpu);
	}
}

static const struct drm_encoder_helper_funcs dummy_encoder_helper_funcs = {
	.enable = sprd_dummy_encoder_enable,
	.disable = sprd_dummy_encoder_disable
};

static const struct drm_encoder_funcs dummy_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int sprd_dummy_encoder_init(struct drm_device *drm,
			       struct dummy_connector *dummy)
{
	struct drm_encoder *encoder = &dummy->encoder;
	int ret;

	encoder->possible_crtcs = 0x01;
	ret = drm_encoder_init(drm, encoder, &dummy_encoder_funcs,
			       DRM_MODE_ENCODER_DPI, NULL);
	if (ret) {
		DRM_ERROR("failed to init dummy encoder\n");
		return ret;
	}

	drm_encoder_helper_add(encoder, &dummy_encoder_helper_funcs);

	return 0;
}

static int sprd_dummy_connector_get_modes(struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct dummy_connector *dummy = connector_to_dummy(connector);

	mode = drm_mode_duplicate(connector->dev, &dummy->mode);
	if (!mode) {
		DRM_ERROR("failed to add mode %ux%ux@%u\n",
			  dummy->mode.hdisplay,
			  dummy->mode.vdisplay,
			  dummy->mode.vrefresh);
		return -ENOMEM;
	}

	DRM_INFO("%s() mode: "DRM_MODE_FMT"\n", __func__, DRM_MODE_ARG(mode));

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	return 1;
}

static struct drm_connector_helper_funcs dummy_connector_helper_funcs = {
	.get_modes = sprd_dummy_connector_get_modes,
};

static void sprd_dummy_connector_destroy(struct drm_connector *connector)
{
	DRM_INFO("%s()\n", __func__);

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs dummy_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = sprd_dummy_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int sprd_dummy_connector_init(struct drm_device *drm,
				     struct dummy_connector *dummy)
{
	struct drm_encoder *encoder = &dummy->encoder;
	struct drm_connector *connector = &dummy->connector;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_init(drm, connector,
				 &dummy_connector_funcs,
				 DRM_MODE_CONNECTOR_DPI);
	if (ret) {
		DRM_ERROR("drm_connector_init() failed\n");
		return ret;
	}

	drm_connector_helper_add(connector,
				 &dummy_connector_helper_funcs);

	drm_mode_connector_attach_encoder(connector, encoder);

	return 0;
}

static int sprd_dummy_connector_bind(struct device *dev, struct device *master,
				   void *data)
{
	struct drm_device *drm = data;
	struct dummy_connector *dummy = dev_get_drvdata(dev);
	int ret;

	ret = sprd_dummy_encoder_init(drm, dummy);
	if (ret)
		return ret;

	ret = sprd_dummy_connector_init(drm, dummy);
	if (ret) {
		drm_encoder_cleanup(&dummy->encoder);
		return ret;
	}

	return 0;
}

static const struct component_ops dummy_connector_component_ops = {
	.bind = sprd_dummy_connector_bind,
};

static int sprd_dummy_connector_parse_dt(struct device_node *np,
				struct dummy_connector *dummy)
{
	int rc;
	u32 val;

	rc = of_get_drm_display_mode(np, &dummy->mode, 0,
				     OF_USE_NATIVE_MODE);
	if (rc) {
		DRM_ERROR("get display timing failed\n");
		return rc;
	}

	dummy->mode.vrefresh = drm_mode_vrefresh(&dummy->mode);

	rc = of_property_read_u32(np, "sprd,width-mm", &val);
	if (!rc)
		dummy->mode.width_mm = val;
	else
		dummy->mode.width_mm = 68;

	rc = of_property_read_u32(np, "sprd,height-mm", &val);
	if (!rc)
		dummy->mode.height_mm = val;
	else
		dummy->mode.height_mm = 121;

	return 0;
}

static int sprd_dummy_connector_probe(struct platform_device *pdev)
{
	struct dummy_connector *dummy;
	int ret;

	dummy = devm_kzalloc(&pdev->dev, sizeof(*dummy), GFP_KERNEL);
	if (!dummy) {
		DRM_ERROR("failed to allocate dummy data.\n");
		return -ENOMEM;
	}

	ret = sprd_dummy_connector_parse_dt(pdev->dev.of_node, dummy);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, dummy);

	return component_add(&pdev->dev, &dummy_connector_component_ops);
}

static int sprd_dummy_connector_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dummy_connector_component_ops);

	return 0;
}

static const struct of_device_id dummy_connector_of_match[] = {
	{.compatible = "sprd,dummy-connector"},
	{ }
};
MODULE_DEVICE_TABLE(of, dummy_connector_of_match);

static struct platform_driver sprd_dummy_connector_driver = {
	.probe = sprd_dummy_connector_probe,
	.remove = sprd_dummy_connector_remove,
	.driver = {
		.name = "sprd-dummy-connector-drv",
		.of_match_table = dummy_connector_of_match,
	},
};
module_platform_driver(sprd_dummy_connector_driver);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Dummy Connector Driver for SPRD SoC");
MODULE_LICENSE("GPL v2");
