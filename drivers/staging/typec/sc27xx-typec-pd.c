// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Spreadtrum Communications Inc.

#include <linux/extcon-provider.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/usb/typec.h>
#include <linux/usb/tcpm.h>
#include <linux/usb/pd.h>

/* Typec controller registers definition */
#define SC27XX_TYPEC_EN			0x0
#define SC27XX_TYPEC_MODE		0x4
#define SC27XX_TYPC_PD_CFG		0x8
#define SC27XX_TYPEC_INT_EN		0xc
#define SC27XX_TYPEC_INT_CLR		0x10
#define SC27XX_TYPEC_INT_RAW		0x14
#define SC27XX_TYPEC_INT_MASK		0x18
#define SC27XX_TYPEC_STATUS		0x1c
#define SC27XX_TYPEC_SW_CFG		0x54
#define SC27XX_TYPEC_WC_REG		0x58
#define SC27XX_TYPEC_DBG1		0X60
#define SC27XX_TYPEC_DBG2		0X64
#define SC27XX_TYPEC_DBG3		0X68

/* Bits definitions for SC27XX_TYPEC_EN	register */
#define SC27XX_TYPEC_MODULE_EN		BIT(0)
#define SC27XX_TYPEC_TOGGLE_SLEEP_EN	BIT(1)
#define SC27XX_TYPEC_TOGGLE_ROLE_POLARITE	BIT(5)
#define SC27XX_TYPEC_TRY_SNK_EN		BIT(8)
#define SC27XX_TYPEC_TRY_SRC_EN		BIT(7)

/* Bits definitions for SC27XX_TYPEC_MODE register */
#define SC27XX_TYPEC_MODULE_CFG(x)	((x) & GENMASK(1, 0))
#define SC27XX_TYPEC_RP_LEVEL(x)	(((x) << 2) & GENMASK(3, 2))
#define SC27XX_TYPEC_SRC_EN		BIT(6)
#define SC27XX_TYPEC_TRY_ROLE_CNT(x)	(((x) << 8) & GENMASK(11, 8))

/* Bits definitions for SC27XX_TYPC_PD_CFG register */
#define SC27XX_TYPEC_PD_SUPPORT		BIT(0)
#define SC27XX_TYPEC_PD_CONSTRACT	BIT(6)
#define SC27XX_TYPEC_PD_NO_CHEK_DETACH	BIT(9)
#define SC27XX_TYPEC_SW_FORCE_CC(x)	(((x) << 10) & GENMASK(11, 10))
#define SC27XX_TYPEC_VCCON_LDO_RDY	BIT(12)
#define SC27XX_TYPEC_VCCON_LDO_EN	BIT(13)

/* Bits definitions for SC27XX_TYPEC_INT_EN register */
#define SC27XX_RP_CHANGE_INT_EN	BIT(7)
#define SC27XX_DETACH_INT_EN	BIT(1)
#define SC27XX_ATTACH_INT_EN	BIT(0)

/* Bits definitions for SC27XX_TYPEC_INT_MASK register */
#define SC27XX_TYPEC_RP_CHANGE_INT_MASK	BIT(7)
#define SC27XX_TYPEC_UNATTACH_INT_MASK	BIT(1)
#define SC27XX_TYPEC_ATTACH_INT_MASK	BIT(0)

/* Bits definitions for SC27XX_TYPEC_STATUS register */
#define SC27XX_TYPEC_CURRENT_STATUS	GENMASK(4, 0)
#define SC27XX_TYPEC_FINAL_SWITCH	BIT(5)
#define SC27XX_TYPEC_VBUS_CL(x)		(((x) & GENMASK(7, 6)) >> 6)
#define SC27XX_TYPEC_PLUGIN_CABLE	GENMASK(14, 12)
#define SC27XX_TYPEC_NEED_VCCON_LDO	BIT(15)

/* Bits definitions for SC27XX_TYPEC_SW_CFG register */
#define SC27XX_TYPEC_SW_SWITCH(x)	(((x) << 10) & GENMASK(11, 10))

/* Bits definitions for SC27XX_TYPEC_WC_REG register */
#define SC27XX_TYPEC_PD_PR_SWAP		BIT(2)
#define SC27XX_TYPEC_PD_VCCON_SWAP	BIT(1)
#define SC27XX_TYPEC_TRY_FAIL_CNT_CLR	BIT(0)

/* Bits definitions for SC27XX_TYPEC_DBG1 register */
#define SC27XX_TYPEC_VBUS_OK		BIT(8)
#define SC27XX_TYPEC_CONN_CC		BIT(9)

/* PD controller registers definition */

#define SC27XX_PD_TX_BUF		0x0
#define SC27XX_PD_RX_BUF		0x4
#define SC27XX_PD_HEAD_CFG		0x8
#define SC27XX_PD_CTRL			0xc
#define SC27XX_PD_CFG0			0x10
#define SC27XX_PD_CFG1			0x14
#define SC27XX_PD_MESG_ID_CFG		0x18
#define SC27XX_PD_STS0			0x1c
#define SC27XX_PD_STS1			0x20
#define SC27XX_INT_STS			0x24
#define SC27XX_INT_FLG			0x28
#define SC27XX_INT_CLR			0x2c
#define SC27XX_INT_EN			0x30

