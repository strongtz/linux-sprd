#include <linux/sipa.h>
#include <linux/dma-mapping.h>

#include "../sipa_hal_priv.h"
#include "sipa_device.h"
#include "sipa_glb_phy.h"
#include "sipa_fifo_phy.h"

static int
ipa_reclaim_tx_fifo_unread_node_desc(struct sipa_common_fifo_cfg_tag *fifo_cfg,
				     u32 tx_rptr, u32 tx_wptr,
				     u32 rx_rptr,  u32 rx_wptr)
{
	u32 index, tmp, depth;
	ssize_t node_size = sizeof(struct sipa_node_description_tag);
	struct sipa_node_description_tag *fill_node, *free_node;

	fill_node = fifo_cfg->tx_fifo.virtual_addr;
	free_node = fifo_cfg->rx_fifo.virtual_addr;
	depth = (tx_wptr - rx_wptr) & PTR_MASK(fifo_cfg->rx_fifo.depth);

	index = rx_wptr & (fifo_cfg->tx_fifo.depth - 1);
	if (index + depth <= fifo_cfg->rx_fifo.depth) {
		memcpy(free_node + index, fill_node + index,
		       node_size * depth);
	} else {
		tmp = fifo_cfg->rx_fifo.depth - index;
		memcpy(free_node + index, fill_node + index,
		       node_size * tmp);
		tmp = depth - tmp;
		memcpy(free_node, fill_node, node_size * tmp);
	}

	/* Ensure that data copy completion before we update rptr/wptr */
	smp_wmb();

	tx_rptr = (tx_rptr + depth) &
		PTR_MASK(fifo_cfg->tx_fifo.depth);
	if (!ipa_phy_update_tx_fifo_rptr(fifo_cfg->fifo_reg_base,
					 tx_rptr)) {
		pr_err("sipa reclaim update tx_fifo rptr failed\n");
		return -EIO;
	}
	rx_wptr = (rx_wptr + depth) &
		PTR_MASK(fifo_cfg->rx_fifo.depth);
	if (!ipa_phy_update_rx_fifo_wptr(fifo_cfg->fifo_reg_base,
					 rx_wptr)) {
		pr_err("sipa reclaim update rx_fifo wptr failed\n");
		return -EIO;
	}

	return 0;
}

static int
ipa_reclaim_unfree_node_desc(struct sipa_common_fifo_cfg_tag *fifo_cfg,
			     u32 tx_rptr, u32 tx_wptr,
			     u32 rx_rptr, u32 rx_wptr)
{
	u32 index, tmp, num;
	ssize_t node_size = sizeof(struct sipa_node_description_tag);
	struct sipa_node_description_tag *fill_node, *free_node;

	fill_node = fifo_cfg->tx_fifo.virtual_addr;
	free_node = fifo_cfg->rx_fifo.virtual_addr;
	num = (tx_wptr - rx_wptr) & PTR_MASK(fifo_cfg->rx_fifo.depth);

	index = rx_wptr & (fifo_cfg->rx_fifo.depth - 1);
	if (index + num <= fifo_cfg->rx_fifo.depth) {
		memcpy(free_node + index, fill_node + index,
		       node_size * num);
	} else {
		tmp = fifo_cfg->rx_fifo.depth - index;
		memcpy(free_node + index, fill_node + index,
		       node_size * tmp);
		tmp = num - tmp;
		memcpy(free_node, fill_node, node_size * tmp);
	}

	/* Ensure that data copy completion before we update rptr/wptr */
	smp_wmb();

	rx_wptr = (rx_wptr + num) &
		PTR_MASK(fifo_cfg->rx_fifo.depth);
	if (!ipa_phy_update_rx_fifo_wptr(fifo_cfg->fifo_reg_base,
					 rx_wptr)) {
		pr_err("sipa reclaim unfree update rx_fifo wptr failed\n");
		return -EIO;
	}

	return 0;
}

static inline int
ipa_put_pkts_to_rx_fifo(struct device *dev,
			struct sipa_common_fifo_cfg_tag *fifo_cfg,
			u32 num)
{
	dma_addr_t dma_addr;
	u32 tmp = 0, ret = 0, index = 0, left_cnt = 0, size;
	ssize_t node_size = sizeof(struct sipa_node_description_tag);
	struct sipa_node_description_tag *node =
			(struct sipa_node_description_tag *)
			fifo_cfg->rx_fifo.virtual_addr;

	size = fifo_cfg->rx_fifo.size;
	dma_addr = fifo_cfg->rx_fifo.fifo_base_addr_l |
		(fifo_cfg->rx_fifo.fifo_base_addr_h << 8);

	left_cnt = fifo_cfg->rx_fifo.depth -
		ipa_phy_get_rx_fifo_filled_depth(fifo_cfg->fifo_reg_base);

	if (!left_cnt)
		return -ENOSPC;

	if (left_cnt < num)
		num = left_cnt;

	index = fifo_cfg->rx_fifo.wr & (fifo_cfg->rx_fifo.depth - 1);
	if (index + num <= fifo_cfg->rx_fifo.depth) {
		ret = kfifo_out(&fifo_cfg->rx_priv_fifo, node + index,
				node_size * num);
		dma_sync_single_for_device(dev, dma_addr + index * node_size,
					   node_size * num, DMA_TO_DEVICE);
	} else {
		tmp = fifo_cfg->rx_fifo.depth - index;
		ret = kfifo_out(&fifo_cfg->rx_priv_fifo, node + index,
			tmp * node_size);
		dma_sync_single_for_device(dev, dma_addr + index * node_size,
					   node_size * tmp, DMA_TO_DEVICE);
		tmp = num - tmp;
		ret = kfifo_out(&fifo_cfg->rx_priv_fifo, node,
			tmp * node_size);
		dma_sync_single_for_device(dev, dma_addr,
					   node_size * tmp, DMA_TO_DEVICE);
	}

	fifo_cfg->rx_fifo.wr =
		(fifo_cfg->rx_fifo.wr + num) &
		PTR_MASK(fifo_cfg->rx_fifo.depth);
	ret = ipa_phy_update_rx_fifo_wptr(
			  fifo_cfg->fifo_reg_base,
			  fifo_cfg->rx_fifo.wr);

	if (!ret)
		pr_err("ipa_phy_update_rx_fifo_rptr fail\n");

	return num;
}

