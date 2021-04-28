/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _SPRD_DMA_H_
#define _SPRD_DMA_H_

#define SPRD_DMA_REQ_SHIFT	8
#define SPRD_DMA_TRG_MODE_SHIFT	16
#define SPRD_DMA_CHN_MODE_SHIFT	24
#define SPRD_DMA_FLAGS(chn_mode, trg_mode, req_mode, int_type) \
	((chn_mode) << SPRD_DMA_CHN_MODE_SHIFT | \
	(trg_mode) << SPRD_DMA_TRG_MODE_SHIFT | \
	(req_mode) << SPRD_DMA_REQ_SHIFT | (int_type))

/*
 * enum sprd_dma_chn_mode: define the DMA channel mode
 *
 * The Spreadtrum DMA controller supports two stage channels mode,
 * that means there are two dma channels, one for source channel,
 * and another for destination channel. Each of the channel is
 * independent, and has its own configurations, but if source
 * channel transfer is done, which will trigger destination channel
 * automatically by hardware signal.
 *
 * @SPRD_DMA_CHN_MODE_NONE: channel do not support two stage mode
 * @SPRD_DMA_SRC_CHN0: channel used as source channel 0
 * @SPRD_DMA_SRC_CHN1: channel used as source channel 1
 * @SPRD_DMA_DST_CHN0: channel used as destination channel 0
 * @SPRD_DMA_DST_CHN1: channel used as destination channel 1
 *
 * There are two source channels and two destination channels.
 * Source channel request mode means the channel is used as the
 * triggering channel in dma two stage channels mode.
 * Destination channel request mode means the channel is used as the
 * trigged channel in dma two stage channels mode.
 */
enum sprd_dma_chn_mode {
	SPRD_DMA_CHN_MODE_NONE,
	SPRD_DMA_SRC_CHN0,
	SPRD_DMA_SRC_CHN1,
	SPRD_DMA_DST_CHN0,
	SPRD_DMA_DST_CHN1,
};

/*
 * enum sprd_dma_trg_mode: define the DMA channel trigger mode
 *
 * Refer to "enum sprd_dma_chn_mode" of the description of DMA two
 * stage channels mode.
 *
 * @SPRD_DMA_NO_TRG: channel do not support two stage mode
 * @SPRD_DMA_FRAG_DONE_TRG: if source channel's fragment request is done,
 * it will trigger destination channel
 * @SPRD_DMA_BLOCK_DONE_TRG: if source channel's block request is done,
 * it will trigger destination channel
 * @SPRD_DMA_TRANS_DONE_TRG: if source channel's transfer request is done,
 * it will trigger destination channel
 * @SPRD_DMA_LIST_DONE_TRG: if source channel's link-list request is done,
 * it will trigger destination channel
 */
enum sprd_dma_trg_mode {
	SPRD_DMA_NO_TRG,
	SPRD_DMA_FRAG_DONE_TRG,
	SPRD_DMA_BLOCK_DONE_TRG,
	SPRD_DMA_TRANS_DONE_TRG,
	SPRD_DMA_LIST_DONE_TRG,
};

/*
 * enum sprd_dma_req_mode: define the DMA request mode
 * @SPRD_DMA_FRAG_REQ: fragment request mode
 * @SPRD_DMA_BLK_REQ: block request mode
 * @SPRD_DMA_TRANS_REQ: transaction request mode
 * @SPRD_DMA_LIST_REQ: link-list request mode
 *
 * We have 4 types request mode: fragment mode, block mode, transaction mode
 * and linklist mode. One transaction can contain several blocks, one block can
 * contain several fragments. Link-list mode means we can save several DMA
 * configuration into one reserved memory, then DMA can fetch each DMA
 * configuration automatically to start transfer.
 */
enum sprd_dma_req_mode {
	SPRD_DMA_FRAG_REQ,
	SPRD_DMA_BLK_REQ,
	SPRD_DMA_TRANS_REQ,
	SPRD_DMA_LIST_REQ,
};