/* Bits definitions for SC27XX_PD_HEAD_CFG register */
#define SC27XX_PD_EXTHEAD		BIT(15)
#define SC27XX_PD_NUM_DO(x)		(((x) << 12) & GENMASK(14, 12))
#define SC27XX_PD_MESS_ID(x)		(((x) << 9) & GENMASK(11, 9))
#define SC27XX_PD_POWER_ROLE		BIT(8)
#define SC27XX_PD_SPEC_REV(x)		(((x) << 6) & GENMASK(7, 6))
#define SC27XX_PD_DATA_ROLE		BIT(5)
#define SC27XX_PD_MESSAGE_TYPE(x)	((x) & GENMASK(4, 0))

/* Bits definitions for SC27XX_PD_CTRL register */
#define SC27XX_PD_TX_START		BIT(0)
#define SC27XX_PD_HARD_RESET		BIT(2)
#define SC27XX_PD_TX_FLASH		BIT(3)
#define SC27XX_PD_RX_FLASH		BIT(4)
#define SC27XX_PD_FAST_START		BIT(6)
#define SC27XX_PD_CABLE_RESET		BIT(7)
#define SC27XX_PD_RX_ID_CLR		BIT(8)
#define SC27XX_PD_RP_SINKTXNG_CLR	BIT(9)

/* Bits definitions for SC27XX_PD_CFG0 register */
#define SC27XX_PD_RP_CONTROL(x)		((x) & GENMASK(1, 0))
#define SC27XX_PD_SINK_RP		GENMASK(3, 2)
#define SC27XX_PD_EN_SOP1_TX		BIT(6)
#define SC27XX_PD_EN_SOP2_TX		BIT(7)
#define SC27XX_PD_EN_SOP		BIT(8)
#define SC27XX_PD_SRC_SINK_MODE		BIT(9)
#define SC27XX_PD_CTL_EN		BIT(10)

/* Bits definitions for SC27XX_PD_CFG1 register */
#define SC27XX_PD_AUTO_RETRY		BIT(0)
#define SC27XX_PD_RETRY(x)		(((x) << 1) & GENMASK(2, 1))
#define SC27XX_PD_HEADER_REG_EN		BIT(3)
#define SC27XX_PD_EN_SOP1_RX		BIT(6)
#define SC27XX_PD_EN_SOP2_RX		BIT(7)
#define SC27XX_PD_EN_RX			BIT(8)
#define SC27XX_PD_RX_AUTO_GOOD_CRC	BIT(9)
#define SC27XX_PD_FRS_DETECT_EN		BIT(10)
#define SC27XX_PD_TX_AUTO_GOOD_CRC	BIT(11)

/* Bits definitions for SC27XX_PD_MESG_ID_CFG register */
#define SC27XX_PD_MESS_ID_TX(x)		((x) & GENMASK(2, 0))
#define SC27XX_PD_MESS_ID_RX(x)		(((x) << 4) & GENMASK(6, 4))

/* Bits definitions for SC27XX_INT_FLG register */
#define SC27XX_PD_HARD_RST_FLAG	BIT(0)
#define SC27XX_PD_CABLE_RST_FLAG	BIT(1)
#define SC27XX_PD_SOFT_RST_FLAG	BIT(2)
#define SC27XX_PD_PS_RDY_FLAG		BIT(3)
#define SC27XX_PD_PKG_RV_FLAG	BIT(4)
#define SC27XX_PD_TX_OK_FLAG		BIT(5)
#define SC27XX_PD_TX_ERROR_FLAG		BIT(6)
#define SC27XX_PD_TX_COLLSION_FLAG	BIT(7)
#define SC27XX_PD_PKG_RV_ERROR_FLAG	BIT(8)
#define SC27XX_PD_FRS_RV_FLAG		BIT(9)
#define SC27XX_PD_RX_FIFO_OVERFLOW_FLAG	 BIT(10)

/* Bits definitions for SC27XX_INT_CLR register */
#define SC27XX_PD_HARD_RST_RV_CLR	BIT(0)
#define SC27XX_PD_CABLE_RST_RV_CLR	BIT(1)
#define SC27XX_PD_SOFT_RST_RV_CLR	BIT(2)
#define SC27XX_PD_PS_RDY_CLR		BIT(3)
#define SC27XX_PD_PKG_RV_CLR		BIT(4)
#define SC27XX_PD_TX_OK_CLR		BIT(5)
#define SC27XX_PD_TX_ERROR_CLR		BIT(6)
#define SC27XX_PD_TX_COLLSION_CLR	BIT(7)
#define SC27XX_PD_PKG_RV_ERROR_CLR	BIT(8)
#define SC27XX_PD_FRS_RV_CLR		BIT(9)
#define SC27XX_PD_RX_FIFO_OVERFLOW_CLR	 BIT(10)

/* Bits definitions for SC27XX_INT_EN register */
#define SC27XX_PD_HARD_RST_RV_EN	BIT(0)
#define SC27XX_PD_CABLE_RST_RV_EN	BIT(1)
#define SC27XX_PD_SOFT_RST_RV_EN	BIT(2)
#define SC27XX_PD_PS_RDY_EN		BIT(3)
#define SC27XX_PD_PKG_RV_EN		BIT(4)
#define SC27XX_PD_TX_OK_EN		BIT(5)
#define SC27XX_PD_TX_ERROR_EN		BIT(6)
#define SC27XX_PD_TX_COLLSION_EN	BIT(7)
#define SC27XX_PD_PKG_RV_ERROR_EN	BIT(8)
#define SC27XX_PD_FRS_RV_EN		BIT(9)
#define SC27XX_PD_RX_FIFO_OVERFLOW_EN	 BIT(10)

