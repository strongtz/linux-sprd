#include <asm/irq.h>
#include <linux/sipa.h>
#include <linux/pm_wakeup.h>

#include "../sipa_hal_priv.h"
#include "sipa_device.h"
#include "sipa_glb_phy.h"
#include "sipa_fifo_phy.h"

struct sipa_interrupt_table_tag ipa_int_table[] = {
	{
		.id = SIPA_FIFO_AP_IP_DL,
		.int_owner = IPA_MAP_DL_RX_INTERRUPT_MASK,
	},
	{
		.id = SIPA_FIFO_AP_ETH_DL,
		.int_owner = IPA_MAP_DL_TX_INTERRUPT_MASK,
	},
	{
		.id = SIPA_FIFO_AP_ETH_UL,
		.int_owner = IPA_MAP_UL_RX_INTERRUPT_MASK,
	},
	{
		.id = SIPA_FIFO_AP_IP_UL,
		.int_owner = IPA_MAP_UL_TX_INTERRUPT_MASK,
	},
};

static inline u32
ipa_fifo_traverse_int_bit(enum sipa_cmn_fifo_index id,
			  struct sipa_common_fifo_cfg_tag *ipa_cfg)
{
	void __iomem *fifo_base;
	u32 clr_sts = 0;
	u32 int_status = 0;

	fifo_base = ipa_cfg->fifo_reg_base;
	int_status = ipa_phy_get_fifo_all_int_sts(fifo_base);

	if (int_status & IPA_INT_EXIT_FLOW_CTRL_STS)
		clr_sts |= IPA_EXIT_FLOW_CONTROL_CLR_BIT;

	if (int_status & IPA_INT_ERRORCODE_IN_TX_FIFO_STS)
		clr_sts |= IPA_ERROR_CODE_INTR_CLR_BIT;

	if (int_status & IPA_INT_ENTER_FLOW_CTRL_STS)
		clr_sts |= IPA_ENTRY_FLOW_CONTROL_CLR_BIT;

	if (int_status & IPA_INT_INTR_BIT_STS)
		clr_sts |= IPA_TX_FIFO_INTR_CLR_BIT;

	if (int_status & IPA_INT_TX_FIFO_THRESHOLD_SW_STS)
		clr_sts |= IPA_TX_FIFO_THRESHOLD_CLR_BIT;

	if (int_status & IPA_INT_DELAY_TIMER_STS)
		clr_sts |= IPA_TX_FIFO_TIMER_CLR_BIT;

	if (int_status & IPA_INT_DROP_PACKT_OCCUR)
		clr_sts |= IPA_DROP_PACKET_INTR_CLR_BIT;

	if (int_status & IPA_INT_TXFIFO_OVERFLOW_STS)
		clr_sts |= IPA_TX_FIFO_OVERFLOW_CLR_BIT;

	if (int_status & IPA_INT_TXFIFO_FULL_INT_STS)
		clr_sts |= IPA_TX_FIFO_FULL_INT_CLR_BIT;

	if (ipa_cfg->fifo_irq_callback)
		ipa_cfg->fifo_irq_callback(ipa_cfg->priv, int_status, id);
	else
		pr_debug("Don't register this fifo(%d) irq callback\n", id);

	ipa_phy_clear_int(ipa_cfg->fifo_reg_base, clr_sts);

	return TRUE;
}

static u32 ipa_fifo_irq_main_cb(enum sipa_cmn_fifo_index id,
				struct sipa_common_fifo_cfg_tag *cfg_base)
{
	struct sipa_common_fifo_cfg_tag *ipa_cfg = NULL;

	if (id < SIPA_FIFO_MAX) {
		ipa_cfg = cfg_base + id;
		if (ipa_cfg != NULL)
			ipa_fifo_traverse_int_bit(id, ipa_cfg);
	} else {
		pr_err("don't have this id\n");
	}

	return TRUE;
}

u32 sipa_int_callback_func(int evt, void *cookie)
{
	u32 i = 0, int_sts = 0;
	struct sipa_hal_context *cfg = cookie;

	int_sts = ipa_phy_get_int_status(cfg->phy_virt_res.glb_base);

	for (i = 0; i < ARRAY_SIZE(ipa_int_table); i++) {
		if (int_sts & ipa_int_table[i].int_owner) {
			ipa_fifo_irq_main_cb(ipa_int_table[i].id,
					     cfg->cmn_fifo_cfg);
		}
	}

	if (!cfg->dev->power.wakeup->active)
		pm_wakeup_dev_event(cfg->dev, 500, true);

	return TRUE;
}
EXPORT_SYMBOL(sipa_int_callback_func);
