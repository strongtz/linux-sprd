/*
 * Copyright (C) 2012--2015 Spreadtrum Communications Inc.
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
#ifndef _VSP_PW_DOMAIN_H_
#define _VSP_PW_DOMAIN_H_

#define BIT_PMU_APB_PD_MM_VSP_AUTO_SHUTDOWN_EN                  BIT(24)
#define BIT_PMU_APB_PD_MM_VSP_FORCE_SHUTDOWN                    BIT(25)
#define BIT_PMU_APB_PD_MM_VSP_STATE(x)                          (((x) & 0x1F))

enum {
	VSP_PW_DOMAIN_VSP = 0,
	VSP_PW_DOMAIN_VPP,
	VSP_PW_DOMAIN_VSP_ENC,
	VSP_PW_DOMAIN_VSP_GSP,
	VSP_PW_DOMAIN_VSP_JPG,
	VSP_PW_DOMAIN_VSP_CPP,
	VSP_PW_DOMAIN_COUNT_MAX
};
enum {
	VSP_PW_DOMAIN_OFF = 0,
	VSP_PW_DOMAIN_ON,
};
struct client_info_t {
	u8 pw_state;
	u8 pw_count;
};

struct vsp_pw_domain_info_t {
	struct client_info_t pw_vsp_info[VSP_PW_DOMAIN_COUNT_MAX];
	struct mutex client_lock;
	u8 vsp_pw_state;
};

int vsp_pw_on(u8 client);
int vsp_pw_off(u8 client);
int vsp_core_pw_on(void);
int vsp_core_pw_off(void);

#endif
