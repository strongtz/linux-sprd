#include <linux/sipa.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/slab.h>

#ifdef CONFIG_SIPA_TEST
#include <linux/kthread.h>
#endif

#include "sipa_hal.h"
#include "sipa_hal_priv.h"
#include "sipa_priv.h"

struct sipa_hal_context sipa_hal_ctx;

static int alloc_tx_fifo_ram(struct device *dev,
			     struct sipa_hal_context *cfg,
			     enum sipa_cmn_fifo_index index)
{
	ssize_t size, node_size = sizeof(struct sipa_node_description_tag);
	dma_addr_t phy_addr;
	struct sipa_common_fifo_cfg_tag *fifo_cfg = &cfg->cmn_fifo_cfg[index];

	if (!fifo_cfg->tx_fifo.depth)
		return 0;
	size = fifo_cfg->tx_fifo.depth * node_size;
	if (fifo_cfg->tx_fifo.in_iram) {
		if (cfg->phy_virt_res.iram_allocated_size >=
			cfg->phy_virt_res.iram_size)
			return -ENOMEM;

		fifo_cfg->tx_fifo.virtual_addr =
			cfg->phy_virt_res.iram_base +
			cfg->phy_virt_res.iram_allocated_size;

		phy_addr = cfg->phy_virt_res.iram_phy +
			cfg->phy_virt_res.iram_allocated_size;

		cfg->phy_virt_res.iram_allocated_size += size;
	} else if (fifo_cfg->is_pam) {
		fifo_cfg->tx_fifo.virtual_addr =
			dma_alloc_coherent(dev, size, &phy_addr, GFP_KERNEL);
		if (!fifo_cfg->tx_fifo.virtual_addr)
			return -ENOMEM;
	} else {
		fifo_cfg->tx_fifo.virtual_addr =
			(void *)__get_free_pages(GFP_KERNEL, get_order(size));
		if (!fifo_cfg->tx_fifo.virtual_addr)
			return -ENOMEM;
		memset(fifo_cfg->tx_fifo.virtual_addr, 0, size);
		phy_addr = dma_map_single(dev, fifo_cfg->tx_fifo.virtual_addr,
					  size, DMA_FROM_DEVICE);
		if (dma_mapping_error(dev, phy_addr)) {
			free_pages((unsigned long)
				   fifo_cfg->tx_fifo.virtual_addr,
				   get_order(fifo_cfg->tx_fifo.size));
			fifo_cfg->rx_fifo.virtual_addr = NULL;
			return -ENOMEM;
		}
	}
	fifo_cfg->tx_fifo.size = size;
	fifo_cfg->tx_fifo.fifo_base_addr_l = IPA_GET_LOW32(phy_addr);
	fifo_cfg->tx_fifo.fifo_base_addr_h = IPA_GET_HIGH32(phy_addr);

	return 0;
}

static int alloc_rx_fifo_ram(struct device *dev,
			     struct sipa_hal_context *cfg,
			     enum sipa_cmn_fifo_index index)
{
	dma_addr_t phy_addr;
	ssize_t size, node_size = sizeof(struct sipa_node_description_tag);
	struct sipa_common_fifo_cfg_tag *fifo_cfg = &cfg->cmn_fifo_cfg[index];

	if (!fifo_cfg->rx_fifo.depth)
		return 0;
	size = fifo_cfg->rx_fifo.depth * node_size;
	if (fifo_cfg->rx_fifo.in_iram) {
		if (cfg->phy_virt_res.iram_allocated_size >=
			cfg->phy_virt_res.iram_size) {
			dev_err(dev, "fifo id = %d don't have iram\n", index);
			return -ENOMEM;
		}

		fifo_cfg->rx_fifo.virtual_addr =
			cfg->phy_virt_res.iram_base +
			cfg->phy_virt_res.iram_allocated_size;

		phy_addr = cfg->phy_virt_res.iram_phy +
			cfg->phy_virt_res.iram_allocated_size;

		cfg->phy_virt_res.iram_allocated_size += size;
	} else if (fifo_cfg->is_pam) {
		fifo_cfg->rx_fifo.virtual_addr =
			dma_alloc_coherent(dev, size, &phy_addr, GFP_KERNEL);
		if (!fifo_cfg->rx_fifo.virtual_addr)
			return -ENOMEM;
	} else {
		fifo_cfg->rx_fifo.virtual_addr =
			(void *)__get_free_pages(GFP_KERNEL, get_order(size));
		if (!fifo_cfg->rx_fifo.virtual_addr)
			return -ENOMEM;
		memset(fifo_cfg->rx_fifo.virtual_addr, 0, size);
		phy_addr = dma_map_single(dev, fifo_cfg->rx_fifo.virtual_addr,
					  size, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, phy_addr)) {
			free_pages((unsigned long)
				   fifo_cfg->rx_fifo.virtual_addr,
				   get_order(fifo_cfg->rx_fifo.size));
			fifo_cfg->rx_fifo.virtual_addr = NULL;
			return -ENOMEM;
		}
	}
	fifo_cfg->rx_fifo.size = size;
	fifo_cfg->rx_fifo.fifo_base_addr_l = IPA_GET_LOW32(phy_addr);
	fifo_cfg->rx_fifo.fifo_base_addr_h = IPA_GET_HIGH32(phy_addr);

	return 0;
}

static void free_tx_fifo_ram(struct device *dev,
			     struct sipa_hal_context *cfg,
			     enum sipa_cmn_fifo_index index)
{
	dma_addr_t phy_addr;
	struct sipa_common_fifo_cfg_tag *fifo_cfg =
				&cfg->cmn_fifo_cfg[index];

	if (!fifo_cfg->tx_fifo.virtual_addr)
		return;
	phy_addr = IPA_STI_64BIT(fifo_cfg->tx_fifo.fifo_base_addr_l,
				 fifo_cfg->tx_fifo.fifo_base_addr_h);
	if (!fifo_cfg->tx_fifo.in_iram && !fifo_cfg->is_pam &&
	    fifo_cfg->tx_fifo.virtual_addr) {
		dma_unmap_single(dev, phy_addr, fifo_cfg->tx_fifo.size,
				 DMA_FROM_DEVICE);
		free_pages((unsigned long)fifo_cfg->tx_fifo.virtual_addr,
			   get_order(fifo_cfg->tx_fifo.size));
	} else if (fifo_cfg->is_pam) {
		dma_free_coherent(dev, fifo_cfg->tx_fifo.size,
				  fifo_cfg->tx_fifo.virtual_addr,
				  phy_addr);
	}
}