static inline u32
ipa_recv_pkts_from_tx_fifo(struct device *dev,
			   struct sipa_common_fifo_cfg_tag *fifo_cfg,
			   u32 num)
{
	dma_addr_t dma_addr;
	u32 tmp = 0, index = 0;
	ssize_t node_size = sizeof(struct sipa_node_description_tag);

	dma_addr = fifo_cfg->tx_fifo.fifo_base_addr_l |
		(fifo_cfg->tx_fifo.fifo_base_addr_h << 8);

	num = ipa_phy_get_tx_fifo_filled_depth(fifo_cfg->fifo_reg_base);
	index = fifo_cfg->tx_fifo.rd & (fifo_cfg->tx_fifo.depth - 1);
	if (index + num <= fifo_cfg->tx_fifo.depth) {
		dma_sync_single_for_cpu(dev, dma_addr + index * node_size,
					num * node_size, DMA_FROM_DEVICE);
	} else {
		tmp = fifo_cfg->tx_fifo.depth - index;
		dma_sync_single_for_cpu(dev, dma_addr + index * node_size,
					tmp * node_size, DMA_FROM_DEVICE);
		tmp = num - tmp;
		dma_sync_single_for_cpu(dev, dma_addr,
					tmp * node_size, DMA_FROM_DEVICE);
	}

	return num;
}

static u32 ipa_common_fifo_hal_open(enum sipa_cmn_fifo_index id,
				    struct sipa_common_fifo_cfg_tag *cfg_base,
				    void *cookie)
{
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo = NULL;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (ipa_term_fifo->state == TRUE) {
		pr_err("fifo_id = %d has already opened\n",
		       ipa_term_fifo->fifo_id);
		return TRUE;
	}

	ipa_phy_set_rx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					ipa_term_fifo->rx_fifo.depth);
	ipa_phy_set_rx_fifo_addr(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->rx_fifo.fifo_base_addr_l,
				 ipa_term_fifo->rx_fifo.fifo_base_addr_h);

	ipa_phy_set_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					ipa_term_fifo->tx_fifo.depth);
	ipa_phy_set_tx_fifo_addr(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->tx_fifo.fifo_base_addr_l,
				 ipa_term_fifo->tx_fifo.fifo_base_addr_h);

	ipa_phy_set_cur_term_num(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->cur);
	ipa_phy_set_dst_term_num(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->dst);

	ipa_phy_update_rx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_tx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);

	ipa_term_fifo->rx_fifo.rd = 0;
	ipa_term_fifo->rx_fifo.wr = 0;
	ipa_term_fifo->tx_fifo.rd = 0;
	ipa_term_fifo->tx_fifo.wr = 0;

	ipa_term_fifo->fifo_name = cookie;
	ipa_term_fifo->state = TRUE;

	return TRUE;
}

static u32 ipa_common_fifo_hal_close(enum sipa_cmn_fifo_index id,
				     struct sipa_common_fifo_cfg_tag *cfg_base)
{
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ipa_phy_set_rx_fifo_total_depth(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_set_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base, 0);

	ipa_phy_update_rx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_tx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);

	ipa_term_fifo->rx_fifo.rd = 0;
	ipa_term_fifo->rx_fifo.wr = 0;
	ipa_term_fifo->tx_fifo.rd = 0;
	ipa_term_fifo->tx_fifo.wr = 0;

	ipa_term_fifo->state = FALSE;

	return TRUE;
}

static u32
ipa_common_fifo_hal_reset_fifo(enum sipa_cmn_fifo_index id,
			       struct sipa_common_fifo_cfg_tag *cfg_base)
{
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ipa_phy_set_rx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					ipa_term_fifo->rx_fifo.depth);
	ipa_phy_set_rx_fifo_addr(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->rx_fifo.fifo_base_addr_l,
				 ipa_term_fifo->rx_fifo.fifo_base_addr_h);

	ipa_phy_set_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					ipa_term_fifo->tx_fifo.depth);
	ipa_phy_set_tx_fifo_addr(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->tx_fifo.fifo_base_addr_l,
				 ipa_term_fifo->tx_fifo.fifo_base_addr_h);

	ipa_phy_update_rx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);

	ipa_phy_update_tx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_term_fifo->rx_fifo.rd = 0;
	ipa_term_fifo->rx_fifo.wr = 0;

	ipa_term_fifo->tx_fifo.rd = 0;
	ipa_term_fifo->tx_fifo.wr = 0;

	ipa_term_fifo->state = TRUE;

	return TRUE;
}

