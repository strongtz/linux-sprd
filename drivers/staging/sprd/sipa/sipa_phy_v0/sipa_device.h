#ifndef IPA_DEVICE_H_
#define IPA_DEVICE_H_

#include <linux/sipa.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>

#include "../sipa_hal_priv.h"
#include "sipa_glb_phy.h"
#include "sipa_fifo_phy.h"

#define PTR_MASK(depth) (depth | (depth - 1))

static inline u32
ipa_get_pkt_from_rx_fifo(struct sipa_common_fifo_cfg_tag *fifo_cfg,
			 struct sipa_node_description_tag *fill_node,
			 u32 num)
{
	u32 left_cnt = 0, i = 0;
	u32 ret = 0, index = 0;
	struct sipa_node_description_tag *node;

	if (fill_node == NULL) {
		pr_err("fill node is NULL\n");
		return FALSE;
	}

	node = (struct sipa_node_description_tag *)
		   fifo_cfg->rx_fifo.virtual_addr;

	if (ipa_phy_get_rx_fifo_empty_status(fifo_cfg->fifo_reg_base))
		return FALSE;

	left_cnt = ipa_phy_get_rx_fifo_filled_depth(fifo_cfg->fifo_reg_base);

	if (left_cnt < num) {
		pr_info("%s %d only have %d node free\n",
			fifo_cfg->fifo_name,
			fifo_cfg->fifo_id, left_cnt);
		num = left_cnt;
	}

	for (i = 0; i < num; i++) {
		index = (fifo_cfg->rx_fifo.rd + i) &
				(fifo_cfg->rx_fifo.depth - 1);
		memcpy(fill_node + i, node + index,
		       sizeof(struct sipa_node_description_tag));
	}

	smp_wmb();
	fifo_cfg->rx_fifo.rd = (fifo_cfg->rx_fifo.rd + num) &
		PTR_MASK(fifo_cfg->rx_fifo.depth);
	ret = ipa_phy_update_rx_fifo_rptr(fifo_cfg->fifo_reg_base,
					  fifo_cfg->rx_fifo.rd);
	if (ret == FALSE)
		pr_err("ipa_phy_update_rx_fifo_rptr fail\n");

	return num;
}

static inline u32
ipa_put_pkt_to_rx_fifo(struct sipa_common_fifo_cfg_tag *fifo_cfg,
		       u32 force_intr,
		       struct sipa_node_description_tag *fill_node,
		       u32 num)
{
	u32 i = 0, ret = 0, index = 0, left_cnt = 0;
	struct sipa_node_description_tag *node;

	if (fill_node == NULL) {
		pr_err("fill node is NULL\n");
		return FALSE;
	}

	node = (struct sipa_node_description_tag *)
		   fifo_cfg->rx_fifo.virtual_addr;

	if (ipa_phy_get_rx_fifo_full_status(fifo_cfg->fifo_reg_base))
		return FALSE;

	left_cnt = ipa_phy_get_rx_fifo_total_depth(fifo_cfg->fifo_reg_base) -
		ipa_phy_get_rx_fifo_filled_depth(fifo_cfg->fifo_reg_base);

	if (left_cnt < num)
		num = left_cnt;
	for (i = 0; i < num; i++) {
		index = (fifo_cfg->rx_fifo.wr + i) &
			(fifo_cfg->rx_fifo.depth - 1);
		memcpy(node + index, fill_node + i,
		       sizeof(struct sipa_node_description_tag));
	}

	smp_wmb();
	fifo_cfg->rx_fifo.wr = (fifo_cfg->rx_fifo.wr + num) &
		PTR_MASK(fifo_cfg->rx_fifo.depth);
	ret = ipa_phy_update_rx_fifo_wptr(fifo_cfg->fifo_reg_base,
					  fifo_cfg->rx_fifo.wr);

	if (ret == FALSE)
		pr_err("ipa_phy_update_rx_fifo_rptr fail\n");

	return num;
}

static inline u32
ipa_recv_pkt_from_tx_fifo(struct sipa_common_fifo_cfg_tag *fifo_cfg,
			  struct sipa_node_description_tag *fill_node,
			  u32 num)
{
	struct sipa_node_description_tag *node;
	u32 i = 0, ret = 0, index = 0, left_cnt = 0;

	if (fill_node == NULL) {
		pr_err("fill node is NULL\n");
		return FALSE;
	}

	node = (struct sipa_node_description_tag *)
		   fifo_cfg->tx_fifo.virtual_addr;

	if (ipa_phy_get_tx_fifo_empty_status(fifo_cfg->fifo_reg_base))
		return FALSE;

	left_cnt = ipa_phy_get_tx_fifo_filled_depth(fifo_cfg->fifo_reg_base);
	if (left_cnt < num) {
		pr_info("fifo_id = %d only have %d nodes\n",
			fifo_cfg->fifo_id, left_cnt);
		num = left_cnt;
	}

	for (i = 0; i < num; i++) {
		index = (fifo_cfg->tx_fifo.rd + i) &
				(fifo_cfg->tx_fifo.depth - 1);
		memcpy(fill_node + i, node + index,
		       sizeof(struct sipa_node_description_tag));
	}

	smp_wmb();
	fifo_cfg->tx_fifo.rd = (fifo_cfg->tx_fifo.rd + num) &
		PTR_MASK(fifo_cfg->tx_fifo.depth);
	ret = ipa_phy_update_tx_fifo_rptr(fifo_cfg->fifo_reg_base,
					  fifo_cfg->tx_fifo.rd);

	if (ret == FALSE)
		pr_err("update tx fifo rptr fail !!!\n");

	return num;
}

static inline u32
ipa_put_pkt_to_tx_fifo(struct sipa_common_fifo_cfg_tag *fifo_cfg,
		       struct sipa_node_description_tag *fill_node, u32 num)
{
	u32 left_cnt = 0;
	u32 i = 0, ret = 0, index = 0;
	struct sipa_node_description_tag *node;

	node = (struct sipa_node_description_tag *)
		   fifo_cfg->tx_fifo.virtual_addr;

	if (ipa_phy_get_tx_fifo_full_status(fifo_cfg->fifo_reg_base))
		return FALSE;

	left_cnt = ipa_phy_get_tx_fifo_total_depth(fifo_cfg->fifo_reg_base) -
		ipa_phy_get_tx_fifo_filled_depth(fifo_cfg->fifo_reg_base);

	if (num > left_cnt) {
		pr_info("fifo_id = %d don't have enough space\n",
			fifo_cfg->fifo_id);
		num = left_cnt;
	}

	for (i = 0; i < num; i++) {
		index = (fifo_cfg->tx_fifo.wr + i) &
				(fifo_cfg->tx_fifo.depth - 1);
		memcpy(node + index, fill_node + i,
		       sizeof(struct sipa_node_description_tag));
	}

	smp_wmb();
	fifo_cfg->tx_fifo.wr = (fifo_cfg->tx_fifo.wr + num) &
		PTR_MASK(fifo_cfg->tx_fifo.depth);
	ret = ipa_phy_update_tx_fifo_wptr(fifo_cfg->fifo_reg_base,
					  fifo_cfg->tx_fifo.wr);

	if (ret == FALSE)
		pr_err("update tx fifo rptr fail !!!\n");

	return num;
}
#endif
