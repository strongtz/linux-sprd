/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#undef TRACE_SYSTEM
#define TRACE_SYSTEM gsp

#if !defined(_GSP_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _GSP_TRACE_H

#include <linux/tracepoint.h>
#include "gsp_core.h"
#include "gsp_kcfg.h"


TRACE_EVENT(kcfg_push,
	TP_PROTO(struct gsp_kcfg *kcfg),
	TP_ARGS(kcfg),

	TP_STRUCT__entry(
		__field(int, id)
		__field(int, ktag)
		__field(int, fill_cnt)
	),
	TP_fast_assign(
		__entry->id = kcfg->bind_core->id;
		__entry->ktag = kcfg->tag;
		__entry->fill_cnt = kcfg->wq->fill_cnt;
	),
	TP_printk("kcfg[%d] push to gsp-core[%d] , fill_cnt: %d",
						__entry->ktag,
						__entry->id,
						__entry->fill_cnt)
);

TRACE_EVENT(kcfg_pull,
	TP_PROTO(struct gsp_kcfg *kcfg),
	TP_ARGS(kcfg),

	TP_STRUCT__entry(
		__field(int, id)
		__field(int, ktag)
		__field(int, fill_cnt)
	),
	TP_fast_assign(
		__entry->id = kcfg->bind_core->id;
		__entry->ktag = kcfg->tag;
		__entry->fill_cnt = kcfg->wq->fill_cnt;
	),
	TP_printk("kcfg[%d] pulled by gsp-core[%d] , fill_cnt: %d",
						__entry->ktag,
						__entry->id,
						__entry->fill_cnt)
);

TRACE_EVENT(kcfg_put,
	TP_PROTO(struct gsp_kcfg *kcfg),
	TP_ARGS(kcfg),

	TP_STRUCT__entry(
		__field(int, id)
		__field(int, ktag)
		__field(int, fill_cnt)
	),
	TP_fast_assign(
		__entry->id = kcfg->bind_core->id;
		__entry->ktag = kcfg->tag;
		__entry->fill_cnt = kcfg->wq->fill_cnt;
	),
	TP_printk("kcfg[%d] put by gsp-core[%d] , fill_cnt: %d",
						__entry->ktag,
						__entry->id,
						__entry->fill_cnt)
);

#endif /* _GSP_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE gsp_trace
#include <trace/define_trace.h>