#define SC27XX_TX_RX_BUF_MASK	GENMASK(15, 0)
#define SC27XX_PD_INT_CLR	GENMASK(13, 0)
#define SC27XX_STATE_MASK	GENMASK(4, 0)
#define SC27XX_EVENT_MASK	GENMASK(15, 0)
#define SC27XX_TYPEC_INT_CLR_MASK	GENMASK(9, 0)
#define SC27XX_PD_HEAD_CONFIG_MASK	GENMASK(15, 0)

/* SC27XX_MODE */
#define SC27XX_MODE_SNK			0
#define SC27XX_MODE_SRC			1
#define SC27XX_MODE_DRP			2
#define SC27XX_MODE_MASK		3
#define PD_RETRY_COUNT 3

enum sc27xx_state {
	SC27XX_DETACHED_SNK,
	SC27XX_ATTACHWAIT_SNK,
	SC27XX_ATTACHED_SNK,
	SC27XX_DETACHED_SRC,
	SC27XX_ATTACHWAIT_SRC,
	SC27XX_ATTACHED_SRC,
	SC27XX_POWERED_CABLE,
	SC27XX_AUDIO_CABLE,
	SC27XX_DEBUG_CABLE,
	SC27XX_TOGGLE_SLEEP,
	SC27XX_ERR_RECOV,
	SC27XX_DISABLED,
	SC27XX_TRY_SNK,
	SC27XX_TRY_WAIT_SRC,
	SC27XX_TRY_SRC,
	SC27XX_TRY_WAIT_SNK,
	SC27XX_UNSUPOORT_ACC,
	SC27XX_ORIENTED_DEBUG,
};

struct sc27xx_pd {
	struct device *dev;
	struct tcpm_port *tcpm_port;
	struct regmap *regmap;
	struct tcpc_dev tcpc;
	struct mutex lock;
	struct regulator *vbus;
	struct regulator *vconn;
	struct power_supply *psy;
	struct extcon_dev *edev;
	enum typec_cc_polarity cc_polarity;
	enum typec_cc_status cc1;
	enum typec_cc_status cc2;
	enum typec_role role;
	enum typec_data_role data;
	bool attached;
	bool vconn_on;
	bool vbus_on;
	bool charge_on;
	bool vbus_present;
	u32 base;
	u32 typec_base;
	u32 status;
	u32 cc_rp;
	u32 previous_cable;
};

static inline struct sc27xx_pd *tcpc_to_sc27xx_pd(struct tcpc_dev *tcpc)
{
	return container_of(tcpc, struct sc27xx_pd, tcpc);
}

static int sc27xx_pd_start_drp_toggling(struct tcpc_dev *tcpc,
					enum typec_cc_status cc)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	u32 mask = SC27XX_ATTACH_INT_EN | SC27XX_DETACH_INT_EN |
		   SC27XX_RP_CHANGE_INT_EN;
	int ret;

	mutex_lock(&pd->lock);
	switch (cc) {
	case TYPEC_CC_RP_DEF:
		ret = regmap_update_bits(pd->regmap,
					 pd->typec_base + SC27XX_TYPEC_MODE,
					 SC27XX_TYPEC_RP_LEVEL(0x1),
					 SC27XX_TYPEC_RP_LEVEL(0x1));
		break;
	case TYPEC_CC_RP_1_5:
		ret = regmap_update_bits(pd->regmap,
					 pd->typec_base + SC27XX_TYPEC_MODE,
					 SC27XX_TYPEC_RP_LEVEL(0x2),
					 SC27XX_TYPEC_RP_LEVEL(0x2));
		break;
	case TYPEC_CC_RP_3_0:
		ret = regmap_update_bits(pd->regmap,
					 pd->typec_base + SC27XX_TYPEC_MODE,
					 SC27XX_TYPEC_RP_LEVEL(0x3),
					 SC27XX_TYPEC_RP_LEVEL(0x3));
		break;
	case TYPEC_CC_OPEN:
	default:
		ret = 0;
		break;
	}
	if (ret < 0)
		goto done;

	ret = regmap_update_bits(pd->regmap,
				 pd->typec_base + SC27XX_TYPEC_MODE,
				 SC27XX_TYPEC_MODULE_CFG(0x2),
				 SC27XX_TYPEC_MODULE_CFG(0x2));
	if (ret < 0)
		goto done;

	ret = regmap_write(pd->regmap, pd->typec_base +
			   SC27XX_TYPEC_INT_CLR, SC27XX_TYPEC_INT_CLR_MASK);
	if (ret < 0)
		goto done;

	ret = regmap_update_bits(pd->regmap,
				 pd->typec_base + SC27XX_TYPEC_INT_EN,
				 mask, mask);
	if (ret < 0)
		goto done;

	ret = regmap_update_bits(pd->regmap,
				 pd->typec_base + SC27XX_TYPEC_EN,
				 SC27XX_TYPEC_MODULE_EN,
				 SC27XX_TYPEC_MODULE_EN);
