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
#include <drm/drm_of.h>
#include <linux/component.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_graph.h>
#include <linux/pm_runtime.h>
#include <video/mipi_display.h>

#include "disp_lib.h"
#include "sprd_dpu.h"
#include "sprd_dsi.h"
#include "dsi/sprd_dsi_api.h"
#include "sysfs/sysfs_display.h"

#define encoder_to_dsi(encoder) \
	container_of(encoder, struct sprd_dsi, encoder)
#define host_to_dsi(host) \
	container_of(host, struct sprd_dsi, host)
#define connector_to_dsi(connector) \
	container_of(connector, struct sprd_dsi, connector)

LIST_HEAD(dsi_core_head);
LIST_HEAD(dsi_glb_head);
static DEFINE_MUTEX(dsi_lock);

static int sprd_dsi_resume(struct sprd_dsi *dsi)
{
	if (dsi->glb && dsi->glb->power)
		dsi->glb->power(&dsi->ctx, true);
	if (dsi->glb && dsi->glb->enable)
		dsi->glb->enable(&dsi->ctx);
	if (dsi->glb && dsi->glb->reset)
		dsi->glb->reset(&dsi->ctx);

	sprd_dsi_init(dsi);

	if (dsi->ctx.work_mode == DSI_MODE_VIDEO)
		sprd_dsi_dpi_video(dsi);
	else
		sprd_dsi_edpi_video(dsi);

	DRM_INFO("dsi resume OK\n");
	return 0;
}

static int sprd_dsi_suspend(struct sprd_dsi *dsi)
{
	sprd_dsi_uninit(dsi);

	if (dsi->glb && dsi->glb->disable)
		dsi->glb->disable(&dsi->ctx);
	if (dsi->glb && dsi->glb->power)
		dsi->glb->power(&dsi->ctx, false);

	DRM_INFO("dsi suspend OK\n");
	return 0;
}

/* FIXME: This should be removed in the feature. */
static void sprd_sharkl3_workaround(struct sprd_dsi *dsi)
{
	/* the sharkl3 AA Chip needs to reset D-PHY before HS transmition */
	if (dsi->phy->ctx.chip_id == 0) {
		sprd_dphy_reset(dsi->phy);
		mdelay(1);
	}
}

static void sprd_dsi_encoder_enable(struct drm_encoder *encoder)
{
	struct sprd_dsi *dsi = encoder_to_dsi(encoder);
	struct sprd_dpu *dpu = crtc_to_dpu(encoder->crtc);
	static bool is_enabled = true;

	DRM_INFO("%s()\n", __func__);

	mutex_lock(&dsi_lock);

	/* add if condition to avoid resume dsi for SR feature.
	 * if esd recovery happened during display suspend, skip dsi resume.
	 */
	if (!encoder->crtc || !encoder->crtc->state->active ||
	    (encoder->crtc->state->mode_changed &&
	     !encoder->crtc->state->active_changed)) {
		DRM_INFO("skip dsi resume\n");
		mutex_unlock(&dsi_lock);
		return;
	}

	if (dsi->ctx.is_inited) {
		mutex_unlock(&dsi_lock);
		DRM_ERROR("dsi is inited\n");
		return;
	}

	if (is_enabled) {
		is_enabled = false;
		dsi->ctx.is_inited = true;
		mutex_unlock(&dsi_lock);
		return;
	}

	pm_runtime_get_sync(dsi->dev.parent);

	sprd_dsi_resume(dsi);
	sprd_dphy_resume(dsi->phy);

	sprd_dsi_lp_cmd_enable(dsi, true);

	if (dsi->panel) {
		drm_panel_prepare(dsi->panel);
		drm_panel_enable(dsi->panel);
	}

	sprd_dsi_set_work_mode(dsi, dsi->ctx.work_mode);
	sprd_dsi_state_reset(dsi);

	sprd_sharkl3_workaround(dsi);

	if (dsi->ctx.nc_clk_en)
		sprd_dsi_nc_clk_en(dsi, true);
	else
		sprd_dphy_hs_clk_en(dsi->phy, true);

	sprd_dpu_run(dpu);

	dsi->ctx.is_inited = true;
	mutex_unlock(&dsi_lock);
}

