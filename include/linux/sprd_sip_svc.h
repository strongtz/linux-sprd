/*
 * Copyright (C) 2012-2015 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SPRD_SIP_SVC_H__
#define __SPRD_SIP_SVC_H__

#include <linux/init.h>
#include <linux/types.h>

/*
 * Provides interfaces to SoC implementation-specific services on this
 * platform, for example secure platform initialization, configuration,
 * and some power control services.
 */

/* structure definitions */

/**
 * struct sprd_sip_svc_rev_info - version information structure
 *
 * @major_ver: Major ABI version. Change here implies risk of backward
 *	compatibility break.
 * @minor_ver: Minor ABI version. Change here implies new feature addition,
 *	or compatible change in ABI.
 */
struct sprd_sip_svc_rev_info {
	u32 major_ver;
	u32 minor_ver;
};

/**
 * struct sprd_sip_svc_perf_ops - represents the various operations provided
 *	by SPRD SIP PERF
 *
 * @set_freq: sets the frequency of a given device
 * @get_freq: gets the frequency of a given device
 *
 * @set_div: sets the clock divisor of a given device
 * @get_div: gets the clock divisor of a given device
 *
 * @set_parent: sets the parent of a given device
 * @get_parent: gets the parent of a given device
 */
struct sprd_sip_svc_perf_ops {
	struct sprd_sip_svc_rev_info rev;

	int (*set_freq)(u32 id, u32 parent_id, u32 freq);
	int (*get_freq)(u32 id, u32 parent_id, u32 *p_freq);

	int (*set_div)(u32 id, u32 div);
	int (*get_div)(u32 id, u32 *p_div);

	int (*set_parent)(u32 id, u32 parent_id);
	int (*get_parent)(u32 id, u32 *p_parent_id);
};

/**
 * struct sprd_sip_svc_handle - Handle returned to SPRD SIP clients for usage
 *
 * @perf_ops: pointer to set of performance operations
 */
struct sprd_sip_svc_handle {
	struct sprd_sip_svc_perf_ops perf_ops;
};

/**
 * sprd_sip_svc_get_handle() - returns a pointer to SPRD SIP handle
 *
 * Return: a pointer to SPRD_SIP handle
 */
struct sprd_sip_svc_handle *sprd_sip_svc_get_handle(void);

#endif /* __SPRD_SIP_SVC_H__ */