done:
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_set_cc(struct tcpc_dev *tcpc, enum typec_cc_status cc)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	mutex_lock(&pd->lock);
	switch (cc) {
	case TYPEC_CC_RD:
		ret = regmap_update_bits(pd->regmap,
					 pd->typec_base + SC27XX_TYPEC_EN,
					 SC27XX_TYPEC_TRY_SNK_EN,
					 SC27XX_TYPEC_TRY_SNK_EN);
		break;
	case TYPEC_CC_RP_DEF:
		ret = regmap_update_bits(pd->regmap,
					 pd->typec_base + SC27XX_TYPEC_MODE,
					 SC27XX_TYPEC_RP_LEVEL(0x1),
					 SC27XX_TYPEC_RP_LEVEL(0x1));
		break;
	case TYPEC_CC_RP_1_5:
		ret = regmap_update_bits(pd->regmap,
					 pd->typec_base + SC27XX_TYPEC_MODE,
					 SC27XX_TYPEC_RP_LEVEL(0x2),
					 SC27XX_TYPEC_RP_LEVEL(0x2));
		break;
	case TYPEC_CC_RP_3_0:
		ret = regmap_update_bits(pd->regmap,
					 pd->typec_base + SC27XX_TYPEC_MODE,
					 SC27XX_TYPEC_RP_LEVEL(0x3),
					 SC27XX_TYPEC_RP_LEVEL(0x3));
		break;
	case TYPEC_CC_OPEN:
	default:
		ret = 0;
		break;
	}

	mutex_unlock(&pd->lock);
	return ret;
}

static int sc27xx_pd_get_cc(struct tcpc_dev *tcpc,
			    enum typec_cc_status *cc1,
			    enum typec_cc_status *cc2)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);

	mutex_lock(&pd->lock);
	*cc1 = pd->cc1;
	*cc2 = pd->cc2;
	mutex_unlock(&pd->lock);
	return 0;
}

static int sc27xx_pd_set_vbus(struct tcpc_dev *tcpc, bool on, bool charge)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	mutex_lock(&pd->lock);
	if (pd->vbus_on == on) {
		dev_info(pd->dev, "vbus is already %s\n", on ? "On" : "Off");
	} else {
		if (on)
			ret = regulator_enable(pd->vbus);
		else
			ret = regulator_disable(pd->vbus);
		if (ret < 0) {
			dev_err(pd->dev, "cannot %s vbus regulator, ret=%d\n",
				on ? "enable" : "disable", ret);
			goto done;
		}
		pd->vbus_on = on;
		dev_info(pd->dev, "vbus := %s", on ? "On" : "Off");
	}
	if (pd->charge_on == charge)
		dev_info(pd->dev, "charge is already %s",
			 charge ? "On" : "Off");
	else
		pd->charge_on = charge;

done:
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_get_vbus(struct tcpc_dev *tcpc)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	mutex_lock(&pd->lock);
	ret = pd->vbus_present ? 1 : 0;
	mutex_unlock(&pd->lock);
	return ret;
}

static int sc27xx_pd_set_current_limit(struct tcpc_dev *dev, u32 max_ma, u32 mv)
{
	return 0;
}

static int sc27xx_pd_get_current_limit(struct tcpc_dev *dev)
{
	return 0;
}

static int sc27xx_pd_set_polarity(struct tcpc_dev *tcpc,
				  enum typec_cc_polarity polarity)
{
	return 0;
}

static int sc27xx_pd_set_roles(struct tcpc_dev *tcpc, bool attached,
			       enum typec_role role, enum typec_data_role data)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;
	u32 mask;

	mutex_lock(&pd->lock);
	pd->role = role;
	pd->data = data;
	pd->attached = attached;
	if (pd->role == TYPEC_SOURCE)
		mask = SC27XX_PD_SRC_SINK_MODE;
	else
		mask = (u32)~SC27XX_PD_SRC_SINK_MODE;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CFG0,
				 SC27XX_PD_SRC_SINK_MODE,
				 mask);
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_set_vconn(struct tcpc_dev *tcpc, bool enable)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret = 0;
	u32 mask;

	mutex_lock(&pd->lock);
	if (pd->vconn_on == enable) {
		dev_info(pd->dev, "vconn already %s\n", enable ? "On" : "Off");
		goto unlock;
	}

	mask = (pd->cc_polarity == TYPEC_POLARITY_CC1) ?
		SC27XX_TYPEC_SW_SWITCH(0x3) : SC27XX_TYPEC_SW_SWITCH(0x2);

	if (enable) {
		ret = regulator_enable(pd->vconn);
		if (ret < 0)
			goto unlock;
		ret = regmap_update_bits(pd->regmap,
					pd->typec_base + SC27XX_TYPEC_SW_CFG,
					mask, mask);
		if (ret < 0)
			goto disable_regulator;
	} else {
		regmap_update_bits(pd->regmap,
				   pd->typec_base + SC27XX_TYPEC_SW_CFG,
				   SC27XX_TYPEC_SW_SWITCH(0x1),
				   SC27XX_TYPEC_SW_SWITCH(0x1));

		regulator_disable(pd->vconn);
	}

	pd->vconn_on = enable;
	mutex_unlock(&pd->lock);
	return 0;

disable_regulator:
	regulator_disable(pd->vconn);