static u32
ipa_common_fifo_hal_set_rx_depth(enum sipa_cmn_fifo_index id,
				 struct sipa_common_fifo_cfg_tag *cfg_base,
				 u32 depth)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ret = ipa_phy_set_rx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					      depth);

	return ret;
}

static u32
ipa_common_fifo_hal_get_rx_depth(enum sipa_cmn_fifo_index id,
				 struct sipa_common_fifo_cfg_tag *cfg_base)
{
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return 0;
	}

	return ipa_phy_get_rx_fifo_total_depth(ipa_term_fifo->fifo_reg_base);
}

static u32
ipa_common_fifo_hal_set_tx_depth(enum sipa_cmn_fifo_index id,
				 struct sipa_common_fifo_cfg_tag *cfg_base,
				 u32 depth)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ret = ipa_phy_set_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					      depth);

	return ret;
}

static u32
ipa_common_fifo_hal_get_tx_depth(enum sipa_cmn_fifo_index id,
				 struct sipa_common_fifo_cfg_tag *cfg_base)
{
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return 0;
	}

	return ipa_phy_get_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base);
}

static u32
ipa_common_fifo_hal_set_interrupt_drop_packet(enum sipa_cmn_fifo_index id,
					      struct sipa_common_fifo_cfg_tag *cfg_base,
					      u32 enable, sipa_hal_notify_cb cb)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (enable) {
		ret = ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base,
					     IPA_DROP_PACKET_OCCUR_INT_EN);
	} else {
		ret = ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					      IPA_DROP_PACKET_OCCUR_INT_EN);
	}

	return ret;
}

static u32
ipa_common_fifo_hal_set_interrupt_error_code(enum sipa_cmn_fifo_index id,
					     struct sipa_common_fifo_cfg_tag *cfg_base,
					     u32 enable,
					     sipa_hal_notify_cb cb)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (enable) {
		ret = ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base,
					     IPA_ERRORCODE_IN_TX_FIFO_EN);
	} else {
		ret = ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					      IPA_ERRORCODE_IN_TX_FIFO_EN);
	}

	return ret;
}


static u32
ipa_common_fifo_hal_set_interrupt_timeout(enum sipa_cmn_fifo_index id,
					  struct sipa_common_fifo_cfg_tag *cfg_base,
					  u32 enable,
					  u32 time, sipa_hal_notify_cb cb)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (enable) {
		ret = ipa_phy_set_tx_fifo_interrupt_delay_timer(
				  ipa_term_fifo->fifo_reg_base,
				  time);
		if (ret) {
			ret = ipa_phy_enable_int_bit(
					  ipa_term_fifo->fifo_reg_base,
					  IPA_TXFIFO_INT_DELAY_TIMER_SW_EN);
		} else {
			pr_err("fifo(%d) set timeout threshold fail\n", id);
			ret = FALSE;
		}
	} else {
		ret = ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					      IPA_TXFIFO_INT_DELAY_TIMER_SW_EN);
	}

	return ret;
}

static u32
ipa_common_fifo_hal_set_hw_interrupt_timeout(enum sipa_cmn_fifo_index id,
					     struct sipa_common_fifo_cfg_tag *cfg_base,
					     u32 enable, u32 time,
					     sipa_hal_notify_cb cb)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (enable) {
		ret = ipa_phy_set_tx_fifo_interrupt_delay_timer(
			ipa_term_fifo->fifo_reg_base, time);
		if (ret) {
			ret = ipa_phy_enable_int_bit(
				ipa_term_fifo->fifo_reg_base,
				IPA_TX_FIFO_DELAY_TIMER_EN);
		} else {
			pr_err("fifo(%d) set timeout threshold fail\n", id);
			ret = FALSE;
		}
	} else {
		ret = ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					      IPA_TX_FIFO_DELAY_TIMER_EN);
	}

	return ret;
}

static u32
ipa_common_fifo_hal_set_interrupt_threshold(enum sipa_cmn_fifo_index id,
					    struct sipa_common_fifo_cfg_tag *cfg_base,
					    u32 enable, u32 cnt,
					    sipa_hal_notify_cb cb)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (enable) {
		ret = ipa_phy_set_tx_fifo_interrupt_threshold(
			ipa_term_fifo->fifo_reg_base, cnt);
		if (ret) {
			ret = ipa_phy_enable_int_bit(
				ipa_term_fifo->fifo_reg_base,
				IPA_TXFIFO_INT_THRESHOLD_ONESHOT_EN);
		} else {
			pr_err("fifo(%d) set threshold fail\n", id);
			ret = FALSE;
		}
	} else {
		ret =
		ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					IPA_TXFIFO_INT_THRESHOLD_ONESHOT_EN);
	}

	return ret;
}

