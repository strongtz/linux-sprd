/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _CAM_PW_DOMAIN_H_
#define _CAM_PW_DOMAIN_H_

#include <linux/notifier.h>

/* no need
 * int sprd_cam_pw_domain_init(struct platform_device *pdev);
 * int sprd_cam_pw_domain_deinit(void);
 */
int sprd_cam_pw_on(void);
int sprd_cam_pw_off(void);
int sprd_cam_domain_eb(void);
int sprd_cam_domain_disable(void);
int sprd_cache_flush(void *work_buf_vaddr, int pase_size);

/* power on/off call back function, use as follow:
 * 1: static int ssss_event(struct notifier_block *self, unsigned long event,
 *		void *ptr) {  do something  }
 * 2: static struct notifier_block ssss_notifier = {
 *	.notifier_call = ssss_event,
 *    };
 * 3: sprd_mm_pw_notify_register(&ssss_notifier);
 */
enum {
	_E_PW_OFF = 0,
	_E_PW_ON  = 1,
};
int sprd_mm_pw_notify_register(struct notifier_block *nb);
int sprd_mm_pw_notify_unregister(struct notifier_block *nb);

#endif /* _CAM_PW_DOMAIN_H_ */