unlock:
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_tx_flush(struct sc27xx_pd *pd)
{
	return regmap_update_bits(pd->regmap,
				  pd->base + SC27XX_PD_CTRL, SC27XX_PD_TX_FLASH,
				  SC27XX_PD_TX_FLASH);
}

static int sc27xx_pd_rx_flush(struct sc27xx_pd *pd)
{
	return regmap_update_bits(pd->regmap,
				  pd->base + SC27XX_PD_CTRL, SC27XX_PD_RX_FLASH,
				  SC27XX_PD_RX_FLASH);
}

static int sc27xx_pd_send_hardreset(struct sc27xx_pd *pd)
{
	return regmap_update_bits(pd->regmap,
				  pd->base + SC27XX_PD_CTRL,
				  SC27XX_PD_HARD_RESET, SC27XX_PD_HARD_RESET);
}

static int sc27xx_pd_set_rx(struct tcpc_dev *tcpc, bool on)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	mutex_lock(&pd->lock);
	ret = sc27xx_pd_rx_flush(pd);
	if (ret < 0)
		goto done;

	ret = sc27xx_pd_tx_flush(pd);
	if (ret < 0)
		goto done;

	dev_info(pd->dev, "pd := %s", on ? "on" : "off");
done:
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_tx_msg(struct sc27xx_pd *pd, const struct pd_message *msg)
{
	u32 data_obj_num, header;
	int i, ret;

	data_obj_num = msg ? pd_header_cnt(msg->header) * 2 : 0;
	header = msg ? msg->header : 0;

	ret = regmap_write(pd->regmap, pd->base + SC27XX_PD_HEAD_CFG, header);
	if (ret < 0)
		return ret;

	for (i = 0; i < data_obj_num; i++) {
		ret = regmap_write(pd->regmap, pd->base + SC27XX_PD_TX_BUF,
				   msg->payload[i]);
		if (ret < 0)
			return ret;
	}

	return regmap_update_bits(pd->regmap,
				  pd->base + SC27XX_PD_CTRL, SC27XX_PD_TX_START,
				  SC27XX_PD_TX_START);
}

static int sc27xx_pd_transmit(struct tcpc_dev *tcpc,
			      enum tcpm_transmit_type type,
			      const struct pd_message *msg)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	mutex_lock(&pd->lock);
	switch (type) {
	case TCPC_TX_SOP:
		ret = sc27xx_pd_tx_msg(pd, msg);
		if (ret < 0)
			dev_err(pd->dev, "cannot send PD message, ret=%d\n",
				ret);
		break;
	case TCPC_TX_HARD_RESET:
		ret = sc27xx_pd_send_hardreset(pd);
		if (ret < 0)
			dev_err(pd->dev, "cann't send hardreset ret=%d\n", ret);
		break;
	default:
		dev_err(pd->dev, "type %d not supported", type);
		ret = -EINVAL;
	}
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_read_message(struct sc27xx_pd *pd, struct pd_message *msg)
{
	int ret, i;
	u32 data_obj_num;

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_RX_BUF,
			  (u32 *)&msg->header);
	if (ret < 0)
		return ret;

	msg->header &= SC27XX_TX_RX_BUF_MASK;
	data_obj_num = pd_header_cnt(msg->header) * 2;

	for (i = 0; i < data_obj_num; i++) {
		ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_RX_BUF,
				  &msg->payload[i]);
		if (ret < 0)
			return ret;
	}

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_INT_CLR,
				 SC27XX_PD_PKG_RV_CLR, SC27XX_PD_PKG_RV_CLR);
	if (ret < 0)
		return ret;

	if (!data_obj_num &&
	    pd_header_type_le(msg->header) == PD_CTRL_GOOD_CRC) {
		if (pd->constructed == false) {
			ret = regmap_update_bits(pd->regmap,
						pd->typec_base + SC27XX_TYPC_PD_CFG,
						SC27XX_TYPEC_PD_CONSTRACT,
						SC27XX_TYPEC_PD_CONSTRACT);
			pd->constructed = true;
		}
		tcpm_pd_transmit_complete(pd->tcpm_port, TCPC_TX_SUCCESS);
	} else {
		tcpm_pd_receive(pd->tcpm_port, msg);
	}

	return ret;
}

static int sc27xx_pd_init(struct tcpc_dev *tcpc)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	u32 mask = SC27XX_PD_HARD_RST_RV_EN | SC27XX_PD_CABLE_RST_RV_EN |
	    SC27XX_PD_SOFT_RST_RV_EN | SC27XX_PD_TX_OK_EN |
	    SC27XX_PD_TX_OK_EN | SC27XX_PD_TX_ERROR_EN |
	    SC27XX_PD_PKG_RV_ERROR_EN | SC27XX_PD_RX_FIFO_OVERFLOW_EN;
	int ret;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CFG0,
				 SC27XX_PD_CTL_EN, SC27XX_PD_CTL_EN);
	if (ret < 0)
		return ret;

	ret = regmap_write(pd->regmap, pd->base + SC27XX_INT_CLR,
			   SC27XX_PD_INT_CLR);
	if (ret < 0)
		goto disable_pd;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_INT_EN, mask, mask);
	if (ret < 0)
		goto disable_pd;
	return 0;

disable_pd:
	regmap_update_bits(pd->regmap,
			   pd->base + SC27XX_PD_CFG0,
			   SC27XX_PD_CTL_EN, ~SC27XX_PD_CTL_EN);

	return ret;
}