static u32
ipa_common_fifo_hal_set_hw_interrupt_threshold(enum sipa_cmn_fifo_index id,
					       struct sipa_common_fifo_cfg_tag *cfg_base,
					       u32 enable, u32 cnt,
					       sipa_hal_notify_cb cb)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (enable) {
		ret = ipa_phy_set_tx_fifo_interrupt_threshold(
			ipa_term_fifo->fifo_reg_base, cnt);
		if (ret) {
			ret = ipa_phy_enable_int_bit(
				ipa_term_fifo->fifo_reg_base,
				IPA_TX_FIFO_THRESHOLD_EN);
		} else {
			pr_err("fifo(%d) set threshold fail\n", id);
			ret = FALSE;
		}
	} else {
		ret = ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					      IPA_TX_FIFO_THRESHOLD_EN);
	}

	return ret;
}

static u32
ipa_common_fifo_hal_set_src_dst_term(enum sipa_cmn_fifo_index id,
				     struct sipa_common_fifo_cfg_tag *cfg_base,
				     u32 src, u32 dst)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ret = ipa_phy_set_cur_term_num(ipa_term_fifo->fifo_reg_base, src);
	ret = ipa_phy_set_dst_term_num(ipa_term_fifo->fifo_reg_base, dst);

	return ret;
}

static u32
ipa_common_fifo_hal_enable_local_flowctrl_interrupt(enum sipa_cmn_fifo_index id,
						    struct sipa_common_fifo_cfg_tag *cfg_base,
						    u32 enable, u32 irq_mode,
						    sipa_hal_notify_cb cb)
{
	u32 ret, irq;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	switch (irq_mode) {
	case 0:
		irq = IPA_RX_FIFO_INT_EXIT_FLOW_CTRL_EN;
		break;
	case 1:
		irq = IPA_RX_FIFO_INT_ENTER_FLOW_CTRL_EN;
		break;
	case 2:
		irq = IPA_RX_FIFO_INT_EXIT_FLOW_CTRL_EN |
			  IPA_RX_FIFO_INT_ENTER_FLOW_CTRL_EN;
		break;
	default:
		pr_err("don't have this %d irq type\n", irq_mode);
		return FALSE;
	}

	if (enable) {
		ret = ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base, irq);
		if (!ret) {
			pr_err("fifo_id = %d irq_mode = %d set failed\n",
			       id, irq);
			return FALSE;
		}
	} else {
		ret = ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					      irq);
	}

	return ret;
}

static u32
ipa_common_fifo_hal_enable_remote_flowctrl_interrupt(enum sipa_cmn_fifo_index id,
						     struct sipa_common_fifo_cfg_tag *cfg_base,
						     u32 work_mode,
						     u32 tx_entry_watermark,
						     u32 tx_exit_watermark,
						     u32 rx_entry_watermark,
						     u32 rx_exit_watermark)
{
	u32 ret = 0;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo = NULL;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ret = ipa_phy_set_tx_fifo_exit_flow_ctrl_watermark(
		ipa_term_fifo->fifo_reg_base, tx_exit_watermark);
	if (unlikely(!ret)) {
		pr_err("fifo_id = %d tx_exit_watermark(0x%x) failed\n",
		       id, tx_exit_watermark);
		return FALSE;
	}

	ret = ipa_phy_set_tx_fifo_entry_flow_ctrl_watermark(
		ipa_term_fifo->fifo_reg_base, tx_entry_watermark);
	if (unlikely(!ret)) {
		pr_err("fifo_id = %d tx_entry_watermark(0x%x) failed\n",
		       id, tx_entry_watermark);
		return FALSE;
	}

	ret = ipa_phy_set_rx_fifo_exit_flow_ctrl_watermark(
		ipa_term_fifo->fifo_reg_base, rx_exit_watermark);
	if (unlikely(!ret)) {
		pr_err("fifo_id = %d rx_exit_watermark(0x%x) failed\n",
		       id, rx_exit_watermark);
		return FALSE;
	}

	ret = ipa_phy_set_rx_fifo_entry_flow_ctrl_watermark(
		ipa_term_fifo->fifo_reg_base, rx_entry_watermark);
	if (unlikely(!ret)) {
		pr_err("fifo_id = %d rx_entry_watermark(0x%x) failed\n",
		       id, rx_entry_watermark);
		return FALSE;
	}

	ret = ipa_phy_set_flow_ctrl_config(ipa_term_fifo->fifo_reg_base,
					   work_mode);

	return ret;
}

static u32
ipa_common_fifo_hal_set_interrupt_intr(enum sipa_cmn_fifo_index id,
				       struct sipa_common_fifo_cfg_tag *cfg_base,
				       u32 enable,
				       sipa_hal_notify_cb cb)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (enable) {
		ret = ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base,
					     IPA_TX_FIFO_INTR_SW_BIT_EN);
	} else {
		ret = ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					      IPA_TX_FIFO_INTR_SW_BIT_EN);
	}

	return ret;
}

static u32
ipa_common_fifo_hal_set_interrupt_txfifo_overflow(enum sipa_cmn_fifo_index id,
						  struct sipa_common_fifo_cfg_tag *cfg_base,
						  u32 enable,
						  sipa_hal_notify_cb cb)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (enable) {
		ret = ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base,
					     IPA_TXFIFO_OVERFLOW_EN);
	} else {
		ret = ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					      IPA_TXFIFO_OVERFLOW_EN);
	}

	return ret;
}

