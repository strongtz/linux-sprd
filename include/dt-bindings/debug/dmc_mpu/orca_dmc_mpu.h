#ifndef __DT_BINDINGS_DEBUG_ORCA_DMC_MPU_H__
#define __DT_BINDINGS_DEBUG_ORCA_DMC_MPU_H__

#include "../common.h"

/* list master id */
/* Sub-sys AP */
#define AP_DMA			0x01
#define SDIO_MST		0x02
#define NANDC			0x03
#define EMMC			0x04
#define USB3_0			0x05
#define IPA			0x06
#define TFT			0x07
#define SDIO_SLV		0x08
#define USB3_1			0x09
#define USB_PAM			0x0A
#define PCIE			0x0B
#define PAM_WIFI		0x0C
#define ETR			0x0E
#define AP_CPU			0x0F

/* AON */
#define DAP			0x11
#define CM4			0x12

/* V3_Modem */
#define V3_PS_CR5		0x20
#define V3_PS_LLPP		0x21
#define V3_PS_SEC		0x22
#define V3_PS_TFT		0x23
#define V3_PS_DMALINK		0x24
#define V3_PS_DMALINK_4C	0x25
#define V3_PS_DMA		0x26
#define V3_PS_SDIO		0x27
#define V3_PHY_CR5		0x28
#define V3_PHY_LLPP		0x29
#define V3_PHY_DMA		0x2A
#define V3_PHY_DMALINK0		0x2B

/* LW_PROC */
#define LW_DBG_HSDL		0X2E
#define LW_DBG_DBUF		0X2F
#define WFEC_B_harq_e2i		0X30
#define WFEC_B_harq_i2e		0X31
#define WFEC_B_dpa_sym_cap0	0X32
#define WFEC_B_dpa_sym_cap1	0X33
#define WFEC_B_tdec_dbits_du	0X34
#define LTE_CSDFE_CELLP		0x35
#define LTE_CSDFE_TFC		0x36
#define LTE_CE_CEP		0x37
#define WFEC_A_tdec_dbits_du	0X38
#define RESERVED		0x39
#define WFEC_A_iq_dump_port_1	0X3A
#define WFEC_A_iq_dump_port_2	0X3B
#define LTE_CE_CTP		0X3C
#define LTE_ULCH		0X3D
#define LTE_XSP			0X3E
#define LTE_DPFEC_LFEC		0X3F

/* PS_CP */
#define CR8_M0			0x40
#define PS_CP_CR8_LLPP		0x41
#define PS_CP_CR8_FPP0		0x42
#define PS_CP_CR8_FPP1		0x43
#define V3_SEC			0x44
#define DMA_LINK		0x45
#define DMA_LINK_4C		0x46
#define PS_CP DMA		0x47
#define SDIO			0x48
#define NR_SEC0			0x49
#define NR_SEC1			0x4A

/* NR_CP */
#define CR8_PL310		0x50
#define NR_CP_CR8_LLPP		0x51
#define NR_CP_CR8_FPP0		0x52
#define NR_CP_CR8_FPP1		0x53
#define CR8_DMA			0x54
#define CR8_SEC			0x55
#define XC0_EPP			0x56
#define XC0_EDP			0x57
#define XC1_EPP			0x58
#define XC1_EDP			0x59
#define XC0_DMA			0x5A
#define UL_MAC			0x5B
#define UL_DFE			0x5C
#define UL_HSDL			0x5D
#define NR_CS			0x5E
#define NR_DBUF0		0x5F
#define NR_DBEF1		0x60
#define NR_DLMAC		0x61
#define NR_FEC			0x62
#define NR_CSI			0x63
#define DL_HSDL			0x64
#define SYNC_HSDL		0x65

/* AUD_CP */
#define DSP_P			0x74
#define DSP_D			0x75
#define DMA_AP			0x76
#define DMA_CP			0x77

#endif /* __DT_BINDINGS_DEBUG_ORCA_DMC_MPU_H__ */

