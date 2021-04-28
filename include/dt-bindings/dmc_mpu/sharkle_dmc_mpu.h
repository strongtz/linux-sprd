#include "common.h"

/* list master id */

/* Sub-sys AP */
#define AP_DAP			0x80
#define AP_CPU			0x81
#define AP_CE_S			0x82
#define AP_CE_P			0x89
#define AP_CE_AES_FDE		0x85
#define AP_SDIO0		0x83
#define AP_SDIO1		0x84
#define AP_EMMC			0x86
#define AP_NANDC		0x87
#define AP_USBOTG		0x88
#define AP_DISPC		0x8a
#define AP_GSP			0x8b
#define AP_VSP			0x8d
#define AP_DMA(chn)		(0xa0 | (chn))

/* Sub-sys MM */
#define MM_DCAM			0x90
#define MM_ISP			0x91
#define MM_CPP			0x8c
#define MM_JPG			0x92

/* Sub-sys GPU */
#define GPU_GPU			0x94

/* Sub-sys WCN */
#define WCN_M(num)		(0xc0 | (num))

/* Sub-sys WTLCP */
#define WTLCP_VDEC		0xc8
#define WTLCP_TDEC		0xc9
#define WTLCP_I_Q_PORT0		0xca
#define WTLCP_I_Q_PORT1		0xcb
#define WTLCP_HARQ_E2I		0xcc
#define WTLCP_HARQ_OUT_I2E	0xcd
#define WTLCP_FMRAM_CAP		0xce
#define WTLCP_LDSP_D		0xcf
#define WTLCP_LDSP_DMA		0xd0
#define WTLCP_TGDSP_D		0xd1
#define WTLCP_TGDSP_DMA		0xd2
#define WTLCP_LDSP_P		0xd3
#define WTLCP_LTE_CSDFE_M(num)	(0xd4 | (num))
#define WTLCP_LTE_CE_TFC_M0	0xd7
#define WTLCP_LTE_CE_CTP_M1	0xd8
#define WTLCP_LTE_CE_CEP_M2	0xd9
#define WTLCP_LTE_CE_DBUF_M3	0xda
#define WTLCP_LTE_DPDEC_M3	0xdb
#define WTLCP_LTE_ULCH_M4	0xdc
#define WTLCP_LTE_HSDL_M5	0xdd
#define WTLCP_WTL_TGDSP_P	0xde
#define WTLCP_WDMA1		0xdf

/* Sub-sys PUBCP */
#define PUBCP_CR5		0xf8
#define PUBCP_CR5_PERI		0xf9
#define PUBCP_DMA		0xfa
#define PUBCP_SEC0		0xfb
#define PUBCP_TFT		0xfc
#define PUBCP_DMA_LINK1		0xfd

/* Sub-sys AON */
#define AON_DMA(num)		(0xe0 | (num))
#define AON_WCN			0xf2
#define AON_CM4			0xf1