static u32
ipa_common_fifo_hal_set_interrupt_txfifo_full(enum sipa_cmn_fifo_index id,
					      struct sipa_common_fifo_cfg_tag *cfg_base,
					      u32 enable, sipa_hal_notify_cb cb)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (enable) {
		ret = ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base,
					     IPA_TXFIFO_FULL_INT_EN);
	} else {
		ret = ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					      IPA_TXFIFO_FULL_INT_EN);
	}

	return ret;
}

/**
 * Description: Receive Node from tx fifo.
 * Input:
 *   @id: The FIFO id that need to be operated.
 *   @pkt: The node that need to be stored address.
 *   @num: The num of receive.
 * OUTPUT:
 *   @The num that has be received from tx fifo successful.
 * Note:
 */
static u32
ipa_common_fifo_hal_put_node_to_rx_fifo(struct device *dev,
					enum sipa_cmn_fifo_index id,
					struct sipa_common_fifo_cfg_tag *cfg_base,
					struct sipa_node_description_tag *node,
					u32 force_intr, u32 num)
{
	u32 ret = 0;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (ipa_term_fifo->is_pam)
		ret = ipa_put_pkt_to_rx_fifo(ipa_term_fifo, force_intr,
					     node, num);
	else
		ret = ipa_put_pkts_to_rx_fifo(dev, ipa_term_fifo, num);

	return ret;
}
/**
 * Description: Put Node to rx fifo.
 * Input:
 *   @id: The FIFO id that need to be operated.
 *   @pkt: The node address that need to be put into rx fifo.
 *   @num: The number of put.
 * OUTPUT:
 *   @The num that has be put into rx fifo successful.
 * Note:
 */
static u32
ipa_common_fifo_hal_put_node_to_tx_fifo(struct device *dev,
					enum sipa_cmn_fifo_index id,
					struct sipa_common_fifo_cfg_tag *cfg_base,
					struct sipa_node_description_tag *pkt,
					u32 force_intr, u32 num)
{
	u32 ret = 0;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ret = ipa_put_pkt_to_tx_fifo(ipa_term_fifo, pkt, num);

	return ret;
}

/*
 * Description: Get Node from tx fifo.
 * Input:
 *   id: The FIFO id that need to be operated.
 *   pkt: The node address that need to store node description from tx fifo.
 *   num: The number of get.
 * OUTPUT:
 *   The number that has got from tx fifo successful.
 * Note:
 */
static u32
ipa_common_fifo_hal_get_node_from_rx_fifo(struct device *dev,
					  enum sipa_cmn_fifo_index id,
					  struct sipa_common_fifo_cfg_tag *cfg_base,
					  struct sipa_node_description_tag *pkt,
					  u32 force_intr, u32 num)
{
	u32 ret = 0;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ret = ipa_get_pkt_from_rx_fifo(ipa_term_fifo, pkt, num);

	return ret;
}

static u32
ipa_common_fifo_hal_get_left_cnt(enum sipa_cmn_fifo_index id,
				 struct sipa_common_fifo_cfg_tag *cfg_base)
{
	u32 left_cnt;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo = NULL;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	left_cnt =
		ipa_phy_get_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base) -
		ipa_phy_get_tx_fifo_filled_depth(ipa_term_fifo->fifo_reg_base);

	return left_cnt;
}

/*
 * Description: Send Node to rx fifo.
 * Input:
 *   id: The FIFO id that need to be operated.
 *   pkt: The node address that need send to rx fifo.
 *   num: The number of need to send.
 * OUTPUT:
 *   The number that has get from tx fifo successful.
 * Note:
 */
static u32
ipa_common_fifo_hal_recv_node_from_tx_fifo(struct device *dev,
					   enum sipa_cmn_fifo_index id,
					   struct sipa_common_fifo_cfg_tag *cfg_base,
					   struct sipa_node_description_tag *node,
					   u32 force_intr, u32 num)
{
	u32 ret = 0;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (ipa_term_fifo->is_pam)
		ret = ipa_recv_pkt_from_tx_fifo(ipa_term_fifo, node, num);
	else
		ret = ipa_recv_pkts_from_tx_fifo(dev, ipa_term_fifo, num);

	return ret;
}

static u32
ipa_common_fifo_hal_tx_fill(enum sipa_cmn_fifo_index id,
			    struct sipa_common_fifo_cfg_tag *cfg_base,
			    struct sipa_node_description_tag *node, u32 num)
{
	int i;
	struct sipa_node_description_tag *fifo_addr;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	fifo_addr =
		(struct sipa_node_description_tag *)
		(((u64)ipa_term_fifo->tx_fifo.fifo_base_addr_l) |
		 ((u64)ipa_term_fifo->tx_fifo.fifo_base_addr_h << 32));

	if (node != NULL) {
		for (i = 0; i < num; i++) {
			memcpy(fifo_addr + i, node + i,
			       sizeof(struct sipa_node_description_tag));
		}
	} else {
		return FALSE;
	}

	return TRUE;
}

