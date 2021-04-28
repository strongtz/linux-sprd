/*
 * USB PAM defines
 *
 * These APIs may be used by controller or function drivers.
 */

#ifndef __LINUX_USB_PAM_H
#define __LINUX_USB_PAM_H

#include <linux/notifier.h>
#include <linux/usb.h>
#include <linux/usb/phy.h>

/* associate a type with PAM */
enum usb_pam_type {
	USB_PAM_TYPE_UNDEFINED,
	USB_PAM_TYPE_USB2,
	USB_PAM_TYPE_USB3,
};

#endif /* __LINUX_USB_PAM_H */
