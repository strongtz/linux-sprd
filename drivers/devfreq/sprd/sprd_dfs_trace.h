#undef TRACE_SYSTEM
#define TRACE_SYSTEM sprd_ddr_dfs

#if !defined(_TRACE_SPRD_DDR_DFS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SPRD_DDR_DFS_H

#include <linux/tracepoint.h>
#include <linux/binfmts.h>
#include <linux/sprd_dfs_drv.h>

TRACE_EVENT(sprd_dfs_sysfs,
	TP_PROTO(const char *func_, int arg),
	TP_ARGS(func_, arg),
	TP_STRUCT__entry(
	    __string(func_, func_)
	    __field(int, arg)
	),
	TP_fast_assign(
	    __assign_str(func_, func_);
	    __entry->arg = arg;
	),
	TP_printk("dfs governor:%s; arg:%d\n",
	    __get_str(func_), __entry->arg)
);

TRACE_EVENT(sprd_scene,
	TP_PROTO(struct scene_freq *scene, unsigned int set),
	TP_ARGS(scene, set),
	TP_STRUCT__entry(
	    __field(char *, scene)
	    __field(unsigned int, set)
	    __field(unsigned int, freq)
	    __field(unsigned int, count)
	),
	TP_fast_assign(
	    __entry->set = set;
	    __entry->scene = scene->scene_name;
	    __entry->freq = scene->scene_freq;
	    __entry->count = scene->scene_count;
	),
	TP_printk("dfs scene:%s; freq:%d; set:%d; count:%d\n",
	    __entry->scene, __entry->freq, __entry->set, __entry->count)
);

TRACE_EVENT(sprd_dfs_poll,
	TP_PROTO(unsigned int cur_freq, unsigned int ap_freq,
	    unsigned int cp_freq, unsigned int force_freq,
	    unsigned int on_off, unsigned int auto_on_off),
	TP_ARGS(cur_freq, ap_freq, cp_freq, force_freq, on_off, auto_on_off),
	TP_STRUCT__entry(
	    __field(unsigned int, cur_freq)
	    __field(unsigned int, ap_freq)
	    __field(unsigned int, cp_freq)
	    __field(unsigned int, force_freq)
	    __field(unsigned int, on_off)
	    __field(unsigned int, auto_on_off)
	),
	TP_fast_assign(
	    __entry->cur_freq = cur_freq;
	    __entry->ap_freq = ap_freq;
	    __entry->cp_freq = cp_freq;
	    __entry->force_freq = force_freq;
	    __entry->on_off = on_off;
	    __entry->auto_on_off = auto_on_off;
	),
	TP_printk("dfs cur_freq:%d; ap_freq:%d; cp_freq:%d; force_freq:%d; status:%d; auto_status:%d\n",
	    __entry->cur_freq, __entry->ap_freq, __entry->cp_freq,
	    __entry->force_freq, __entry->on_off, __entry->auto_on_off)
);
#endif /* _TRACE_SPRD_DDR_DFS_H */


/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sprd_dfs_trace
#include <trace/define_trace.h>