static int sc27xx_pd_reset(struct sc27xx_pd *pd)
{
	int ret;
	u32 mask = SC27XX_PD_CTL_EN;

	mutex_lock(&pd->lock);
	ret = sc27xx_pd_rx_flush(pd);
	if (ret < 0)
		goto done;

	ret = sc27xx_pd_tx_flush(pd);
	if (ret < 0)
		goto done;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CTRL, SC27XX_PD_RX_ID_CLR,
				 SC27XX_PD_RX_ID_CLR);
	if (ret < 0)
		goto done;

	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_HEAD_CFG,
				 SC27XX_PD_HEAD_CONFIG_MASK,
				 0x0);
	if (ret < 0)
		goto done;

	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_MESG_ID_CFG,
				 SC27XX_PD_MESS_ID_TX(0x0),
				 0x0);
	if (ret < 0)
		goto done;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CFG0, mask, ~mask);
	if (ret < 0)
		goto done;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CFG0, mask, mask);
	if (ret < 0)
		goto done;
done:
	mutex_unlock(&pd->lock);

	return ret;
}

static irqreturn_t sc27xx_pd_irq(int irq, void *dev_id)
{
	struct sc27xx_pd *pd = dev_id;
	struct pd_message pd_msg;
	u32 status;
	int ret;

	mutex_lock(&pd->lock);

	ret = regmap_read(pd->regmap, pd->base + SC27XX_INT_FLG, &status);
	if (ret < 0)
		goto done;

	if (status & SC27XX_PD_FRS_RV_FLAG)
		dev_warn(pd->dev, "IRQ: PD fast role switch error\n");

	if (status & SC27XX_PD_CABLE_RST_FLAG)
		dev_warn(pd->dev, "IRQ: PD cable rst flag\n");

	if (status & SC27XX_PD_RX_FIFO_OVERFLOW_FLAG)
		dev_warn(pd->dev, "IRQ: PD rx fifo overflow flag\n");

	if (status & SC27XX_PD_SOFT_RST_FLAG) {
		ret = sc27xx_pd_reset(pd);
		if (ret < 0) {
			dev_err(pd->dev, "cannot PD reset, ret=%d\n", ret);
			goto done;
		}
		tcpm_pd_hard_reset(pd->tcpm_port);
	}

	if (status & SC27XX_PD_HARD_RST_FLAG) {
		dev_info(pd->dev, "IRQ: PD received hardreset");
		ret = sc27xx_pd_reset(pd);
		if (ret < 0) {
			dev_err(pd->dev, "cannot PD reset, ret=%d\n", ret);
			goto done;
		}
		tcpm_pd_hard_reset(pd->tcpm_port);
	}

	if (status & SC27XX_PD_TX_OK_FLAG)
		tcpm_pd_transmit_complete(pd->tcpm_port, TCPC_TX_SUCCESS);

	if (status & SC27XX_PD_TX_ERROR_FLAG) {
		dev_err(pd->dev, "IRQ: PD tx failed\n");
		tcpm_pd_transmit_complete(pd->tcpm_port, TCPC_TX_FAILED);
	}

	if (status & SC27XX_PD_TX_COLLSION_FLAG) {
		dev_warn(pd->dev, "IRQ: PD collision\n");
		tcpm_pd_transmit_complete(pd->tcpm_port, TCPC_TX_FAILED);
	}

	if (status & SC27XX_PD_PKG_RV_FLAG) {
		ret = sc27xx_pd_read_message(pd, &pd_msg);
		if (ret < 0) {
			dev_err(pd->dev, "not read PD msg, ret=%d\n", ret);
			goto done;
		}
	}

	if (status & SC27XX_PD_PKG_RV_ERROR_FLAG)
		dev_err(pd->dev, "IRQ: PD rx error flag");

done:
	mutex_unlock(&pd->lock);

	return IRQ_HANDLED;
}

static int sc27xx_get_vbus_status(struct sc27xx_pd *pd)
{
	u32 status;
	bool vbus_present;
	int ret;

	ret = regmap_read(pd->regmap, pd->typec_base +
			  SC27XX_TYPEC_DBG1, &status);
	if (ret < 0)
		return ret;

	vbus_present = !!(status & SC27XX_TYPEC_VBUS_OK);
	if (vbus_present != pd->vbus_present) {
		pd->vbus_present = vbus_present;
		tcpm_vbus_change(pd->tcpm_port);
	}

	return 0;
}

static void sc27xx_cc_polarity_status(struct sc27xx_pd *pd)
{
	if (pd->status & SC27XX_TYPEC_FINAL_SWITCH)
		pd->cc_polarity = TYPEC_POLARITY_CC1;
	else
		pd->cc_polarity = TYPEC_POLARITY_CC2;
}

static void sc27xx_cc_rp_status(struct sc27xx_pd *pd)
{
	u32 rp_sts = SC27XX_TYPEC_VBUS_CL(pd->status);

	switch (rp_sts) {
	case 0:
		pd->cc_rp = TYPEC_CC_RP_DEF;
		break;
	case 1:
		pd->cc_rp = TYPEC_CC_RP_1_5;
		break;
	case 2:
		pd->cc_rp = TYPEC_CC_RP_3_0;
		break;
	default:
		pd->cc_rp = TYPEC_CC_OPEN;
		break;
	}
}