/*
 * enum sprd_dma_int_type: define the DMA interrupt type
 * @SPRD_DMA_NO_INT: do not need generate DMA interrupts.
 * @SPRD_DMA_FRAG_INT: fragment done interrupt when one fragment request
 * is done.
 * @SPRD_DMA_BLK_INT: block done interrupt when one block request is done.
 * @SPRD_DMA_BLK_FRAG_INT: block and fragment interrupt when one fragment
 * or one block request is done.
 * @SPRD_DMA_TRANS_INT: tansaction done interrupt when one transaction
 * request is done.
 * @SPRD_DMA_TRANS_FRAG_INT: transaction and fragment interrupt when one
 * transaction request or fragment request is done.
 * @SPRD_DMA_TRANS_BLK_INT: transaction and block interrupt when one
 * transaction request or block request is done.
 * @SPRD_DMA_LIST_INT: link-list done interrupt when one link-list request
 * is done.
 * @SPRD_DMA_CFGERR_INT: configure error interrupt when configuration is
 * incorrect.
 * @SPRD_DMA_SRC_CHN0_INT: interrupt occurred when source channel0
 * transfer is done.
 * @SPRD_DMA_SRC_CHN1_INT: interrupt occurred when source channel1
 * transfer is done.
 * @SPRD_DMA_DST_CHN0_INT: interrupt occurred when destination channel0
 * transfer is done.
 * @SPRD_DMA_DST_CHN1_INT: interrupt occurred when destination channel1
 * transfer is done.
 */
enum sprd_dma_int_type {
	SPRD_DMA_NO_INT,
	SPRD_DMA_FRAG_INT,
	SPRD_DMA_BLK_INT,
	SPRD_DMA_BLK_FRAG_INT,
	SPRD_DMA_TRANS_INT,
	SPRD_DMA_TRANS_FRAG_INT,
	SPRD_DMA_TRANS_BLK_INT,
	SPRD_DMA_LIST_INT,
	SPRD_DMA_CFGERR_INT,
	SPRD_DMA_SRC_CHN0_INT,
	SPRD_DMA_SRC_CHN1_INT,
	SPRD_DMA_DST_CHN0_INT,
	SPRD_DMA_DST_CHN1_INT,
};

/*
 * struct sprd_dma_linklist - DMA link-list address structure
 * @virt_addr: link-list virtual address to configure link-list node
 * @phy_addr: link-list physical address to link DMA transfer
 * @wrap_ptr: wrap address for wrap mode, if transfer address is equal
 * to the wrap address, the current destination address will wrap to
 * the destination initial configuration address.
 *
 * The Spreadtrum DMA controller supports the link-list mode, that means slaves
 * can supply several groups configurations (each configuration represents one
 * DMA transfer) saved in memory, and DMA controller will link these groups
 * configurations by writing the physical address of each configuration into the
 * link-list register.
 *
 * Just as shown below, the link-list pointer register will be pointed to the
 * physical address of 'configuration 1', and the 'configuration 1' link-list
 * pointer will be pointed to 'configuration 2', and so on.
 * Once trigger the DMA transfer, the DMA controller will load 'configuration
 * 1' to its registers automatically, after 'configuration 1' transaction is
 * done, DMA controller will load 'configuration 2' automatically, until all
 * DMA transactions are done.
 *
 * Note: The last link-list pointer should point to the physical address
 * of 'configuration 1', which can avoid DMA controller loads incorrect
 * configuration when the last configuration transaction is done.
 *
 *     DMA controller                    linklist memory
 * ======================             -----------------------
 *|                      |           |    configuration 1    |<---
 *|   DMA controller     |   ------->|                       |   |
 *|                      |   |       |                       |   |
 *|                      |   |       |                       |   |
 *|                      |   |       |                       |   |
 *| linklist pointer reg |----   ----|    linklist pointer   |   |
 * ======================        |    -----------------------    |
 *                               |                               |
 *                               |    -----------------------    |
 *                               |   |    configuration 2    |   |
 *                               --->|                       |   |
 *                                   |                       |   |
 *                                   |                       |   |
 *                                   |                       |   |
 *                               ----|    linklist pointer   |   |
 *                               |    -----------------------    |
 *                               |                               |
 *                               |    -----------------------    |
 *                               |   |    configuration 3    |   |
 *                               --->|                       |   |
 *                                   |                       |   |
 *                                   |           .           |   |
 *                                               .               |
 *                                               .               |
 *                                               .               |
 *                               |               .               |
 *                               |    -----------------------    |
 *                               |   |    configuration n    |   |
 *                               --->|                       |   |
 *                                   |                       |   |
 *                                   |                       |   |
 *                                   |                       |   |
 *                                   |    linklist pointer   |----
 *                                    -----------------------
 *
 * To support the link-list mode, DMA slaves should allocate one segment memory
 * from always-on IRAM or dma coherent memory to store these groups of DMA
 * configuration, and pass the virtual and physical address to DMA controller.
 */
struct sprd_dma_linklist {
	unsigned long virt_addr;
	phys_addr_t phy_addr;
	phys_addr_t wrap_ptr;
};

#endif
