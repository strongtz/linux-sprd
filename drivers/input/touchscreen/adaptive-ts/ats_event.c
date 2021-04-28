#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include "ats_core.h"

#ifdef TS_USE_ADF_NOTIFIER
#include <video/adf_notifier.h>
#endif

/*
 * stores notifier to receive external events
 */
struct ts_event_receiver {
	struct ts_data *pdata;
	event_handler_t inform;
#ifdef TS_USE_ADF_NOTIFIER
	struct notifier_block adf_event_block;
#endif
#ifdef CONFIG_USB_SPRD_DWC
	struct notifier_block usb_event_block;
#endif
};

static struct ts_event_receiver receiver;

#ifdef TS_USE_ADF_NOTIFIER
/*
 * touchscreen's suspend and resume state should rely on screen state,
 * as fb_notifier and early_suspend are all disabled on our platform,
 * we can only use adf_event now
 */
static int ts_adf_event_handler(
	struct notifier_block *nb, unsigned long action, void *data)
{
	struct ts_event_receiver *p = container_of(
		nb, struct ts_event_receiver, adf_event_block);
	struct adf_notifier_event *event = data;
	int adf_event_data;

	if (action != ADF_EVENT_BLANK)
		return NOTIFY_DONE;

	adf_event_data = *(int *)event->data;
	pr_debug("receive adf event with adf_event_data=%d", adf_event_data);

	switch (adf_event_data) {
	case DRM_MODE_DPMS_ON:
		p->inform(p->pdata, TSEVENT_RESUME, NULL);
		break;
	case DRM_MODE_DPMS_OFF:
		p->inform(p->pdata, TSEVENT_SUSPEND, NULL);
		break;
	default:
		pr_warn("receive adf event with error data, adf_event_data=%d",
			adf_event_data);
		break;
	}

	return NOTIFY_OK;
}
#endif

#ifdef CONFIG_USB_SPRD_DWC
/*
 * indicates plug in/out
 */
enum usb_cable_status {
	USB_CABLE_PLUG_IN,
	USB_CABLE_PLUG_OUT,
};
extern int register_usb_notifier(struct notifier_block *nb);
extern int unregister_usb_notifier(struct notifier_block *nb);

static int ts_usb_event_handler(
	struct notifier_block *nb, unsigned long action, void *data)
{
	struct ts_event_receiver *p = container_of(
		nb, struct ts_event_receiver, usb_event_block);

	switch (action) {
	case USB_CABLE_PLUG_IN:
		pr_debug("receive usb plug-in event");
		p->inform(p->pdata, TSEVENT_NOISE_HIGH, NULL);
		break;
	case USB_CABLE_PLUG_OUT:
		pr_debug("receive usb plug-out event");
		p->inform(p->pdata, TSEVENT_NOISE_NORMAL, NULL);
		break;
	default:
		pr_warn("receive usb event with unknown action: %lu", action);
		break;
	}
	return NOTIFY_OK;
}
#endif

/*
 * register handler and send external events to ats core
 */
int ts_register_ext_event_handler(
	struct ts_data *pdata,	event_handler_t handler)
{
	int ret = 0;

	receiver.pdata = pdata;
	receiver.inform = handler;

#ifdef TS_USE_ADF_NOTIFIER
	receiver.adf_event_block.notifier_call = ts_adf_event_handler;
	ret = adf_register_client(&receiver.adf_event_block);
	if (ret < 0)
		dev_warn(&pdata->pdev->dev, "register adf notifier fail, cannot sleep when screen off");
	else
		dev_dbg(&pdata->pdev->dev, "register adf notifier succeed");
#endif

#ifdef CONFIG_USB_SPRD_DWC
	receiver.usb_event_block.notifier_call = ts_usb_event_handler;
	ret = register_usb_notifier(&receiver.usb_event_block);
	if (ret < 0)
		dev_warn(&pdata->pdev->dev, "register usb notifier fail");
	else
		dev_dbg(&pdata->pdev->dev, "register usb notifier succeed");
#endif

	return ret;
}

void ts_unregister_ext_event_handler(void)
{
#ifdef TS_USE_ADF_NOTIFIER
	adf_unregister_client(&receiver.adf_event_block);
#endif

#ifdef CONFIG_USB_SPRD_DWC
	unregister_usb_notifier(&receiver.usb_event_block);
#endif
}

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum touchscreen external event receiver");
MODULE_LICENSE("GPL");