static void sc27xx_cc_status(struct sc27xx_pd *pd)
{
	switch (pd->status) {
	case SC27XX_ATTACHED_SNK:
		if (pd->cc_polarity == TYPEC_POLARITY_CC1) {
			pd->cc1 = pd->cc_rp;
			pd->cc2 = TYPEC_CC_OPEN;
		} else {
			pd->cc1 = TYPEC_CC_OPEN;
			pd->cc2 = pd->cc_rp;
		}
		break;

	case SC27XX_ATTACHED_SRC:
		if (pd->cc_polarity == TYPEC_POLARITY_CC1) {
			pd->cc1 = TYPEC_CC_RD;
			pd->cc2 = TYPEC_CC_OPEN;
		} else {
			pd->cc1 = TYPEC_CC_OPEN;
			pd->cc2 = TYPEC_CC_RD;
		}
		break;

	case SC27XX_POWERED_CABLE:
		if (pd->cc_polarity == TYPEC_POLARITY_CC1) {
			pd->cc1 = TYPEC_CC_RD;
			pd->cc2 = TYPEC_CC_RA;
		} else {
			pd->cc1 = TYPEC_CC_RA;
			pd->cc2 = TYPEC_CC_RD;
		}
		break;
	default:
		pd->cc1 = TYPEC_CC_OPEN;
		pd->cc2 = TYPEC_CC_OPEN;
		break;
	}

	tcpm_cc_change(pd->tcpm_port);
}

static void sc27xx_typec_vbus_cc_status(struct sc27xx_pd *pd)
{
	sc27xx_cc_polarity_status(pd);
	sc27xx_cc_status(pd);
	sc27xx_get_vbus_status(pd);
}

static int sc27xx_typec_connect(struct sc27xx_pd *pd)
{
	switch (pd->status) {
	case SC27XX_ATTACHED_SNK;
		return extcon_set_state_sync(pd->edev, EXTCON_USB, true);

	case SC27XX_ATTACHED_SRC:
		return extcon_set_state_sync(pd->edev, EXTCON_USB_HOST, true);

	default:
		return -EINVAL;
	}
}

static int sc27xx_typec_disconnect(struct sc27xx_pd *pd)
{
	switch (pd->status) {
	case SC27XX_ATTACHED_SNK;
		return extcon_set_state_sync(pd->edev, EXTCON_USB, false);

	case SC27XX_ATTACHED_SRC:
		return extcon_set_state_sync(pd->edev, EXTCON_USB_HOST, false);

	default:
		return -EINVAL;
	}
}

static irqreturn_t sc27xx_typec_irq(int irq, void *data)
{
	struct sc27xx_pd *pd = data;
	u32 event;
	int ret;

	mutex_lock(&pd->lock);
	ret = regmap_read(pd->regmap, pd->typec_base + SC27XX_TYPEC_INT_MASK,
			  &event);
	if (ret)
		goto unlock;

	event &= SC27XX_EVENT_MASK;

	ret = regmap_read(pd->regmap, pd->typec_base +
			SC27XX_TYPEC_STATUS, &pd->status);
	if (ret)
		goto clear_ints;

	sc27xx_typec_vbus_cc_status(pd);
	pd->status &= SC27XX_STATE_MASK;
	if (event & SC27XX_TYPEC_RP_CHANGE_INT_MASK)
		sc27xx_cc_rp_status(pd);

	if (event & SC27XX_TYPEC_ATTACH_INT_MASK) {
		ret = sc27xx_typec_connect(pd);
		if (ret)
			dev_err(pd->dev, "typec connect sync failed\n");
	} else if (event & SC27XX_TYPEC_UNATTACH_INT_MASK) {
		ret = sc27xx_typec_disconnect(pd);
		if (ret)
			dev_err(pd->dev, "typec disconnect sync failed\n");
	}

clear_ints:
	regmap_write(pd->regmap, pd->typec_base + SC27XX_TYPEC_INT_CLR, event);
unlock:
	mutex_unlock(&pd->lock);

	dev_info(pd->dev, "now works as DRP and is in %d state, event %d\n",
		pd->status, event);

	return IRQ_HANDLED;
}

#define PDO_FIXED_FLAGS \
	(PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_USB_COMM)

static const u32 src_pdo[] = {
	PDO_FIXED(5000, 400, PDO_FIXED_FLAGS),
};

static const u32 snk_pdo[] = {
	PDO_FIXED(5000, 400, PDO_FIXED_FLAGS),
};

static const struct tcpc_config sc27xx_pd_config = {
	.src_pdo = src_pdo,
	.nr_src_pdo = ARRAY_SIZE(src_pdo),
	.snk_pdo = snk_pdo,
	.nr_snk_pdo = ARRAY_SIZE(snk_pdo),
	.max_snk_mv = 5000,
	.max_snk_ma = 3000,
	.max_snk_mw = 15000,
	.operating_snk_mw = 2500,
	.type = TYPEC_PORT_DRP,
	.default_role = TYPEC_SINK,
	.alt_modes = NULL,
};

static const u32 sc27xx_tcable[] = {
	EXTCON_NONE,
	EXTCON_USB,
	EXTCON_USB_HOST,
};

