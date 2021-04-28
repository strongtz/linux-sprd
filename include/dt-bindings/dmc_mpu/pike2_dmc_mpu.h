#include "common.h"

/* list master id */

/* Sub-sys AP */
#define AP_DAP		0x80
#define AP_CPU		0x81
#define AP_CE_SEC	0x82
#define AP_SDIO		0x83
#define AP_NANDC	0x85
#define AP_EMMC		0x86
#define AP_USBOTG	0x87
#define AP_CE_PUB	0x88
#define AP_DISPC	0x89
#define AP_GSP		0x8a
#define AP_CE_FDE_AES	0x8b
#define AP_DMA(n)	(0xa0 + (n))

/* Sub-sys MM */
#define DCAM		0x90
#define ISP		0x91
#define VSP		0x92
#define JPG		0x93

/* Sub-sys GPU */
#define GPU		0x94

/* Sub-sys WCN */
#define WCN_M(n)	(0xc0 + (n))

/* Sub-sys CP */
#define CP		(0xc8)

/* Sub-sys AON */
#define AON_DMA(n)	(0xe0 + (n))
#define AON_WCN		0xf2
#define AON_SP		0xf3
#define AON_RF		0xF4