static void sprd_dsi_encoder_disable(struct drm_encoder *encoder)
{
	struct sprd_dsi *dsi = encoder_to_dsi(encoder);
	struct sprd_dpu *dpu = crtc_to_dpu(encoder->crtc);

	DRM_INFO("%s()\n", __func__);

	/* add if condition to avoid suspend dsi for SR feature */
	if (encoder->crtc->state->mode_changed &&
	    !encoder->crtc->state->active_changed)
		return;

	mutex_lock(&dsi_lock);

	if (!dsi->ctx.is_inited) {
		mutex_unlock(&dsi_lock);
		DRM_ERROR("dsi isn't inited\n");
		return;
	}

	sprd_dpu_stop(dpu);
	sprd_dsi_set_work_mode(dsi, DSI_MODE_CMD);
	sprd_dsi_lp_cmd_enable(dsi, true);

	if (dsi->panel) {
		drm_panel_disable(dsi->panel);
		if (dsi->phy->ctx.ulps_enable)
			sprd_dphy_ulps_enter(dsi->phy);
		drm_panel_unprepare(dsi->panel);
	}

	sprd_dphy_suspend(dsi->phy);
	sprd_dsi_suspend(dsi);

	pm_runtime_put(dsi->dev.parent);

	dsi->ctx.is_inited = false;
	mutex_unlock(&dsi_lock);
}

static void sprd_dsi_encoder_mode_set(struct drm_encoder *encoder,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adj_mode)
{
	struct sprd_dsi *dsi = encoder_to_dsi(encoder);

	DRM_INFO("%s() set mode: %s\n", __func__, dsi->mode->name);
}

static int sprd_dsi_encoder_atomic_check(struct drm_encoder *encoder,
				    struct drm_crtc_state *crtc_state,
				    struct drm_connector_state *conn_state)
{
	DRM_INFO("%s()\n", __func__);

	return 0;
}

static const struct drm_encoder_helper_funcs sprd_encoder_helper_funcs = {
	.atomic_check	= sprd_dsi_encoder_atomic_check,
	.mode_set	= sprd_dsi_encoder_mode_set,
	.enable		= sprd_dsi_encoder_enable,
	.disable	= sprd_dsi_encoder_disable
};

static const struct drm_encoder_funcs sprd_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int sprd_dsi_encoder_init(struct drm_device *drm,
			       struct sprd_dsi *dsi)
{
	struct drm_encoder *encoder = &dsi->encoder;
	struct device *dev = dsi->host.dev;
	u32 crtc_mask;
	int ret;

	crtc_mask = drm_of_find_possible_crtcs(drm, dev->of_node);
	if (!crtc_mask) {
		DRM_ERROR("failed to find crtc mask\n");
		return -EINVAL;
	}
	DRM_INFO("find possible crtcs: 0x%08x\n", crtc_mask);

	encoder->possible_crtcs = crtc_mask;
	ret = drm_encoder_init(drm, encoder, &sprd_encoder_funcs,
			       DRM_MODE_ENCODER_DSI, NULL);
	if (ret) {
		DRM_ERROR("failed to init dsi encoder\n");
		return ret;
	}

	drm_encoder_helper_add(encoder, &sprd_encoder_helper_funcs);

	return 0;
}

static int sprd_dsi_find_panel(struct sprd_dsi *dsi)
{
	struct device *dev = dsi->host.dev;
	struct device_node *child, *lcds_node;
	struct drm_panel *panel;

	/* search /lcds child node first */
	lcds_node = of_find_node_by_path("/lcds");
	for_each_child_of_node(lcds_node, child) {
		panel = of_drm_find_panel(child);
		if (panel) {
			dsi->panel = panel;
			return 0;
		}
	}

	/*
	 * If /lcds child node search failed, we search
	 * the child of dsi host node.
	 */
	for_each_child_of_node(dev->of_node, child) {
		panel = of_drm_find_panel(child);
		if (panel) {
			dsi->panel = panel;
			return 0;
		}
	}

	DRM_ERROR("of_drm_find_panel() failed\n");
	return -ENODEV;
}