static u32
ipa_common_fifo_hal_rx_fill(enum sipa_cmn_fifo_index id,
			    struct sipa_common_fifo_cfg_tag *cfg_base,
			    struct sipa_node_description_tag *node, u32 num)
{
	int i;
	struct sipa_node_description_tag *fifo_addr;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	fifo_addr =
		(struct sipa_node_description_tag *)
		(((u64)ipa_term_fifo->rx_fifo.fifo_base_addr_l) |
		 ((u64)ipa_term_fifo->rx_fifo.fifo_base_addr_h << 32));

	if (node != NULL) {
		for (i = 0; i < num; i++) {
			memcpy(fifo_addr + i, node + i,
			       sizeof(struct sipa_node_description_tag));
		}
	} else {
		return FALSE;
	}

	return TRUE;
}


static u32
ipa_common_fifo_hal_get_rx_ptr(enum sipa_cmn_fifo_index id,
			       struct sipa_common_fifo_cfg_tag *cfg_base,
			       u32 *wr, u32 *rd)
{
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (rd != NULL)
		*rd = ipa_phy_get_rx_fifo_rptr(ipa_term_fifo->fifo_reg_base);
	if (wr != NULL)
		*wr = ipa_phy_get_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base);

	return TRUE;
}

static u32
ipa_common_fifo_hal_get_tx_ptr(enum sipa_cmn_fifo_index id,
			       struct sipa_common_fifo_cfg_tag *cfg_base,
			       u32 *wr, u32 *rd)
{
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (rd != NULL)
		*rd = ipa_phy_get_tx_fifo_rptr(ipa_term_fifo->fifo_reg_base);
	if (wr != NULL)
		*wr = ipa_phy_get_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base);

	return TRUE;
}

static u32
ipa_common_fifo_hal_get_filled_depth(enum sipa_cmn_fifo_index id,
				     struct sipa_common_fifo_cfg_tag *cfg_base,
				     u32 *rx_filled, u32 *tx_filled)
{
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	if (tx_filled != NULL)
		*tx_filled = ipa_phy_get_tx_fifo_filled_depth(
			ipa_term_fifo->fifo_reg_base);
	if (rx_filled != NULL)
		*rx_filled = ipa_phy_get_rx_fifo_filled_depth(
			ipa_term_fifo->fifo_reg_base);

	return TRUE;
}

static u32
ipa_common_fifo_hal_get_tx_full_status(enum sipa_cmn_fifo_index id,
				       struct sipa_common_fifo_cfg_tag *cfg_base)
{
	u32 ret = 0;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ret = ipa_phy_get_tx_fifo_full_status(ipa_term_fifo->fifo_reg_base);

	return ret;
}

static u32
ipa_common_fifo_hal_get_tx_empty_status(enum sipa_cmn_fifo_index id,
					struct sipa_common_fifo_cfg_tag *cfg_base)
{
	u32 ret = 0;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ret = ipa_phy_get_tx_fifo_empty_status(ipa_term_fifo->fifo_reg_base);

	return ret;
}

static u32
ipa_common_fifo_hal_get_rx_full_status(enum sipa_cmn_fifo_index id,
				       struct sipa_common_fifo_cfg_tag *cfg_base)
{
	u32 ret = 0;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ret = ipa_phy_get_rx_fifo_full_status(ipa_term_fifo->fifo_reg_base);

	return ret;
}

static u32
ipa_common_fifo_hal_get_rx_empty_status(enum sipa_cmn_fifo_index id,
					struct sipa_common_fifo_cfg_tag *cfg_base)
{
	u32 ret = 0;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	ret = ipa_phy_get_rx_fifo_empty_status(ipa_term_fifo->fifo_reg_base);

	return ret;
}

static bool
ipa_common_fifo_set_rx_fifo_wptr(enum sipa_cmn_fifo_index id,
				 struct sipa_common_fifo_cfg_tag *cfg_base,
				 u32 wptr)
{
	u32 ret;
	u32 rx_wptr;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return false;
	}

	rx_wptr = ipa_phy_get_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base);

	if (wptr != rx_wptr) {
		wptr = wptr & PTR_MASK(ipa_term_fifo->rx_fifo.depth);
		ipa_term_fifo->rx_fifo.wr = wptr;
		ret = ipa_phy_update_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base,
						  wptr);
		if (!ret) {
			pr_err("fifo id = %d update rx fifo wptr = 0x%x failed !!!",
			       id, wptr);
			return false;
		}
	}

	return true;
}

static bool
ipa_common_fifo_set_tx_fifo_wptr(enum sipa_cmn_fifo_index id,
				 struct sipa_common_fifo_cfg_tag *cfg_base,
				 u32 wptr)
{
	u32 ret;
	u32 tx_wptr;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return false;
	}

	tx_wptr = ipa_phy_get_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base);

	if (wptr != tx_wptr) {
		wptr = wptr & PTR_MASK(ipa_term_fifo->rx_fifo.depth);
		ipa_term_fifo->tx_fifo.wr = wptr;
		ret = ipa_phy_update_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base,
						  wptr);
		if (!ret) {
			pr_err("fifo id = %d update tx fifo wptr = 0x%x failed !!!",
			       id, wptr);
			return false;
		}
	}

	return true;
}