static void free_rx_fifo_ram(struct device *dev,
			     struct sipa_hal_context *cfg,
			     enum sipa_cmn_fifo_index index)
{
	dma_addr_t phy_addr;
	struct sipa_common_fifo_cfg_tag *fifo_cfg =
				&cfg->cmn_fifo_cfg[index];

	if (!fifo_cfg->rx_fifo.virtual_addr)
		return;
	phy_addr = IPA_STI_64BIT(fifo_cfg->rx_fifo.fifo_base_addr_l,
				 fifo_cfg->rx_fifo.fifo_base_addr_h);
	if (!fifo_cfg->rx_fifo.in_iram && !fifo_cfg->is_pam &&
	    fifo_cfg->rx_fifo.virtual_addr) {
		dma_unmap_single(dev, phy_addr, fifo_cfg->rx_fifo.size,
				 DMA_TO_DEVICE);
		free_pages((unsigned long)fifo_cfg->rx_fifo.virtual_addr,
			   get_order(fifo_cfg->rx_fifo.size));
	} else if (fifo_cfg->is_pam) {
		dma_free_coherent(dev, fifo_cfg->rx_fifo.size,
				  fifo_cfg->rx_fifo.virtual_addr,
				  phy_addr);
	}
}

static int sipa_init_fifo_addr(struct device *dev, struct sipa_hal_context *cfg)
{
	int i, ret;

	for (i = 0; i < SIPA_FIFO_MAX; i++) {
		ret = alloc_rx_fifo_ram(dev, cfg, i);
		if (ret)
			return -1;
		ret = alloc_tx_fifo_ram(dev, cfg, i);
		if (ret)
			return -1;
	}

	return 0;
}

static u32 sipa_init_fifo_reg_base(struct sipa_hal_context *cfg)
{
	int i;

	for (i = 0; i < SIPA_FIFO_MAX; i++) {
		cfg->cmn_fifo_cfg[i].fifo_reg_base =
			cfg->phy_virt_res.glb_base +
			((i + 1) * SIPA_FIFO_REG_SIZE);
		cfg->cmn_fifo_cfg[i].fifo_phy_addr =
			cfg->phy_virt_res.glb_phy +
			((i + 1) * SIPA_FIFO_REG_SIZE);
	}

	return 0;
}

static void siap_init_hal_cfg(struct sipa_plat_drv_cfg *cfg)
{
	u32 i;
	struct sipa_hal_context *hal_cfg = &sipa_hal_ctx;

	hal_cfg->ipa_intr = cfg->ipa_intr;

	for (i = 0; i < SIPA_FIFO_MAX; i++) {
		hal_cfg->cmn_fifo_cfg[i].cur = cfg->common_fifo_cfg[i].src;
		hal_cfg->cmn_fifo_cfg[i].dst = cfg->common_fifo_cfg[i].dst;
		hal_cfg->cmn_fifo_cfg[i].is_recv =
			cfg->common_fifo_cfg[i].is_recv;
		hal_cfg->cmn_fifo_cfg[i].tx_fifo.depth =
			cfg->common_fifo_cfg[i].tx_fifo.fifo_size;
		hal_cfg->cmn_fifo_cfg[i].tx_fifo.in_iram =
			cfg->common_fifo_cfg[i].tx_fifo.in_iram;
		hal_cfg->cmn_fifo_cfg[i].rx_fifo.depth =
			cfg->common_fifo_cfg[i].rx_fifo.fifo_size;
		hal_cfg->cmn_fifo_cfg[i].rx_fifo.in_iram =
			cfg->common_fifo_cfg[i].rx_fifo.in_iram;
		hal_cfg->cmn_fifo_cfg[i].is_pam =
			cfg->common_fifo_cfg[i].is_pam;
		hal_cfg->cmn_fifo_cfg[i].fifo_id = i;
	}
}

static void sipa_backup_open_fifo_params(sipa_hal_hdl hdl,
					 enum sipa_cmn_fifo_index id,
					 struct sipa_comm_fifo_params *attr,
					 struct sipa_ext_fifo_params *ext_attr,
					 bool force_sw_intr,
					 sipa_hal_notify_cb cb,
					 void *priv)
{
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_open_fifo_param *fifo_param = &hal_cfg->fifo_param[id];

	if (attr) {
		if (!fifo_param->attr) {
			fifo_param->attr = kzalloc(sizeof(*attr), GFP_KERNEL);
			if (!fifo_param->attr)
				return;
		}
		memcpy(fifo_param->attr, attr, sizeof(*attr));
	}

	if (ext_attr) {
		if (!fifo_param->ext_attr) {
			fifo_param->ext_attr = kzalloc(sizeof(*ext_attr),
						       GFP_KERNEL);
			if (!fifo_param->ext_attr)
				return;
		}
		memcpy(fifo_param->ext_attr, ext_attr, sizeof(*ext_attr));
	}

	fifo_param->force_sw_intr = force_sw_intr;
	fifo_param->cb = cb;
	fifo_param->priv = priv;
	fifo_param->open_flag = true;
}

static void sipa_remove_fifo_params(sipa_hal_hdl hdl,
				    enum sipa_cmn_fifo_index id)
{
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_open_fifo_param *fifo_param = &hal_cfg->fifo_param[id];

	kfree(fifo_param->attr);
	kfree(fifo_param->ext_attr);
	fifo_param->attr = NULL;
	fifo_param->ext_attr = NULL;

	fifo_param->open_flag = false;
}

sipa_hal_hdl sipa_hal_init(struct device *dev,
			   struct sipa_plat_drv_cfg *cfg)
{
	int ret;
	struct sipa_hal_context *hal_cfg = &sipa_hal_ctx;

	hal_cfg->dev = dev;

	sipa_glb_ops_init(&hal_cfg->glb_ops);
	sipa_fifo_ops_init(&hal_cfg->fifo_ops);

	siap_init_hal_cfg(cfg);

	ret = request_irq(hal_cfg->ipa_intr, sipa_int_callback_func,
			  IRQF_NO_SUSPEND, "sprd,sipa", hal_cfg);
	if (ret)
		dev_err(dev, "request irq err ret = %d\n", ret);

