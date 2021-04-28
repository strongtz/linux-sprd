/*
 * Copyright (C) 2012-2019 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) "sprd_sip: " fmt

#include <linux/arm-smccc.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/sprd_sip_svc.h>
#include <linux/string.h>
#include <linux/types.h>

/* SIP Function Identifier */

/* SIP General */
#define	SPRD_SIP_SVC_CALL_COUNT						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0xFF00)

#define	SPRD_SIP_SVC_UID						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0xFF01)

#define	SPRD_SIP_SVC_REV						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0xFF03)

/* SIP performance operations */
#define	SPRD_SIP_SVC_PERF_REV						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0100)

#define	SPRD_SIP_SVC_PERF_SET_FREQ					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0101)

#define	SPRD_SIP_SVC_PERF_GET_FREQ					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0102)

#define	SPRD_SIP_SVC_PERF_SET_DIV					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0103)

#define	SPRD_SIP_SVC_PERF_GET_DIV					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0104)

#define	SPRD_SIP_SVC_PERF_SET_PARENT					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0105)

#define	SPRD_SIP_SVC_PERF_GET_PARENT					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0106)

static struct sprd_sip_svc_handle sprd_sip_svc_handle = {};

struct sprd_sip_svc_handle *sprd_sip_svc_get_handle(void)
{
	return &sprd_sip_svc_handle;
}
EXPORT_SYMBOL(sprd_sip_svc_get_handle);

static int sprd_sip_svc_perf_set_freq(u32 id, u32 parent_id, u32 freq)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_SET_FREQ,
			id, parent_id, freq, 0, 0, 0, 0, &res);

	return res.a0;
}

static int sprd_sip_svc_perf_get_freq(u32 id, u32 parent_id, u32 *p_freq)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_GET_FREQ,
			id, parent_id, 0, 0, 0, 0, 0, &res);

	if (p_freq != NULL)
		*p_freq = res.a1;

	return res.a0;
}

static int sprd_sip_svc_perf_set_div(u32 id, u32 div)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_SET_DIV,
			id, div, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static int sprd_sip_svc_perf_get_div(u32 id, u32 *p_div)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_GET_DIV,
			id, 0, 0, 0, 0, 0, 0, &res);

	if (p_div != NULL)
		*p_div = res.a1;

	return res.a0;
}

static int sprd_sip_svc_perf_set_parent(u32 id, u32 parent_id)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_SET_PARENT,
			id, parent_id, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static int sprd_sip_svc_perf_get_parent(u32 id, u32 *p_parent_id)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_GET_PARENT,
			id, 0, 0, 0, 0, 0, 0, &res);

	if (p_parent_id != NULL)
		*p_parent_id = res.a1;

	return res.a0;
}

static int __init sprd_sip_svc_init(void)
{
	int ret = 0;
	struct arm_smccc_res res;

	/* init perf_ops */
	arm_smccc_smc(SPRD_SIP_SVC_PERF_REV, 0, 0, 0, 0, 0, 0, 0, &res);
	sprd_sip_svc_handle.perf_ops.rev.major_ver = res.a0;
	sprd_sip_svc_handle.perf_ops.rev.minor_ver = res.a1;

	sprd_sip_svc_handle.perf_ops.set_freq = sprd_sip_svc_perf_set_freq;
	sprd_sip_svc_handle.perf_ops.get_freq = sprd_sip_svc_perf_get_freq;

	sprd_sip_svc_handle.perf_ops.set_div = sprd_sip_svc_perf_set_div;
	sprd_sip_svc_handle.perf_ops.get_div = sprd_sip_svc_perf_get_div;

	sprd_sip_svc_handle.perf_ops.set_parent = sprd_sip_svc_perf_set_parent;
	sprd_sip_svc_handle.perf_ops.get_parent = sprd_sip_svc_perf_get_parent;

	pr_notice("SPRD SIP SVC PERF:v%d.%d detected in firmware.\n",
		sprd_sip_svc_handle.perf_ops.rev.major_ver,
		sprd_sip_svc_handle.perf_ops.rev.minor_ver);

	return ret;
}

arch_initcall(sprd_sip_svc_init);