static u32
ipa_common_fifo_set_rx_tx_fifo_wr_rd_ptr(enum sipa_cmn_fifo_index id,
					 struct sipa_common_fifo_cfg_tag *cfg_base,
					 u32 rx_rd, u32 rx_wr,
					 u32 tx_rd, u32 tx_wr)
{
	u32 ret = 0, ret1 = TRUE;
	u32 rx_rptr = 0, rx_wptr = 0;
	u32 tx_rptr = 0, tx_wptr = 0;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return FALSE;
	}

	tx_wptr = ipa_phy_get_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base);
	tx_rptr = ipa_phy_get_tx_fifo_rptr(ipa_term_fifo->fifo_reg_base);
	rx_wptr = ipa_phy_get_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base);
	rx_rptr = ipa_phy_get_rx_fifo_rptr(ipa_term_fifo->fifo_reg_base);

	if (rx_rd != rx_rptr) {
		rx_rd = rx_rd & PTR_MASK(ipa_term_fifo->rx_fifo.depth);
		ipa_term_fifo->rx_fifo.rd = rx_rd;
		ret = ipa_phy_update_rx_fifo_rptr(ipa_term_fifo->fifo_reg_base,
						  rx_rd);
		if (!ret) {
			ret1 = FALSE;
			pr_err("update rx fifo rptr = 0x%x failed !!!", rx_rd);
		}
	}

	if (rx_wr != rx_wptr) {
		rx_wr = rx_wr & PTR_MASK(ipa_term_fifo->rx_fifo.depth);
		ipa_term_fifo->rx_fifo.wr = rx_wr;
		ret = ipa_phy_update_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base,
						  rx_wr);
		if (!ret) {
			ret1 = FALSE;
			pr_err("update rx fifo wptr = 0x%x failed !!!",	rx_wr);
		}
	}

	if (tx_rd != tx_rptr) {
		tx_rd = tx_rd & PTR_MASK(ipa_term_fifo->rx_fifo.depth);
		ipa_term_fifo->tx_fifo.rd = tx_rd;
		ret = ipa_phy_update_tx_fifo_rptr(ipa_term_fifo->fifo_reg_base,
						  tx_rd);
		if (!ret) {
			ret1 = FALSE;
			pr_err("update tx fifo rptr = 0x%x failed !!!", tx_rd);
		}
	}

	if (tx_wr != tx_wptr) {
		tx_wr = tx_wr & PTR_MASK(ipa_term_fifo->rx_fifo.depth);
		ipa_term_fifo->tx_fifo.wr = tx_wr;
		ret = ipa_phy_update_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base,
						  tx_wr);
		if (!ret) {
			ret1 = FALSE;
			pr_err("update tx fifo wptr = 0x%x failed !!!", tx_wr);
		}
	}

	return ret1;
}

static u32
ipa_common_fifo_ctrl_receive(enum sipa_cmn_fifo_index id,
			     struct sipa_common_fifo_cfg_tag *cfg_base,
			     bool stop)
{
	u32 ret;
	struct sipa_common_fifo_cfg_tag *ipa_term_fifo;

	if (likely(id < SIPA_FIFO_MAX)) {
		ipa_term_fifo = cfg_base + id;
	} else {
		pr_err("don't have this id %d\n", id);
		return 0;
	}

	if (stop)
		ret = ipa_phy_stop_receive(ipa_term_fifo->fifo_reg_base);
	else
		ret = ipa_phy_clear_stop_receive(ipa_term_fifo->fifo_reg_base);

	return ret;
}

static struct sipa_node_description_tag *
ipa_get_tx_fifo_node_pointer(enum sipa_cmn_fifo_index id,
			     struct sipa_common_fifo_cfg_tag *cfg_base,
			     u32 index)
{
	u32 tmp;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;
	struct sipa_node_description_tag *node;

	if (unlikely(id >= SIPA_FIFO_MAX))
		return NULL;

	fifo_cfg = cfg_base + id;
	node = (struct sipa_node_description_tag *)
		fifo_cfg->tx_fifo.virtual_addr;

	if (unlikely(!node))
		return NULL;

	tmp = (fifo_cfg->tx_fifo.rd + index) & (fifo_cfg->tx_fifo.depth - 1);

	return node + tmp;
}

static int ipa_reclaim_cmn_fifo(enum sipa_cmn_fifo_index id,
				struct sipa_common_fifo_cfg_tag *cfg_base)
{
	int ret = 0;
	u32 tx_rptr, tx_wptr, rx_wptr, rx_rptr;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;

	if (unlikely(id >= SIPA_FIFO_MAX))
		return -EINVAL;

	fifo_cfg = cfg_base + id;
	if (!fifo_cfg->is_pam) {
		pr_err("sipa reclaim fifo id is err\n");
		return -EINVAL;
	}
	rx_rptr = ipa_phy_get_rx_fifo_rptr(fifo_cfg->fifo_reg_base);
	rx_wptr = ipa_phy_get_rx_fifo_wptr(fifo_cfg->fifo_reg_base);
	tx_rptr = ipa_phy_get_tx_fifo_rptr(fifo_cfg->fifo_reg_base);
	tx_wptr = ipa_phy_get_tx_fifo_wptr(fifo_cfg->fifo_reg_base);

	if (tx_wptr != tx_rptr)
		ret = ipa_reclaim_tx_fifo_unread_node_desc(fifo_cfg,
							   tx_rptr, tx_wptr,
							   rx_rptr, rx_wptr);
	else if (tx_wptr != rx_wptr)
		ret = ipa_reclaim_unfree_node_desc(fifo_cfg,
						   tx_rptr, tx_wptr,
						   rx_rptr, rx_wptr);