	hal_cfg->phy_virt_res.glb_phy = cfg->glb_phy;
	hal_cfg->phy_virt_res.glb_size = cfg->glb_size;
	hal_cfg->phy_virt_res.glb_base =
		devm_ioremap_nocache(dev, cfg->glb_phy, cfg->glb_size);

	if (!hal_cfg->phy_virt_res.glb_base) {
		dev_err(dev, "remap glb_base fail\n");
		return NULL;
	}

	hal_cfg->phy_virt_res.iram_phy = cfg->iram_phy;
	hal_cfg->phy_virt_res.iram_size = cfg->iram_size;
	hal_cfg->phy_virt_res.iram_base =
		memremap((resource_size_t)cfg->iram_phy,
			 (size_t)cfg->iram_size, MEMREMAP_WT);
	if (!hal_cfg->phy_virt_res.iram_base) {
		dev_err(dev, "remap iram_base fail\n");
		return NULL;
	}

	ret = sipa_init_fifo_addr(dev, hal_cfg);
	if (ret)
		dev_err(dev, "init fifo addr err ret = %d\n", ret);

	sipa_init_fifo_reg_base(hal_cfg);

	return ((sipa_hal_hdl)hal_cfg);
}
EXPORT_SYMBOL(sipa_hal_init);

int sipa_hal_set_enabled(struct sipa_plat_drv_cfg *cfg, bool enable)
{
	int ret = 0;
	u32 val = enable ? cfg->enable_mask : (~cfg->enable_mask);
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (cfg->sys_regmap) {
		ret = regmap_update_bits(cfg->sys_regmap,
					 cfg->enable_reg,
					 cfg->enable_mask,
					 val);
		if (ret < 0)
			dev_err(ctrl->ctx->pdev,
				"regmap update bits failed");
	}
	return ret;
}
EXPORT_SYMBOL(sipa_hal_set_enabled);

int sipa_force_wakeup(struct sipa_plat_drv_cfg *cfg, bool wake)
{
	int ret = 0;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!cfg->vpower)
		return ret;

	if (wake)
		ret = regulator_enable(cfg->vpower);
	else
		ret = regulator_disable(cfg->vpower);

	if (ret < 0)
		dev_err(ctrl->ctx->pdev, "enable vpower failed");

	return ret;
}
EXPORT_SYMBOL(sipa_force_wakeup);


int sipa_open_common_fifo(sipa_hal_hdl hdl,
			  enum sipa_cmn_fifo_index fifo,
			  struct sipa_comm_fifo_params *attr,
			  struct sipa_ext_fifo_params *ext_attr,
			  bool force_sw_intr,
			  sipa_hal_notify_cb cb,
			  void *priv)
{
	int ret;
	struct sipa_hal_context *hal_cfg;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();
	ssize_t node_size = sizeof(struct sipa_node_description_tag);

	if (unlikely(!hdl)) {
		dev_err(ctrl->ctx->pdev, "hdl is null\n");
		return -1;
	}
	hal_cfg = (struct sipa_hal_context *)hdl;
	fifo_cfg = hal_cfg->cmn_fifo_cfg;

	fifo_cfg[fifo].priv = priv;
	fifo_cfg[fifo].fifo_irq_callback = cb;

	dev_info(hal_cfg->dev, "fifo_id = %d is_pam = %d is_recv = %d\n",
		 fifo_cfg[fifo].fifo_id,
		 fifo_cfg[fifo].is_pam,
		 fifo_cfg[fifo].is_recv);

	sipa_backup_open_fifo_params(hdl, fifo, attr, ext_attr, force_sw_intr,
				     cb, priv);

	if (!fifo_cfg[fifo].is_pam || (fifo_cfg[fifo].tx_fifo.in_iram &&
				       fifo_cfg[fifo].rx_fifo.in_iram)) {
		if (!kfifo_initialized(&fifo_cfg[fifo].tx_priv_fifo) &&
		    !kfifo_initialized(&fifo_cfg[fifo].rx_priv_fifo)) {
			ret = kfifo_alloc(&fifo_cfg[fifo].tx_priv_fifo,
					  fifo_cfg[fifo].tx_fifo.depth *
					  node_size, GFP_KERNEL);
			if (ret)
				return -ENOMEM;

			ret = kfifo_alloc(&fifo_cfg[fifo].rx_priv_fifo,
					  fifo_cfg[fifo].rx_fifo.depth *
					  node_size, GFP_KERNEL);
			if (ret) {
				kfifo_free(&fifo_cfg[fifo].tx_priv_fifo);
				return -ENOMEM;
			}
		}
	}

	if (ext_attr) {
		fifo_cfg[fifo].rx_fifo.depth = ext_attr->rx_depth;
		fifo_cfg[fifo].rx_fifo.fifo_base_addr_l = ext_attr->rx_fifo_pal;
		fifo_cfg[fifo].rx_fifo.fifo_base_addr_h = ext_attr->rx_fifo_pah;
		fifo_cfg[fifo].rx_fifo.virtual_addr = ext_attr->rx_fifo_va;

		fifo_cfg[fifo].tx_fifo.depth = ext_attr->tx_depth;
		fifo_cfg[fifo].tx_fifo.fifo_base_addr_l = ext_attr->tx_fifo_pal;
		fifo_cfg[fifo].tx_fifo.fifo_base_addr_h = ext_attr->tx_fifo_pah;
		fifo_cfg[fifo].tx_fifo.virtual_addr = ext_attr->tx_fifo_va;
	}

	if (!ctrl->params_cfg.enable_cnt)
		return 0;

	hal_cfg->fifo_ops.open(fifo, fifo_cfg, NULL);
	if (!force_sw_intr && fifo_cfg[fifo].is_pam) {
		hal_cfg->fifo_ops.set_hw_interrupt_threshold(fifo, fifo_cfg, 1,
							attr->tx_intr_threshold,
							NULL);
		hal_cfg->fifo_ops.set_hw_interrupt_timeout(fifo, fifo_cfg, 1,
							attr->tx_intr_delay_us,
							NULL);
	} else {
		if (attr->tx_intr_threshold)
			hal_cfg->fifo_ops.set_interrupt_threshold(fifo,
					     fifo_cfg, 1,
					     attr->tx_intr_threshold,
					     NULL);
		if (attr->tx_intr_delay_us)
			hal_cfg->fifo_ops.set_interrupt_timeout(fifo, fifo_cfg,
					1, attr->tx_intr_delay_us, NULL);
	}

	if (fifo_cfg[fifo].is_recv)
		hal_cfg->fifo_ops.enable_remote_flowctrl_interrupt(
			fifo, fifo_cfg, attr->flow_ctrl_cfg,
			attr->tx_enter_flowctrl_watermark,
			attr->tx_leave_flowctrl_watermark,
			attr->rx_enter_flowctrl_watermark,
			attr->rx_leave_flowctrl_watermark);
	else
		hal_cfg->fifo_ops.enable_local_flowctrl_interrupt(fifo,
				fifo_cfg, 1, attr->flow_ctrl_irq_mode, NULL);

	if (attr->flowctrl_in_tx_full)
		hal_cfg->fifo_ops.set_interrupt_txfifo_full(fifo, fifo_cfg,
							    1, NULL);
	else
		hal_cfg->fifo_ops.set_interrupt_txfifo_full(fifo, fifo_cfg,
							    0, NULL);

	return 0;
}
EXPORT_SYMBOL(sipa_open_common_fifo);

