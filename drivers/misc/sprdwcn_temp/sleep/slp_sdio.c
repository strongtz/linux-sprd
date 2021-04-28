#include <linux/err.h>
#include <linux/export.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include <misc/wcn_bus.h>

#include "sdio_int.h"
#include "slp_mgr.h"
#include "slp_sdio.h"
#include "wcn_glb.h"
#include "slp_dbg.h"

int slp_allow_sleep(void)
{
	union CP_SLP_CTL_REG reg_slp_ctl = {0};

	reg_slp_ctl.bit.cp_slp_ctl = 1;
	sprdwcn_bus_aon_writeb(REG_CP_SLP_CTL, reg_slp_ctl.reg);

	sdio_ap_int_cp0(ALLOW_CP_SLP);
	/* make SLP_CTL high_level keep 2 cycle of 32khz */
	udelay(65);
	return 0;
}

static void req_slp_isr(void)
{
	struct slp_mgr_t *slp_mgr;

	slp_mgr = slp_get_info();
	mutex_lock(&(slp_mgr->drv_slp_lock));
	/* allow sleep */
	if (slp_mgr->active_module == 0) {
		WCN_INFO("allow sleep\n");
		slp_allow_sleep();

		atomic_set(&(slp_mgr->cp2_state), STAY_SLPING);
	} else {
		WCN_INFO("forbid slp module-0x%x\n",
			 slp_mgr->active_module);
	}
	mutex_unlock(&(slp_mgr->drv_slp_lock));
}

static void wakeup_ack_isr(void)
{
	struct slp_mgr_t *slp_mgr;

	slp_mgr = slp_get_info();
	if (STAY_SLPING == (atomic_read(&(slp_mgr->cp2_state)))) {
		WCN_INFO("wakeup ack\n");
		complete(&(slp_mgr->wakeup_ack_completion));
	} else
		WCN_INFO("discard wakeup ack\n");
}

int slp_pub_int_regcb(void)
{
	sdio_pub_int_regcb(WAKEUP_ACK, (PUB_INT_ISR)wakeup_ack_isr);
	sdio_pub_int_regcb(REQ_SLP, (PUB_INT_ISR)req_slp_isr);

	return 0;
}