static void sc27xx_init_tcpc_dev(struct sc27xx_pd *pd)
{
	pd->tcpc.config = &sc27xx_pd_config;
	pd->tcpc.init = sc27xx_pd_init;
	pd->tcpc.get_vbus = sc27xx_pd_get_vbus;
	pd->tcpc.get_current_limit = sc27xx_pd_get_current_limit;
	pd->tcpc.set_cc = sc27xx_pd_set_cc;
	pd->tcpc.get_cc = sc27xx_pd_get_cc;
	pd->tcpc.set_polarity = sc27xx_pd_set_polarity;
	pd->tcpc.set_vconn = sc27xx_pd_set_vconn;
	pd->tcpc.set_vbus = sc27xx_pd_set_vbus;
	pd->tcpc.set_current_limit = sc27xx_pd_set_current_limit;
	pd->tcpc.set_pd_rx = sc27xx_pd_set_rx;
	pd->tcpc.set_roles = sc27xx_pd_set_roles;
	pd->tcpc.start_drp_toggling = sc27xx_pd_start_drp_toggling;
	pd->tcpc.pd_transmit = sc27xx_pd_transmit;
}

static int sc27xx_pd_probe(struct platform_device *pdev)
{
	struct sc27xx_pd *pd;
	u32 val;
	int pd_irq, typec_irq, ret;

	pd = devm_kzalloc(&pdev->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;

	pd->previous_cable = EXTCON_NONE;
	pd->edev = devm_extcon_dev_allocate(&pdev->dev, sc27xx_tcable);
	if (IS_ERR(pd->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(&pdev->dev, pd->edev);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't register extcon device: %d\n", ret);
		return ret;
	}

	pd->dev = &pdev->dev;
	pd->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!pd->regmap) {
		dev_err(&pdev->dev, "failed to get pd regmap\n");
		return -ENODEV;
	}

	if (of_property_read_u32_index(np, "reg", 0, &pd->base)) {
		dev_err(&pdev->dev, "failed to get pd reg address\n");
		return -ENODEV;
	}

	if (of_property_read_u32_index(np, "reg", 1, &pd->typec_base)) {
		dev_err(&pdev->dev, "failed to get typec reg address\n");
		return -ENODEV;
	}

	pd_irq = platform_get_irq(pdev, 0);
	if (pd_irq < 0) {
		dev_err(&pdev->dev, "failed to get pd irq number\n");
		return pd_irq;
	}

	typec_irq = platform_get_irq(pdev, 1);
	if (typec_irq < 0) {
		dev_err(&pdev->dev, "failed to get typec irq number\n");
		return typec_irq;
	}

	mutex_init(&pd->lock);
	pd->vbus_present = false;
	pd->constructed = false;
	sc27xx_init_tcpc_dev(pd);

	if (!device_property_read_u32(pd->dev, "sprd,max-sink-mv", &val))
		pd->tcpc_config.max_snk_mv = val;

	if (!device_property_read_u32(pd->dev, "sprd,max-sink-ma", &val))
		pd->tcpc_config.max_snk_ma = val;

	if (!device_property_read_u32(pd->dev, "sprd,max-sink-mw", &val))
		pd->tcpc_config.max_snk_mw = val;

	if (!device_property_read_u32(pd->dev,
				      "sprd,operating-sink-mw", &val))
		pd->tcpc_config.operating_snk_mw = val;

	ret = devm_request_threaded_irq(pd->dev, pd_irq, NULL,
					sc27xx_pd_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_LOW,
					"sc27xx_pd", pd);
	if (ret < 0)
		return ret;

	ret = devm_request_threaded_irq(pd->dev, typec_irq, NULL,
					sc27xx_typec_irq,
					IRQF_NO_SUSPEND | IRQF_EARLY_RESUME,
					"sc27xx_typec", pd);
	if (ret)
		return ret;

	pd->vbus = devm_regulator_get(pd->dev, "vbus");
	if (IS_ERR(pd->vbus)) {
		dev_err(&pdev->dev, "pd failed to get vbus\n");
		return PTR_ERR(pd->vbus);
	}
	pd->vconn = devm_regulator_get(pd->dev, "vconn");
	if (IS_ERR(pd->vbus)) {
		dev_err(&pdev->dev, "pd failed to get vccon\n");
		return PTR_ERR(pd->vbus);
	}

	pd->tcpm_port = tcpm_register_port(pd->dev, &pd->tcpc);
	platform_set_drvdata(pdev, pd);

	return PTR_ERR_OR_ZERO(pd->tcpm_port);
}

static int sc27xx_pd_remove(struct platform_device *pdev)
{
	struct sc27xx_pd *pd = platform_get_drvdata(pdev);

	tcpm_unregister_port(pd->tcpm_port);
	return 0;
}

static const struct of_device_id sc27xx_pd_of_match[] = {
	{.compatible = "sprd,sc2730-typec-pd"},
	{}
};

static struct platform_driver sc27xx_pd_driver = {
	.probe = sc27xx_pd_probe,
	.remove = sc27xx_pd_remove,
	.driver = {
		   .name = "sc27xx-typec-pd",
		   .of_match_table = sc27xx_pd_of_match,
	},
};

module_platform_driver(sc27xx_pd_driver);

MODULE_AUTHOR("Freeman Liu <freeman.liu@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum SC27xx typec driver");
MODULE_LICENSE("GPL v2");