int sipa_close_common_fifo(sipa_hal_hdl hdl,
			   enum sipa_cmn_fifo_index fifo)
{
	struct sipa_hal_context *hal_cfg;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (unlikely(!hdl)) {
		dev_err(ctrl->ctx->pdev, "hdl is null\n");
		return -EINVAL;
	}

	hal_cfg = (struct sipa_hal_context *)hdl;
	fifo_cfg = hal_cfg->cmn_fifo_cfg;

	hal_cfg->fifo_ops.close(fifo, fifo_cfg);
	sipa_remove_fifo_params(hdl, fifo);

	return 0;
}
EXPORT_SYMBOL(sipa_close_common_fifo);

static int sipa_resume_cmn_fifo_ptr(sipa_hal_hdl hdl,
				    enum sipa_cmn_fifo_index id,
				    struct sipa_plat_drv_cfg *cfg)
{
	u32 depth;
	struct sipa_hal_context *hal_cfg = hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg = hal_cfg->cmn_fifo_cfg;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	switch (id) {
	case SIPA_FIFO_CP_UL:
		if (!cfg->wiap_ul_dma)
			return 0;
		depth = fifo_cfg[id].rx_fifo.depth;
		hal_cfg->fifo_ops.set_rx_fifo_wptr(id, fifo_cfg, depth);
		break;
	case SIPA_FIFO_CP_DL:
		depth = fifo_cfg[id].tx_fifo.depth;
		hal_cfg->fifo_ops.set_tx_fifo_wptr(id, fifo_cfg, depth);
		break;
	case SIPA_FIFO_AP_IP_DL:
		depth = fifo_cfg[id].rx_fifo.depth;
		if (sipa_hal_check_rx_priv_fifo_is_full(&sipa_hal_ctx, id))
			sipa_fill_free_node(ctrl->receiver[SIPA_PKT_IP],
					    depth);
		hal_cfg->fifo_ops.set_rx_fifo_wptr(id, fifo_cfg, depth);
		break;
	case SIPA_FIFO_AP_ETH_UL:
		depth = fifo_cfg[id].rx_fifo.depth;
		if (sipa_hal_check_rx_priv_fifo_is_full(&sipa_hal_ctx, id))
			sipa_fill_free_node(ctrl->receiver[SIPA_PKT_ETH],
					    depth);
		hal_cfg->fifo_ops.set_rx_fifo_wptr(id, fifo_cfg, depth);
		break;
	default:
		dev_info(ctrl->ctx->pdev,
			 "this fifo %d do not need to update ptr", id);
		break;
	}

	return 0;
}

int sipa_resume_common_fifo(sipa_hal_hdl hdl, struct sipa_plat_drv_cfg *cfg)
{
	int i;
	struct sipa_open_fifo_param *iter;
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;

	for (i = 0; i < SIPA_FIFO_MAX; i++) {
		iter = &hal_cfg->fifo_param[i];

		if (!iter->open_flag)
			continue;

		if (hal_cfg->fifo_ops.get_tx_depth(i, hal_cfg->cmn_fifo_cfg))
			continue;

		hal_cfg->cmn_fifo_cfg[i].state = false;
		sipa_open_common_fifo(hdl, i, iter->attr,
				      iter->ext_attr, iter->force_sw_intr,
				      iter->cb, iter->priv);
		sipa_resume_cmn_fifo_ptr(hdl, i, cfg);
	}

	return 0;
}
EXPORT_SYMBOL(sipa_resume_common_fifo);

bool sipa_hal_cmn_fifo_open_status(sipa_hal_hdl hdl,
				   enum sipa_cmn_fifo_index fifo)
{
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg =
		&hal_cfg->cmn_fifo_cfg[fifo];

	return fifo_cfg->state;
}
EXPORT_SYMBOL(sipa_hal_cmn_fifo_open_status);

int sipa_tft_mode_init(sipa_hal_hdl hdl)
{
	int ret;
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg = hal_cfg->cmn_fifo_cfg;
	void __iomem *glb_addr = hal_cfg->phy_virt_res.glb_base;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (unlikely(!hdl)) {
		dev_err(ctrl->ctx->pdev, "hdl is null\n");
		return -EINVAL;
	}

	ret = hal_cfg->fifo_ops.set_cur_dst_term(SIPA_FIFO_PCIE_UL,
						 fifo_cfg, SIPA_TERM_PCIE0,
						 SIPA_TERM_VCP);
	if (unlikely(!ret))
		dev_warn(hal_cfg->dev, "set pcie ul dst/cur id failed\n");

	ret = hal_cfg->fifo_ops.set_cur_dst_term(SIPA_FIFO_PCIE_DL,
						 fifo_cfg, SIPA_TERM_PCIE0,
						 SIPA_TERM_VCP);
	if (unlikely(!ret))
		dev_warn(hal_cfg->dev, "set pcie dl dst/cur id failed\n");

	ret = hal_cfg->glb_ops.set_cp_ul_cur_num(glb_addr, SIPA_TERM_VCP);
	if (unlikely(!ret))
		dev_warn(hal_cfg->dev, "set cp ul cur id failed\n");

	ret = hal_cfg->glb_ops.set_cp_ul_dst_num(glb_addr, SIPA_TERM_PCIE0);
	if (unlikely(!ret))
		dev_warn(hal_cfg->dev, "set cp ul dst id failed\n");

	ret = hal_cfg->glb_ops.set_cp_dl_cur_num(glb_addr, SIPA_TERM_VCP);
	if (unlikely(!ret))
		dev_warn(hal_cfg->dev, "set cp dl cur id failed\n");

	ret = hal_cfg->glb_ops.set_cp_dl_dst_num(glb_addr, SIPA_TERM_PCIE0);
	if (unlikely(!ret))
		dev_warn(hal_cfg->dev, "set cp dl dst id failed\n");

	ret = hal_cfg->glb_ops.enable_from_pcie_no_mac(glb_addr, 1);
	if (unlikely(!ret))
		dev_warn(hal_cfg->dev, "enable from pcie no mac failed\n");

	ret = hal_cfg->glb_ops.enable_to_pcie_no_mac(glb_addr, 1);
	if (unlikely(!ret))
		dev_warn(hal_cfg->dev, "enable to pcie no mac failed\n");

	return 0;
}
EXPORT_SYMBOL(sipa_tft_mode_init);