static int sprd_dsi_phy_attach(struct sprd_dsi *dsi)
{
	struct device *dev;

	dev = sprd_disp_pipe_get_output(&dsi->dev);
	if (!dev)
		return -ENODEV;

	dsi->phy = dev_get_drvdata(dev);
	if (!dsi->phy) {
		DRM_ERROR("dsi attach phy failed\n");
		return -EINVAL;
	}

	dsi->phy->ctx.lanes = dsi->ctx.lanes;

	return 0;
}

static int sprd_dsi_host_attach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *slave)
{
	struct sprd_dsi *dsi = host_to_dsi(host);
	struct dsi_context *ctx = &dsi->ctx;
	struct device_node *lcd_node;
	u32 val;
	int ret;

	DRM_INFO("%s()\n", __func__);

	dsi->slave = slave;
	ctx->lanes = slave->lanes;
	ctx->format = slave->format;

	if (slave->mode_flags & MIPI_DSI_MODE_VIDEO)
		ctx->work_mode = DSI_MODE_VIDEO;
	else
		ctx->work_mode = DSI_MODE_CMD;

	if (slave->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
		ctx->burst_mode = VIDEO_BURST_WITH_SYNC_PULSES;
	else if (slave->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
		ctx->burst_mode = VIDEO_NON_BURST_WITH_SYNC_PULSES;
	else
		ctx->burst_mode = VIDEO_NON_BURST_WITH_SYNC_EVENTS;

	if (slave->mode_flags & MIPI_DSI_CLOCK_NON_CONTINUOUS)
		ctx->nc_clk_en = true;

	ret = sprd_dsi_phy_attach(dsi);
	if (ret)
		return ret;

	ret = sprd_dsi_find_panel(dsi);
	if (ret)
		return ret;

	lcd_node = dsi->panel->dev->of_node;

	ret = of_property_read_u32(lcd_node, "sprd,phy-bit-clock", &val);
	if (!ret) {
		dsi->phy->ctx.freq = val;
		ctx->byte_clk = val / 8;
	} else {
		dsi->phy->ctx.freq = 500000;
		ctx->byte_clk = 500000 / 8;
	}

	ret = of_property_read_u32(lcd_node, "sprd,phy-escape-clock", &val);
	if (!ret)
		ctx->esc_clk = val > 20000 ? 20000 : val;
	else
		ctx->esc_clk = 20000;

	return 0;
}

static int sprd_dsi_host_detach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *slave)
{
	DRM_INFO("%s()\n", __func__);
	/* do nothing */
	return 0;
}

static ssize_t sprd_dsi_host_transfer(struct mipi_dsi_host *host,
				const struct mipi_dsi_msg *msg)
{
	struct sprd_dsi *dsi = host_to_dsi(host);
	const u8 *tx_buf = msg->tx_buf;

	if (msg->rx_buf && msg->rx_len) {
		u8 lsb = (msg->tx_len > 0) ? tx_buf[0] : 0;
		u8 msb = (msg->tx_len > 1) ? tx_buf[1] : 0;

		return sprd_dsi_rd_pkt(dsi, msg->channel, msg->type,
				msb, lsb, msg->rx_buf, msg->rx_len);
	}

	if (msg->tx_buf && msg->tx_len)
		return sprd_dsi_wr_pkt(dsi, msg->channel, msg->type,
					tx_buf, msg->tx_len);

	return 0;
}

static const struct mipi_dsi_host_ops sprd_dsi_host_ops = {
	.attach = sprd_dsi_host_attach,
	.detach = sprd_dsi_host_detach,
	.transfer = sprd_dsi_host_transfer,
};

static int sprd_dsi_host_init(struct device *dev, struct sprd_dsi *dsi)
{
	int ret;

	dsi->host.dev = dev;
	dsi->host.ops = &sprd_dsi_host_ops;

	ret = mipi_dsi_host_register(&dsi->host);
	if (ret)
		DRM_ERROR("failed to register dsi host\n");

	return ret;
}

static int sprd_dsi_connector_get_modes(struct drm_connector *connector)
{
	struct sprd_dsi *dsi = connector_to_dsi(connector);

	DRM_INFO("%s()\n", __func__);

	return drm_panel_get_modes(dsi->panel);
}

static enum drm_mode_status
sprd_dsi_connector_mode_valid(struct drm_connector *connector,
			 struct drm_display_mode *mode)
{
	struct sprd_dsi *dsi = connector_to_dsi(connector);
	struct drm_display_mode *pmode;

	DRM_INFO("%s() mode: "DRM_MODE_FMT"\n", __func__, DRM_MODE_ARG(mode));

	if (mode->type & DRM_MODE_TYPE_PREFERRED) {
		dsi->mode = mode;
		drm_display_mode_to_videomode(dsi->mode, &dsi->ctx.vm);
	}

	if (mode->type & DRM_MODE_TYPE_BUILTIN) {
		list_for_each_entry(pmode, &connector->modes, head) {
			if (pmode->type & DRM_MODE_TYPE_PREFERRED) {
				list_del(&pmode->head);
				drm_mode_destroy(connector->dev, pmode);
				dsi->mode = mode;
				break;
			}
		}
	}

	return MODE_OK;
}

static struct drm_encoder *
sprd_dsi_connector_best_encoder(struct drm_connector *connector)
{
	struct sprd_dsi *dsi = connector_to_dsi(connector);

	DRM_INFO("%s()\n", __func__);
	return &dsi->encoder;
}

static struct drm_connector_helper_funcs sprd_dsi_connector_helper_funcs = {
	.get_modes = sprd_dsi_connector_get_modes,
	.mode_valid = sprd_dsi_connector_mode_valid,
	.best_encoder = sprd_dsi_connector_best_encoder,
};

static enum drm_connector_status
sprd_dsi_connector_detect(struct drm_connector *connector, bool force)
{
	struct sprd_dsi *dsi = connector_to_dsi(connector);

	DRM_INFO("%s()\n", __func__);

	if (dsi->panel) {
		drm_panel_attach(dsi->panel, connector);
		return connector_status_connected;
	}

	return connector_status_disconnected;
}

static void sprd_dsi_connector_destroy(struct drm_connector *connector)
{
	DRM_INFO("%s()\n", __func__);

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs sprd_dsi_atomic_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = sprd_dsi_connector_detect,
	.destroy = sprd_dsi_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int sprd_dsi_connector_init(struct drm_device *drm, struct sprd_dsi *dsi)
{
	struct drm_encoder *encoder = &dsi->encoder;
	struct drm_connector *connector = &dsi->connector;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_init(drm, connector,
				 &sprd_dsi_atomic_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);
	if (ret) {
		DRM_ERROR("drm_connector_init() failed\n");
		return ret;
	}

	drm_connector_helper_add(connector,
				 &sprd_dsi_connector_helper_funcs);

	drm_mode_connector_attach_encoder(connector, encoder);

	return 0;
}

static int sprd_dsi_bridge_attach(struct sprd_dsi *dsi)
{
	struct drm_encoder *encoder = &dsi->encoder;
	struct drm_bridge *bridge = dsi->bridge;
	struct device *dev = dsi->host.dev;
	struct device_node *bridge_node;
	int ret;

	bridge_node = of_graph_get_remote_node(dev->of_node, 2, 0);
	if (!bridge_node)
		return 0;

	bridge = of_drm_find_bridge(bridge_node);
	if (!bridge) {
		DRM_ERROR("of_drm_find_bridge() failed\n");
		return -ENODEV;
	}
	dsi->bridge = bridge;

	ret = drm_bridge_attach(encoder, bridge, NULL);
	if (ret) {
		DRM_ERROR("failed to attach external bridge\n");
		return ret;
	}

	return 0;
}

static int sprd_dsi_glb_init(struct sprd_dsi *dsi)
{
	if (dsi->glb && dsi->glb->power)
		dsi->glb->power(&dsi->ctx, true);
	if (dsi->glb && dsi->glb->enable)
		dsi->glb->enable(&dsi->ctx);

	return 0;
}

static irqreturn_t sprd_dsi_isr(int irq, void *data)
{
	u32 status = 0;
	struct sprd_dsi *dsi = data;

	if (!dsi) {
		DRM_ERROR("dsi pointer is NULL\n");
		return IRQ_HANDLED;
	}

	if (dsi->ctx.irq0 == irq)
		status = sprd_dsi_int_status(dsi, 0);
	else if (dsi->ctx.irq1 == irq)
		status = sprd_dsi_int_status(dsi, 1);

	if (status & DSI_INT_STS_NEED_SOFT_RESET)
		sprd_dsi_state_reset(dsi);

	return IRQ_HANDLED;
}

static int sprd_dsi_irq_request(struct sprd_dsi *dsi)
{
	int ret;
	int irq0, irq1;
	struct dsi_context *ctx = &dsi->ctx;

	irq0 = irq_of_parse_and_map(dsi->host.dev->of_node, 0);
	if (irq0) {
		DRM_INFO("dsi irq0 num = %d\n", irq0);
		ret = request_irq(irq0, sprd_dsi_isr, 0, "DSI_INT0", dsi);
		if (ret) {
			DRM_ERROR("dsi failed to request irq int0!\n");
			return -EINVAL;
		}
	}
	ctx->irq0 = irq0;

	irq1 = irq_of_parse_and_map(dsi->host.dev->of_node, 1);
	if (irq1) {
		DRM_INFO("dsi irq1 num = %d\n", irq1);
		ret = request_irq(irq1, sprd_dsi_isr, 0, "DSI_INT1", dsi);
		if (ret) {
			DRM_ERROR("dsi failed to request irq int1!\n");
			return -EINVAL;
		}
	}
	ctx->irq1 = irq1;

	return 0;
}

static int sprd_dsi_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct sprd_dsi *dsi = dev_get_drvdata(dev);
	int ret;

	ret = sprd_dsi_encoder_init(drm, dsi);
	if (ret)
		goto cleanup_host;

	ret = sprd_dsi_connector_init(drm, dsi);
	if (ret)
		goto cleanup_encoder;

	ret = sprd_dsi_bridge_attach(dsi);
	if (ret)
		goto cleanup_connector;

	ret = sprd_dsi_glb_init(dsi);
	if (ret)
		goto cleanup_connector;

	ret = sprd_dsi_irq_request(dsi);
	if (ret)
		goto cleanup_connector;

	return 0;

cleanup_connector:
	drm_connector_cleanup(&dsi->connector);
cleanup_encoder:
	drm_encoder_cleanup(&dsi->encoder);
cleanup_host:
	mipi_dsi_host_unregister(&dsi->host);
	return ret;
}

static void sprd_dsi_unbind(struct device *dev,
			struct device *master, void *data)
{
	/* do nothing */
	DRM_INFO("%s()\n", __func__);

}

static const struct component_ops dsi_component_ops = {
	.bind	= sprd_dsi_bind,
	.unbind	= sprd_dsi_unbind,
};

static int sprd_dsi_device_create(struct sprd_dsi *dsi,
				struct device *parent)
{
	int ret;

	dsi->dev.class = display_class;
	dsi->dev.parent = parent;
	dsi->dev.of_node = parent->of_node;
	dev_set_name(&dsi->dev, "dsi");
	dev_set_drvdata(&dsi->dev, dsi);

	ret = device_register(&dsi->dev);
	if (ret)
		DRM_ERROR("dsi device register failed\n");

	return ret;
}

static int sprd_dsi_context_init(struct sprd_dsi *dsi, struct device_node *np)
{
	struct dsi_context *ctx = &dsi->ctx;
	struct resource r;
	u32 tmp;

	if (dsi->glb && dsi->glb->parse_dt)
		dsi->glb->parse_dt(&dsi->ctx, np);

	if (of_address_to_resource(np, 0, &r)) {
		DRM_ERROR("parse dsi ctrl reg base failed\n");
		return -ENODEV;
	}
	ctx->base = (unsigned long)
	    ioremap_nocache(r.start, resource_size(&r));
	if (ctx->base == 0) {
		DRM_ERROR("dsi ctrl reg base ioremap failed\n");
		return -ENODEV;
	}

	if (!of_property_read_u32(np, "dev-id", &tmp))
		ctx->id = tmp;

	if (!of_property_read_u32(np, "sprd,data-hs2lp", &tmp))
		ctx->data_hs2lp = tmp;
	else
		ctx->data_hs2lp = 120;

	if (!of_property_read_u32(np, "sprd,data-lp2hs", &tmp))
		ctx->data_lp2hs = tmp;
	else
		ctx->data_lp2hs = 500;

	if (!of_property_read_u32(np, "sprd,clk-hs2lp", &tmp))
		ctx->clk_hs2lp = tmp;
	else
		ctx->clk_hs2lp = 4;

	if (!of_property_read_u32(np, "sprd,clk-lp2hs", &tmp))
		ctx->clk_lp2hs = tmp;
	else
		ctx->clk_lp2hs = 15;

	if (!of_property_read_u32(np, "sprd,max-read-time", &tmp))
		ctx->max_rd_time = tmp;
	else
		ctx->max_rd_time = 6000;

	if (!of_property_read_u32(np, "sprd,int0_mask", &tmp))
		ctx->int0_mask = tmp;
	else
		ctx->int0_mask = 0xffffffff;

	if (!of_property_read_u32(np, "sprd,int1_mask", &tmp))
		ctx->int1_mask = tmp;
	else
		ctx->int1_mask = 0xffffffff;

	return 0;
}

static int sprd_dsi_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sprd_dsi *dsi;
	const char *str;
	int ret;

	if (calibration_mode) {
		DRM_WARN("Calibration Mode! Don't register sprd dsi driver\n");
		return -ENODEV;
	}

	dsi = devm_kzalloc(&pdev->dev, sizeof(*dsi), GFP_KERNEL);
	if (!dsi) {
		DRM_ERROR("failed to allocate dsi data.\n");
		return -ENOMEM;
	}

	if (!of_property_read_string(np, "sprd,ip", &str))
		dsi->core = dsi_core_ops_attach(str);
	else
		DRM_WARN("error: 'sprd,ip' was not found\n");

	if (!of_property_read_string(np, "sprd,soc", &str))
		dsi->glb = dsi_glb_ops_attach(str);
	else
		DRM_WARN("error: 'sprd,soc' was not found\n");

	ret = sprd_dsi_context_init(dsi, np);
	if (ret)
		return -EINVAL;

	sprd_dsi_device_create(dsi, &pdev->dev);
	sprd_dsi_sysfs_init(&dsi->dev);
	platform_set_drvdata(pdev, dsi);

	ret = sprd_dsi_host_init(&pdev->dev, dsi);
	if (ret)
		return ret;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return component_add(&pdev->dev, &dsi_component_ops);
}

static int sprd_dsi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dsi_component_ops);

	return 0;
}

static const struct of_device_id sprd_dsi_of_match[] = {
	{.compatible = "sprd,dsi-host"},
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_dsi_of_match);

static struct platform_driver sprd_dsi_driver = {
	.probe = sprd_dsi_probe,
	.remove = sprd_dsi_remove,
	.driver = {
		.name = "sprd-dsi-drv",
		.of_match_table = sprd_dsi_of_match,
	},
};

module_platform_driver(sprd_dsi_driver);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("SPRD MIPI DSI HOST Controller Driver");
MODULE_LICENSE("GPL v2");
