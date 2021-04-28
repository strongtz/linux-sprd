#ifndef __SLP_DBG_H__
#define __SLP_DBG_H__

#include "../include/wcn_dbg.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "WCN SLP_MGR" fmt

#endif