/*
 * stop : true : stop recv false : start receive
 */
int sipa_hal_cmn_fifo_set_receive(sipa_hal_hdl hdl,
				  enum sipa_cmn_fifo_index fifo_id,
				  bool stop)
{
	u32 ret;
	struct sipa_hal_context *hal_cfg;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;

	hal_cfg = (struct sipa_hal_context *)hdl;
	fifo_cfg = hal_cfg->cmn_fifo_cfg;

	ret = hal_cfg->fifo_ops.ctrl_receive(fifo_id, fifo_cfg, stop);

	if (ret)
		return 0;
	else
		return -1;
}
EXPORT_SYMBOL(sipa_hal_cmn_fifo_set_receive);

int sipa_hal_init_set_tx_fifo(sipa_hal_hdl hdl,
			      enum sipa_cmn_fifo_index fifo_id,
			      struct sipa_hal_fifo_item *items,
			      u32 num)
{
	u32 ret, i;
	struct sipa_hal_context *hal_cfg;
	struct sipa_node_description_tag node;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;

	hal_cfg = (struct sipa_hal_context *)hdl;
	fifo_cfg = hal_cfg->cmn_fifo_cfg;

	memset(&node, 0, sizeof(node));
	for (i = 0; i < num; i++) {
		node.address = (items + i)->addr;
		node.length = (items + i)->len;
		node.dst = (items + i)->dst;
		node.offset = (items + i)->offset;
		ret = hal_cfg->fifo_ops.put_node_to_tx_fifo(hal_cfg->dev,
							    fifo_id, fifo_cfg,
							    &node, 0, 1);
		if (ret == 0) {
			dev_err(hal_cfg->dev, "put node to tx fifo %d fail\n",
				fifo_id);
			return -1;
		}
	}

	return 0;
}
EXPORT_SYMBOL(sipa_hal_init_set_tx_fifo);

int sipa_hal_get_tx_fifo_item(sipa_hal_hdl hdl,
			      enum sipa_cmn_fifo_index fifo_id,
			      struct sipa_hal_fifo_item *item)
{
	u32 ret;
	struct sipa_hal_context *hal_cfg;
	struct sipa_node_description_tag node;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;

	hal_cfg = (struct sipa_hal_context *)hdl;
	fifo_cfg = hal_cfg->cmn_fifo_cfg;

	ret = hal_cfg->fifo_ops.recv_node_from_tx_fifo(hal_cfg->dev,
						       fifo_id, fifo_cfg,
						       &node, 0, 1);

	if (ret == 0) {
		dev_err(hal_cfg->dev, "get node from tx fifo %d fail\n",
			fifo_id);
		return -1;
	}

	item->addr = node.address;
	item->len = node.length;
	item->dst = node.dst;
	item->offset = node.offset;
	item->src = node.src;
	item->err_code = node.err_code;
	item->netid = node.net_id;
	item->intr = node.intr;

	return 0;
}
EXPORT_SYMBOL(sipa_hal_get_tx_fifo_item);

u32 sipa_hal_get_tx_fifo_items(sipa_hal_hdl hdl,
			       enum sipa_cmn_fifo_index fifo_id)
{
	u32 num;
	struct sipa_node_description_tag node;
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg = hal_cfg->cmn_fifo_cfg;

	num = hal_cfg->fifo_ops.recv_node_from_tx_fifo(hal_cfg->dev,
						       fifo_id, fifo_cfg,
						       &node, 0, -1);
	if (!num)
		dev_warn(hal_cfg->dev,
			 "fifo id %d tx fifo don't have node\n", fifo_id);

	return num;
}
EXPORT_SYMBOL(sipa_hal_get_tx_fifo_items);

int sipa_hal_recv_conversion_node_to_item(sipa_hal_hdl hdl,
					  enum sipa_cmn_fifo_index fifo_id,
					  struct sipa_hal_fifo_item *item,
					  u32 index)
{
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg = hal_cfg->cmn_fifo_cfg;
	struct sipa_node_description_tag *node =
		hal_cfg->fifo_ops.get_tx_fifo_node(fifo_id, fifo_cfg, index);

	if (unlikely(!node))
		return -EINVAL;

	item->addr = node->address;
	item->len = node->length;
	item->dst = node->dst;
	item->offset = node->offset;
	item->src = node->src;
	item->err_code = node->err_code;
	item->netid = node->net_id;
	item->intr = node->intr;

	return 0;
}
EXPORT_SYMBOL(sipa_hal_recv_conversion_node_to_item);

int sipa_hal_conversion_node_to_item(sipa_hal_hdl hdl,
				     enum sipa_cmn_fifo_index fifo_id,
				     struct sipa_hal_fifo_item *item)
{
	struct sipa_node_description_tag node;
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg = hal_cfg->cmn_fifo_cfg;

	if (!kfifo_out(&((fifo_cfg + fifo_id)->tx_priv_fifo),
		       &node, sizeof(node)))
		return 0;

	item->addr = node.address;
	item->len = node.length;
	item->dst = node.dst;
	item->offset = node.offset;
	item->src = node.src;
	item->err_code = node.err_code;
	item->netid = node.net_id;
	item->intr = node.intr;

	return 0;
}
EXPORT_SYMBOL(sipa_hal_conversion_node_to_item);

