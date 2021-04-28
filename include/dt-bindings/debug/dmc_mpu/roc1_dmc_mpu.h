#ifndef __DT_BINDINGS_DEBUG_ROC1_DMC_MPU_H__
#define __DT_BINDINGS_DEBUG_ROC1_DMC_MPU_H__

#include "../common.h"

/* list master id */
/* Sub-sys AP */
#define AP_DAP			0x80
#define AP_CPU			0x81
#define ETR			0x82
#define AP_DISP			0x83
#define AP_VDSP_EPP		0x83
#define AP_VDSP_EDP		0x84
#define AP_VDMA			0x86
#define AP_CE_SEC		0x87
#define AP_CE_PUB		0x88
#define AP_CE_FDE		0x89
#define AP_SDIO0		0x8a
#define AP_SDIO1		0x8b
#define AP_SDIO2		0x8c
#define AP_EMMC			0x8d
#define AP_VSP			0x8E
#define AP_VDSP_M0		0x8F
#define AP_UFS			0xC1

/* MM */
#define DCAM_IF			0xc0
#define ISP_YUV			0xc2
#define VSP_DSP			0xc3
#define VSP_IDMA		0xc4
#define JPG			0xc5
#define CPP			0xc6
#define MM_FD			0xc7

/* PUBCP */
#define PUBCP_CR5		0xc8
#define PUBCP_CR5_PERI		0xc9
#define PUBCP_DMA		0xca
#define PUBCP_SEC0		0xcb
#define PUBCP_TFT		0xcc
#define PUBCP_DMA_LINK0		0xcd
#define PUBCP_DMA_LINK_4C	0Xce
#define PUBCP_SDIO		0xcf

/* WTLCP */
#define HU3GE_A_VDEC		0xe0
#define HU3GE_B_VDEC		0xe8
#define LDSP_P			0xd2
#define LDSP_D			0xd3
#define TGDPS_P			0xdf
#define TGDPS_D			0xde
#define LDSP_DMA		0xef
#define TGDSP_DMA		0xe7
#define LTE_CSDFE_RXDFE		0xd4
#define LTE_CE_MEAS		0xd5
#define LTE_CE_ICSNCS		0xd6
#define LTE_CE_TFC		0xd7
#define LTE_CE_CTP		0xd8
#define LTE_CE_CEP		0xd9
#define LTE_CE_DBUF 		0xda
#define LTE_DPFEC		0xdb
#define LTE_ULCH		0xdc
#define LTE_HSDL		0xdd

/* AGCP */
#define AGDSP_L			0xf4
#define AGDSP_D			0xf5
#define DMA_AP			0xf6
#define DMA_CP			0xf7

/* GPU */
#define GPU			0x90

/* SP */
#define USBOTG			0xf1
#define TMC			0xf2
#define SP_CM4			0xf3

/* AI */
#define AI_CAMBRICON		0xed
#define AI_POWERVR		0xec

/* SPU */
#define SPU			0xe2

/* IPA */
#define PCIE2			0xe3
#define PCIE3			0xe4
#define PAM_U3			0xe5
#define PAM_IPA			0xe6
#define PAM0			0xe9
#define PAM_WIFI		0xea
#define USB3			0xeb
#define IPA_CM4			0xee

#endif /* __DT_BINDINGS_DEBUG_ROC1_DMC_MPU_H__ */

