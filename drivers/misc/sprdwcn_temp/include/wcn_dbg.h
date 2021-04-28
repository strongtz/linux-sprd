#ifndef __WCN_DBG_H__
#define __WCN_DBG_H__

#ifdef pr_fmt
#undef pr_fmt
#endif

#define MDBG_DEBUG_MODE 0
#define WCN_DEBUG_ON 1
#define WCN_DEBUG_OFF 0

extern u32 wcn_print_level;

#define pr_fmt(fmt) "WCN BASE" fmt

#define WCN_INFO(fmt, args...)\
	pr_info(": " fmt, ## args)

#define WCN_ERR(fmt, args...)\
	pr_err(" error: " fmt, ## args)

#define WCN_DBG(fmt, args...)\
	pr_debug(" dbg: " fmt, ## args)

#define WCN_DEBUG(fmt, args...) do { \
	if (wcn_print_level ==  WCN_DEBUG_ON)\
		pr_info(" debug: " fmt, ## args);\
} while (0)

#if MDBG_DEBUG_MODE
#define WCN_LOG(fmt, args...)\
	pr_err(" dbg_err: %s  %d:" fmt \
	       "\n", __func__, __LINE__, ## args)
#else
#define WCN_LOG(fmt, args...)
#endif

#define MDBG_FUNC_ENTERY        WCN_LOG("ENTER.")
#define MDBG_FUNC_EXIT          WCN_LOG("EXIT.")

#endif