int sipa_hal_set_tx_fifo_rptr(sipa_hal_hdl hdl, enum sipa_cmn_fifo_index id,
			      u32 num)
{
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg = hal_cfg->cmn_fifo_cfg;

	return hal_cfg->fifo_ops.update_tx_fifo_rptr(id, fifo_cfg, num);
}
EXPORT_SYMBOL(sipa_hal_set_tx_fifo_rptr);

int sipa_hal_get_cmn_fifo_filled_depth(sipa_hal_hdl hdl,
				       enum sipa_cmn_fifo_index fifo_id,
				       u32 *rx_filled, u32 *tx_filled)
{
	u32 ret;
	struct sipa_hal_context *hal_cfg;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;

	hal_cfg = (struct sipa_hal_context *)hdl;
	fifo_cfg = hal_cfg->cmn_fifo_cfg;

	ret = hal_cfg->fifo_ops.get_filled_depth(fifo_id, fifo_cfg,
						 rx_filled, tx_filled);

	return 0;
}
EXPORT_SYMBOL(sipa_hal_get_cmn_fifo_filled_depth);

int sipa_hal_enable_wiap_dma(sipa_hal_hdl hdl, bool dma)
{
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;

	return hal_cfg->glb_ops.enable_wiap_ul_dma(
		hal_cfg->phy_virt_res.glb_base, dma);
}
EXPORT_SYMBOL(sipa_hal_enable_wiap_dma);

/**
 * sipa_hal_reclaim_unuse_node() - reclaim that unfree description.
 * @hdl: &sipa_hal_ctx.
 * @fifo_id: the cmn fifo that need to reclaim.
 *
 * Some node descriptions that sent out may not be free normally,
 * so wo need use soft method to reclaim these node description.
 *
 * Return:
 *	0: reclaim success.
 *	negative value: reclaim fail.
 */
int sipa_hal_reclaim_unuse_node(sipa_hal_hdl hdl,
				enum sipa_cmn_fifo_index fifo_id)
{
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg = hal_cfg->cmn_fifo_cfg;

	return hal_cfg->fifo_ops.reclaim_node_desc(fifo_id, fifo_cfg);
}
EXPORT_SYMBOL(sipa_hal_reclaim_unuse_node);

int sipa_hal_put_rx_fifo_item(sipa_hal_hdl hdl,
			      enum sipa_cmn_fifo_index fifo_id,
			      struct sipa_hal_fifo_item *item)
{
	u32 ret;
	struct sipa_hal_context *hal_cfg;
	struct sipa_node_description_tag node;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;

	hal_cfg = (struct sipa_hal_context *)hdl;
	fifo_cfg = hal_cfg->cmn_fifo_cfg;

	memset(&node, 0, sizeof(node));
	node.address = item->addr;
	node.length = item->len;
	node.dst = item->dst;
	node.offset = item->offset;
	node.src = item->src;
	node.err_code = item->err_code;
	node.net_id = item->netid;
	node.intr = item->intr;