	return ret;
}

static int ipa_set_tx_fifo_rptr(enum sipa_cmn_fifo_index id,
				struct sipa_common_fifo_cfg_tag *cfg_base,
				u32 tx_rd)
{
	int ret;
	struct sipa_common_fifo_cfg_tag *fifo_cfg;

	if (unlikely(id >= SIPA_FIFO_MAX))
		return -EINVAL;

	fifo_cfg = cfg_base + id;
	fifo_cfg->tx_fifo.rd = (fifo_cfg->tx_fifo.rd + tx_rd) &
		PTR_MASK(fifo_cfg->tx_fifo.depth);
	ret = ipa_phy_update_tx_fifo_rptr(fifo_cfg->fifo_reg_base,
					  fifo_cfg->tx_fifo.rd);

	if (!ret) {
		pr_err("update tx fifo rptr fail !!!\n");
		return -EINVAL;
	}

	return 0;
}

u32 sipa_fifo_ops_init(struct sipa_hal_fifo_ops *ops)
{
	ops->open = ipa_common_fifo_hal_open;

	ops->close = ipa_common_fifo_hal_close;

	ops->enable_remote_flowctrl_interrupt =
		ipa_common_fifo_hal_enable_remote_flowctrl_interrupt;

	ops->enable_local_flowctrl_interrupt =
		ipa_common_fifo_hal_enable_local_flowctrl_interrupt;

	ops->get_left_cnt = ipa_common_fifo_hal_get_left_cnt;

	ops->put_node_to_rx_fifo =
		ipa_common_fifo_hal_put_node_to_rx_fifo;

	ops->put_node_to_tx_fifo =
		ipa_common_fifo_hal_put_node_to_tx_fifo;

	ops->recv_node_from_tx_fifo =
		ipa_common_fifo_hal_recv_node_from_tx_fifo;

	ops->get_node_from_rx_fifo =
		ipa_common_fifo_hal_get_node_from_rx_fifo;

	ops->rx_fill = ipa_common_fifo_hal_rx_fill;

	ops->tx_fill = ipa_common_fifo_hal_tx_fill;

	ops->set_interrupt_drop_packet =
		ipa_common_fifo_hal_set_interrupt_drop_packet;

	ops->set_interrupt_error_code =
		ipa_common_fifo_hal_set_interrupt_error_code;

	ops->set_interrupt_threshold =
		ipa_common_fifo_hal_set_interrupt_threshold;

	ops->set_interrupt_timeout =
		ipa_common_fifo_hal_set_interrupt_timeout;

	ops->set_hw_interrupt_threshold =
		ipa_common_fifo_hal_set_hw_interrupt_threshold;

	ops->set_hw_interrupt_timeout =
		ipa_common_fifo_hal_set_hw_interrupt_timeout;

	ops->set_interrupt_intr =
		ipa_common_fifo_hal_set_interrupt_intr;

	ops->set_interrupt_txfifo_full =
		ipa_common_fifo_hal_set_interrupt_txfifo_full;

	ops->set_interrupt_txfifo_overflow =
		ipa_common_fifo_hal_set_interrupt_txfifo_overflow;

	ops->set_rx_depth =
		ipa_common_fifo_hal_set_rx_depth;

	ops->set_tx_depth =
		ipa_common_fifo_hal_set_tx_depth;

	ops->get_rx_depth =
		ipa_common_fifo_hal_get_rx_depth;

	ops->get_tx_depth =
		ipa_common_fifo_hal_get_tx_depth;

	ops->get_tx_fifo_wr_rd_ptr =
		ipa_common_fifo_hal_get_tx_ptr;

	ops->get_rx_fifo_wr_rd_ptr =
		ipa_common_fifo_hal_get_rx_ptr;

	ops->get_filled_depth =
		ipa_common_fifo_hal_get_filled_depth;

	ops->reset = ipa_common_fifo_hal_reset_fifo;

	ops->get_tx_empty_status =
		ipa_common_fifo_hal_get_tx_empty_status;

	ops->get_tx_full_status =
		ipa_common_fifo_hal_get_tx_full_status;

	ops->get_rx_empty_status =
		ipa_common_fifo_hal_get_rx_empty_status;

	ops->get_rx_full_status =
		ipa_common_fifo_hal_get_rx_full_status;

	ops->set_rx_tx_fifo_wr_rd_ptr =
		ipa_common_fifo_set_rx_tx_fifo_wr_rd_ptr;

	ops->set_tx_fifo_wptr =
		ipa_common_fifo_set_tx_fifo_wptr;

	ops->set_rx_fifo_wptr =
		ipa_common_fifo_set_rx_fifo_wptr;

	ops->set_cur_dst_term =
		ipa_common_fifo_hal_set_src_dst_term;

	ops->ctrl_receive =
		ipa_common_fifo_ctrl_receive;

	ops->update_tx_fifo_rptr = ipa_set_tx_fifo_rptr;
	ops->get_tx_fifo_node = ipa_get_tx_fifo_node_pointer;
	ops->reclaim_node_desc = ipa_reclaim_cmn_fifo;

	return TRUE;
}

