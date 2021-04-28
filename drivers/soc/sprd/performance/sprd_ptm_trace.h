#undef TRACE_SYSTEM
#define TRACE_SYSTEM ptm

#if !defined(_TRACE_PTM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PTM_H

#include <linux/tracepoint.h>

TRACE_EVENT(ptm_ddr_chn,

	TP_PROTO(int chn),

	TP_ARGS(chn),

	TP_STRUCT__entry(
		__field(int, chn)
	),

	TP_fast_assign(
		__entry->chn = chn;
	),

	TP_printk("%d", __entry->chn)
);

TRACE_EVENT(ptm_ddr_ts,

	TP_PROTO(u32 ts),

	TP_ARGS(ts),

	TP_STRUCT__entry(
		__field(u32, ts)
	),

	TP_fast_assign(
		__entry->ts = ts;
	),

	TP_printk("%u", __entry->ts)
);

TRACE_EVENT(ptm_ddr_info,

	TP_PROTO(const char *chn, u32 rd_cnt, u32 rd_bw, u32 rd_lty,
		 u32 wr_cnt, u32 wr_bw, u32 wr_lty),

	TP_ARGS(chn, rd_cnt, rd_bw, rd_lty, wr_cnt, wr_bw, wr_lty),

	TP_STRUCT__entry(
		__array(char, chn, 20)
		__field(u32, rd_cnt)
		__field(u32, rd_bw)
		__field(u32, rd_lty)
		__field(u32, wr_cnt)
		__field(u32, wr_bw)
		__field(u32, wr_lty)
	),

	TP_fast_assign(
		strlcpy(__entry->chn, chn, 20);
		__entry->rd_cnt	= rd_cnt;
		__entry->rd_bw	= rd_bw;
		__entry->rd_lty	= rd_lty;
		__entry->wr_cnt	= wr_cnt;
		__entry->wr_bw	= wr_bw;
		__entry->wr_lty	= wr_lty;
	),

	TP_printk("%s %u %u %u %u %u %u", __entry->chn,
		  __entry->rd_cnt, __entry->rd_bw, __entry->rd_lty,
		  __entry->wr_cnt, __entry->wr_bw, __entry->wr_lty)
);
#endif /* _TRACE_PTM_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE sprd_ptm_trace
#include <trace/define_trace.h>