	ret = hal_cfg->fifo_ops.put_node_to_rx_fifo(hal_cfg->dev,
						    fifo_id, fifo_cfg,
						    &node, 0, 1);
	if (ret == 0) {
		dev_err(hal_cfg->dev,
			"put node to rx fifo %d fail\n", fifo_id);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(sipa_hal_put_rx_fifo_item);

int sipa_hal_put_rx_fifo_items(sipa_hal_hdl hdl,
			       enum sipa_cmn_fifo_index fifo_id)
{
	u32 ret, num;
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg = hal_cfg->cmn_fifo_cfg;

	num = kfifo_len(&((fifo_cfg + fifo_id)->rx_priv_fifo));
	num /= sizeof(struct sipa_node_description_tag);
	if (num) {
		ret = hal_cfg->fifo_ops.put_node_to_rx_fifo(hal_cfg->dev,
							    fifo_id, fifo_cfg,
							    NULL, 0, num);
		if (ret != num) {
			dev_err(hal_cfg->dev,
				"put node to rx fifo %d fail\n", fifo_id);
			return -ENOSPC;
		}
	} else {
		dev_err(hal_cfg->dev,
			"fifo id %d rx priv fifo is empty", fifo_id);
	}

	return 0;
}
EXPORT_SYMBOL(sipa_hal_put_rx_fifo_items);

int sipa_hal_cache_rx_fifo_item(sipa_hal_hdl hdl,
				enum sipa_cmn_fifo_index fifo_id,
				struct sipa_hal_fifo_item *item)
{
	u32 ret;
	struct sipa_node_description_tag node;
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg =
			&hal_cfg->cmn_fifo_cfg[fifo_id];

	memset(&node, 0, sizeof(node));
	node.address = item->addr;
	node.length = item->len;
	node.dst = item->dst;
	node.offset = item->offset;
	node.src = item->src;
	node.err_code = item->err_code;
	node.net_id = item->netid;
	node.intr = item->intr;
	node.reserved = item->reserved;

	ret = kfifo_in(&fifo_cfg->rx_priv_fifo, &node, sizeof(node));
	if (ret != sizeof(node)) {
		dev_err(hal_cfg->dev,
			"fifo id %d rx priv fifo is full\n", fifo_id);
		return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(sipa_hal_cache_rx_fifo_item);

bool sipa_hal_is_rx_fifo_empty(sipa_hal_hdl hdl,
			       enum sipa_cmn_fifo_index fifo_id)
{
	u32 ret;
	struct sipa_hal_context *hal_cfg;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;

	hal_cfg = (struct sipa_hal_context *)hdl;
	fifo_cfg = hal_cfg->cmn_fifo_cfg;

	ret = hal_cfg->fifo_ops.get_rx_empty_status(fifo_id, fifo_cfg);

	return ret;
}
EXPORT_SYMBOL(sipa_hal_is_rx_fifo_empty);

bool sipa_hal_check_rx_priv_fifo_is_empty(sipa_hal_hdl hdl,
					  enum sipa_cmn_fifo_index fifo_id)
{
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg =
			&hal_cfg->cmn_fifo_cfg[fifo_id];

	return kfifo_is_empty(&fifo_cfg->rx_priv_fifo);
}
EXPORT_SYMBOL(sipa_hal_check_rx_priv_fifo_is_empty);

bool sipa_hal_check_rx_priv_fifo_is_full(sipa_hal_hdl hdl,
					 enum sipa_cmn_fifo_index fifo_id)
{
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg =
			&hal_cfg->cmn_fifo_cfg[fifo_id];

	return kfifo_is_full(&fifo_cfg->rx_priv_fifo);
}
EXPORT_SYMBOL(sipa_hal_check_rx_priv_fifo_is_full);

bool sipa_hal_is_rx_fifo_full(sipa_hal_hdl hdl,
			      enum sipa_cmn_fifo_index fifo_id)
{
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg = hal_cfg->cmn_fifo_cfg;

	return hal_cfg->fifo_ops.get_rx_full_status(fifo_id, fifo_cfg);
}
EXPORT_SYMBOL(sipa_hal_is_rx_fifo_full);

bool sipa_hal_bk_fifo_node(sipa_hal_hdl hdl,
			   enum sipa_cmn_fifo_index fifo_id)
{
	u32 ret;
	ssize_t node_size = sizeof(struct sipa_node_description_tag);
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg =
		&hal_cfg->cmn_fifo_cfg[fifo_id];

	if (!fifo_cfg->state || !kfifo_initialized(&fifo_cfg->rx_priv_fifo) ||
	    !kfifo_initialized(&fifo_cfg->tx_priv_fifo) || !fifo_cfg->is_pam)
		return false;

	kfifo_reset(&fifo_cfg->tx_priv_fifo);
	kfifo_reset(&fifo_cfg->rx_priv_fifo);
	ret = kfifo_in(&fifo_cfg->rx_priv_fifo,
		       fifo_cfg->rx_fifo.virtual_addr,
		       fifo_cfg->rx_fifo.depth * node_size);
	if (ret != fifo_cfg->rx_fifo.depth * node_size)
		pr_err("backup %d rx fifo failed ret = %d\n", fifo_id, ret);

	ret = kfifo_in(&fifo_cfg->tx_priv_fifo,
		       fifo_cfg->tx_fifo.virtual_addr,
		       fifo_cfg->tx_fifo.depth * node_size);
	if (ret != fifo_cfg->tx_fifo.depth * node_size)
		pr_err("backup %d tx fifo failed ret = %d\n", fifo_id, ret);

	return true;
}
EXPORT_SYMBOL(sipa_hal_bk_fifo_node);

bool sipa_hal_resume_fifo_node(sipa_hal_hdl hdl,
			       enum sipa_cmn_fifo_index fifo_id)
{
	u32 ret;
	ssize_t node_size = sizeof(struct sipa_node_description_tag);
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg =
		&hal_cfg->cmn_fifo_cfg[fifo_id];

	if (!kfifo_initialized(&fifo_cfg->rx_priv_fifo) ||
	    !kfifo_initialized(&fifo_cfg->tx_priv_fifo) || !fifo_cfg->is_pam)
		return false;

	if (!kfifo_is_full(&fifo_cfg->rx_priv_fifo) ||
	    !kfifo_is_full(&fifo_cfg->tx_priv_fifo))
		return false;

	if (!hal_cfg->fifo_ops.get_tx_depth(fifo_id, hal_cfg->cmn_fifo_cfg))
		return false;

	ret = kfifo_out(&fifo_cfg->rx_priv_fifo,
			fifo_cfg->rx_fifo.virtual_addr,
			fifo_cfg->rx_fifo.depth * node_size);
	if (ret != fifo_cfg->rx_fifo.depth * node_size)
		pr_err("resume %d rx fifo node failed\n", fifo_id);

	ret = kfifo_out(&fifo_cfg->tx_priv_fifo,
			fifo_cfg->tx_fifo.virtual_addr,
			fifo_cfg->tx_fifo.depth * node_size);
	if (ret != fifo_cfg->tx_fifo.depth * node_size)
		pr_err("resume %d tx fifo node failed\n", fifo_id);

	return true;
}
EXPORT_SYMBOL(sipa_hal_resume_fifo_node);

bool sipa_hal_is_tx_fifo_empty(sipa_hal_hdl hdl,
			       enum sipa_cmn_fifo_index fifo_id)
{
	u32 ret;
	struct sipa_hal_context *hal_cfg;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;

	hal_cfg = (struct sipa_hal_context *)hdl;
	fifo_cfg = hal_cfg->cmn_fifo_cfg;

	ret = hal_cfg->fifo_ops.get_tx_empty_status(fifo_id, fifo_cfg);

	return ret;
}
EXPORT_SYMBOL(sipa_hal_is_tx_fifo_empty);

/**
 * sipa_hal_check_send_cmn_fifo_com() - Check cmn fifo send and free whether
 * completion
 * @hdl: &sipa_hal_ctx
 * @fifo_id: The cmn fifo id that need to be check.
 *
 * If fifo is not CP_DL, its tx/rx fifo's wptr/rptr should be equal,
 * if fifo is CP_DL, its wptr of tx fifo should be greated than rx fifo
 * wptr and rptr total depth.
 *
 * Return:
 *	true: send and free complete.
 *	false: send or free not complete.
 */
bool sipa_hal_check_send_cmn_fifo_com(sipa_hal_hdl hdl,
				      enum sipa_cmn_fifo_index fifo_id)
{
	bool status;
	u32 ret, tx_rptr, tx_wptr, rx_rptr, rx_wptr, depth;
	struct sipa_hal_context *hal_cfg = (struct sipa_hal_context *)hdl;
	struct sipa_common_fifo_cfg_tag *fifo_cfg = hal_cfg->cmn_fifo_cfg;

	depth = (fifo_cfg + fifo_id)->tx_fifo.depth;
	ret = hal_cfg->fifo_ops.get_tx_empty_status(fifo_id, fifo_cfg);
	hal_cfg->fifo_ops.get_tx_fifo_wr_rd_ptr(fifo_id, fifo_cfg,
						&tx_wptr, &tx_rptr);
	hal_cfg->fifo_ops.get_rx_fifo_wr_rd_ptr(fifo_id, fifo_cfg,
						&rx_wptr, &rx_rptr);
	if (fifo_id != SIPA_FIFO_CP_DL) {
		status = (tx_wptr == tx_rptr) && (tx_rptr == rx_wptr) &&
			(rx_wptr == rx_rptr);
	} else {
		status = (rx_wptr == rx_rptr) &&
			(((depth | (depth - 1)) &
			(rx_wptr + depth)) == tx_wptr);
		ret = true;
	}

	if (status && ret)
		return true;

	return false;
}
EXPORT_SYMBOL(sipa_hal_check_send_cmn_fifo_com);

int sipa_hal_free_tx_rx_fifo_buf(sipa_hal_hdl hdl,
				 enum sipa_cmn_fifo_index fifo_id,
				 struct sipa_hal_fifo_item *item)
{
	struct sipa_hal_context *hal_cfg;

	hal_cfg = (struct sipa_hal_context *)hdl;

	free_rx_fifo_ram(hal_cfg->dev, hal_cfg, fifo_id);
	free_tx_fifo_ram(hal_cfg->dev, hal_cfg, fifo_id);

	return 0;
}
EXPORT_SYMBOL(sipa_hal_free_tx_rx_fifo_buf);

int sipa_hal_init_pam_param(enum sipa_cmn_fifo_index dl_idx,
			    enum sipa_cmn_fifo_index ul_idx,
			    struct sipa_to_pam_info *out)
{
	struct sipa_common_fifo_cfg_tag *dl_fifo_cfg, *ul_fifo_cfg;
	struct sipa_hal_context *hal_cfg = &sipa_hal_ctx;

	dl_fifo_cfg = &hal_cfg->cmn_fifo_cfg[dl_idx];
	ul_fifo_cfg = &hal_cfg->cmn_fifo_cfg[ul_idx];

	out->dl_fifo.tx_fifo_base_addr =
		IPA_STI_64BIT(dl_fifo_cfg->tx_fifo.fifo_base_addr_l,
			      dl_fifo_cfg->tx_fifo.fifo_base_addr_h);
	out->dl_fifo.rx_fifo_base_addr =
		IPA_STI_64BIT(dl_fifo_cfg->rx_fifo.fifo_base_addr_l,
			      dl_fifo_cfg->rx_fifo.fifo_base_addr_h);

	out->ul_fifo.tx_fifo_base_addr =
		IPA_STI_64BIT(ul_fifo_cfg->tx_fifo.fifo_base_addr_l,
			      ul_fifo_cfg->tx_fifo.fifo_base_addr_h);
	out->ul_fifo.rx_fifo_base_addr =
		IPA_STI_64BIT(ul_fifo_cfg->rx_fifo.fifo_base_addr_l,
			      ul_fifo_cfg->rx_fifo.fifo_base_addr_h);

	out->dl_fifo.fifo_sts_addr = dl_fifo_cfg->fifo_phy_addr;
	out->ul_fifo.fifo_sts_addr = ul_fifo_cfg->fifo_phy_addr;

	out->dl_fifo.fifo_depth = dl_fifo_cfg->tx_fifo.depth;
	out->ul_fifo.fifo_depth = ul_fifo_cfg->tx_fifo.depth;

	return 0;
}
EXPORT_SYMBOL(sipa_hal_init_pam_param);

int sipa_swap_hash_table(struct sipa_hash_table *new_tbl,
			 struct sipa_hash_table *old_tbl)
{
	u32 len, addrl, addrh;
	struct sipa_hal_context *hal_cfg = &sipa_hal_ctx;

	if (old_tbl) {
		hal_cfg->glb_ops.get_hash_table(hal_cfg->phy_virt_res.glb_base,
						&addrl, &addrh, &len);
		old_tbl->tbl_phy_addr = IPA_STI_64BIT(addrl, addrh);
		old_tbl->depth = len;
	}

	if (new_tbl) {
		addrl = IPA_GET_LOW32(new_tbl->tbl_phy_addr);
		addrh = IPA_GET_HIGH32(new_tbl->tbl_phy_addr);
		hal_cfg->glb_ops.hash_table_switch(
			hal_cfg->phy_virt_res.glb_base,
			addrl, addrh, new_tbl->depth);
		/*
		 * Because return done does not mean that the hash table is
		 * actually written to ddr, so add delay ensure that IPA gets
		 * the latest hash table.
		 */
		udelay(500);
	}

	return 0;
}
EXPORT_SYMBOL(sipa_swap_hash_table);

int sipa_hal_ctrl_action(u32 enable)
{
	struct sipa_hal_context *hal_cfg = &sipa_hal_ctx;

	return hal_cfg->glb_ops.ctrl_ipa_action(hal_cfg->phy_virt_res.glb_base,
						enable);
}
EXPORT_SYMBOL(sipa_hal_ctrl_action);

bool sipa_hal_get_resume_status(void)
{
	struct sipa_hal_context *hal_cfg = &sipa_hal_ctx;
	void __iomem *base = hal_cfg->phy_virt_res.glb_base;

	return hal_cfg->glb_ops.get_resume_status(base);
}
EXPORT_SYMBOL(sipa_hal_get_resume_status);

bool sipa_hal_get_pause_status(void)
{
	struct sipa_hal_context *hal_cfg = &sipa_hal_ctx;
	void __iomem *base = hal_cfg->phy_virt_res.glb_base;

	return hal_cfg->glb_ops.get_pause_status(base);
}
EXPORT_SYMBOL(sipa_hal_get_pause_status);

void sipa_test_enable_periph_int_to_sw(void)
{
	struct sipa_hal_context *hal_cfg = &sipa_hal_ctx;

	hal_cfg->glb_ops.map_interrupt_src_en(hal_cfg->phy_virt_res.glb_base,
					      1, 0x3ffff);
}
EXPORT_SYMBOL(sipa_test_enable_periph_int_to_sw);

void sipa_resume_glb_reg_cfg(struct sipa_plat_drv_cfg *cfg)
{
	struct sipa_hal_context *hal_cfg = &sipa_hal_ctx;
	void __iomem *glb_base = hal_cfg->phy_virt_res.glb_base;

	hal_cfg->glb_ops.enable_wiap_ul_dma(glb_base, (u32)cfg->wiap_ul_dma);

	hal_cfg->glb_ops.enable_cp_through_pcie(glb_base,
						(u32)cfg->need_through_pcie);

	hal_cfg->glb_ops.enable_def_flowctrl_to_src_blk(glb_base);

	hal_cfg->glb_ops.set_mode(glb_base, cfg->is_bypass);

	if (cfg->is_bypass)
		hal_cfg->glb_ops.enable_pcie_intr_write_reg_mode(glb_base, 1);
}
EXPORT_SYMBOL(sipa_resume_glb_reg_cfg);
