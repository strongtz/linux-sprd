/*
 *  log, watchdog, loopcheck, reset, .
 *
 *  WCN log debug module header.
 *
 *  Copyright (C) 2017 Spreadtrum Company
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 */

#ifndef _RDC_DEBUG
#define _RDC_DEBUG

/* Functionmask for debug CP2 */
enum {
	/* depend on Ylog or not, 1:not depend */
	CP2_FLAG_YLOG              = 0x01,
	/* open or close ARMlog, 1: close */
	CP2_FLAG_ARMLOG_EN         = 0x02,
	CP2_FLAG_RESET_EN          = 0x04,
	CP2_FLAG_WATCHDOG_EN       = 0x08,
	CP2_FLAG_SLEEP_EN          = 0x10,
	CP2_FLAG_LOOPCHECK_EN      = 0x20,
	/* 0: SDIO log, 1: UART log */
	CP2_FLAG_SWITCH_LOG_EN     = 0x40,
};

extern char functionmask[8];
extern struct completion dumpmem_complete;
int wcn_debug_init(void);
int log_rx_callback(void *addr, unsigned int len, unsigned int fifo_id);

#endif
