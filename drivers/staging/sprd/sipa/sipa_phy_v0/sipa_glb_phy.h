#ifndef _IPA_GLB_REGS_H_
#define _IPA_GLB_REGS_H_

#include <linux/bitops.h>
#include "../sipa_hal_priv.h"

#define ZERO							0l
#define REG_AP_IPA_AHB_RF_AHB_EB				0x0000
#define REG_AP_IPA_AHB_RF_AHB_RST				0x0004
#define REG_AP_IPA_AHB_RF_AP_SYS_FORCE_SLEEP_CFG                0x0008
#define REG_AP_IPA_AHB_RF_USB1_CTRL                             0x000c
#define REG_AP_IPA_AHB_RF_USB1_DBG0                             0x0014
#define REG_AP_IPA_AHB_RF_USB1_DBG1                             0x0018
#define REG_AP_IPA_AHB_RF_USB1_DBG2                             0x001c
#define REG_AP_IPA_AHB_RF_PCIE_CTL0                             0x002c
#define REG_AP_IPA_AHB_RF_PCIE_CTL1                             0x0030
#define REG_AP_IPA_AHB_RF_PCIE_CTL2                             0x0034
#define REG_AP_IPA_AHB_RF_PCIE_CTL3                             0x0038
#define REG_AP_IPA_AHB_RF_APB_PCLK_AUTO_GATE_EB                 0x003c
#define REG_AP_IPA_AHB_RF_APB_PCLK_AUTO_SLOW_SEL                0x0040
#define REG_AP_IPA_AHB_RF_AP_SYS_AUTO_SLEEP_CFG			0x004c
#define REG_AP_IPA_AHB_RF_IMTX_M0_LPC                           0x0100
#define REG_AP_IPA_AHB_RF_IMTX_M1_LPC				0x0104
#define REG_AP_IPA_AHB_RF_IMTX_M2_LPC                           0x0108
#define REG_AP_IPA_AHB_RF_IMTX_M3_LPC                           0x010c
#define REG_AP_IPA_AHB_RF_IMTX_M4_LPC                           0x0110
#define REG_AP_IPA_AHB_RF_IMTX_M5_LPC                           0x0114
#define REG_AP_IPA_AHB_RF_IMTX_M6_LPC                           0x0118
#define REG_AP_IPA_AHB_RF_IMTX_M7_LPC                           0x011c
#define REG_AP_IPA_AHB_RF_IMTX_M8_LPC                           0x0120
#define REG_AP_IPA_AHB_RF_IMTX_M9_LPC                           0x0124
#define REG_AP_IPA_AHB_RF_IMTX_MAIN_LPC                         0x013c
#define REG_AP_IPA_AHB_RF_IMTX_S0_LPC                           0x0140
#define REG_AP_IPA_AHB_RF_IMTX_S1_LPC                           0x0144
#define REG_AP_IPA_AHB_RF_IMTX_S2_LPC                           0x0148
#define REG_AP_IPA_AHB_RF_IMTX_S3_LPC                           0x014c
#define REG_AP_IPA_AHB_RF_IMTX_S4_LPC                           0x0150
#define REG_AP_IPA_AHB_RF_IMTX_S5_LPC                           0x0154
#define REG_AP_IPA_AHB_RF_IMTX_S6_LPC                           0x0158
#define REG_AP_IPA_AHB_RF_IMTX_S7_LPC                           0x015c
#define REG_AP_IPA_AHB_RF_IMTX_S8_LPC                           0x0160
#define REG_AP_IPA_AHB_RF_IMTX_S9_LPC                           0x0164
#define REG_AP_IPA_AHB_RF_IMTX_MST_FRC_LSLP                     0x0180
#define REG_AP_IPA_AHB_RF_IMTX_LSLP_LPC_BYPASS                  0x0184
#define REG_AP_IPA_AHB_RF_IMTX_MST_FRC_PUB_DSLP                 0x0188
#define REG_AP_IPA_AHB_RF_IMTX_PUB_DSLP_LPC_BYPASS              0x018c
#define REG_AP_IPA_AHB_RF_IMTX_MST_FRC_DOZE                     0x0190
#define REG_AP_IPA_AHB_RF_IMTX_DOZE_LPC_BYPASS                  0x0194
#define REG_AP_IPA_AHB_RF_IMTX_AXI_FREQ_ALLOW0                  0x0198
#define REG_AP_IPA_AHB_RF_IMTX_AXI_FREQ_ALLOW1                  0x019c
#define REG_AP_IPA_AHB_RF_IMTX_AXI_FREQ_LSLP_ALLOW0             0x01a0
#define REG_AP_IPA_AHB_RF_IMTX_AXI_FREQ_LSLP_ALLOW1             0x01a4
#define REG_AP_IPA_AHB_RF_AP_QOS0                               0x0300
#define REG_AP_IPA_AHB_RF_AP_QOS1                               0x0304
#define REG_AP_IPA_AHB_RF_AP_URGENCY                            0x0308
#define REG_AP_IPA_AHB_RF_AP_USER0                              0x030c
#define REG_AP_IPA_AHB_RF_AP_USER1				0x0310
#define REG_AP_IPA_AHB_RF_AP_USER2                              0x0314
#define REG_AP_IPA_AHB_RF_IMTX_CTRL                             0x0318
#define REG_AP_IPA_AHB_RF_IPA_CTRL                              0x031c
#define REG_AP_IPA_AHB_RF_PAM_WIFI_CTRL                         0x0320
#define REG_AP_IPA_AHB_RF_IMTX_M10_CTRL                         0x0324
#define REG_AP_IPA_AHB_RF_PCIE_LP_CTRL                          0x0328
#define REG_AP_IPA_AHB_RF_IMTX_PU_NUM0                          0x032c
#define REG_AP_IPA_AHB_RF_IMTX_PU_NUM1                          0x0330
#define REG_AP_IPA_AHB_RF_IMTX_PU_NUM2                          0x0334
#define REG_AP_IPA_AHB_RF_IMTX_PU_NUM3                          0x0338
#define REG_AP_IPA_AHB_RF_IMTX_PU_NUM4                          0x033c
#define REG_AP_IPA_AHB_RF_IMTX_PU_NUM5                          0x0340
#define REG_AP_IPA_AHB_RF_PAM_WIFI_PCLK_DFS_CTRL                0x0344
#define REG_AP_IPA_AHB_RF_PCIE_CTRL                             0x0348
#define REG_AP_IPA_AHB_RF_SYS_ACCESS_EN                         0x034c
#define REG_AP_IPA_AHB_RF_CGM_GATE_EN                           0x0350
#define REG_AP_IPA_AHB_RF_IP_BUSY_GATE_EN                       0x0354
#define REG_AP_IPA_AHB_RF_SDSLV_CTRL                            0x0358
#define REG_AP_IPA_AHB_RF_PCIE_SW_MSI_INT                       0x035c
#define REG_AP_IPA_AHB_RF_RESERVE0                              0x0400
#define REG_AP_IPA_AHB_RF_RESERVE1                              0x0404
/**
 * Mode_n_Flow_Ctrl
 * [31]: ipa_working_mode:
 *         0: Normal
 *         1: Bypass
 * [30:29]: USB_Mode
 *         00: RNDIS
 *         01: MBIM
 *         10 and 11 are reserved for future use
 * [28]: need_cp_through_PCIE
 *       0: do not need to send data to CP behind PCIE;
 *       1: need to send data to CP behind PCIE.
 * [27]: SW_pause_IPA:
 *       0: do not pause IPA
 *       1: SW want IPA to pause
 * [26]: HW_ready_for_check
 *       0: HW is running
 *       1: HW is paused
 * [25]: SW_resume_IPA
 *       0: SW does not resume IPA
 *       1: SW wants IPA to resume
 * [24:17]: MSB 8 bits of SW debug memory. used with SW_pause
 * [16]: Reserved
 * [15]: WiAP_DL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [14]: WIFI_DL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [13]: USB_DL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [12]: SDIO_DL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [11]: mAP_RX_DL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [10]: PCIE_DL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [9]: PCIE_Ch0_DL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [8]: PCIE_Ch1_DL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [7]: PCIE_Ch2_DL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [6]: PCIE_Ch3_DL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [5]: CP_UL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [4]: mAP_RX_UL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [3]: mAP_PCIE_Ch0_UL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [2]: mAP_PCIE_Ch1_UL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [1]: mAP_PCIE_Ch2_UL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 * [0]: mAP_PCIE_Ch3_UL_FLOW_CTL
 *       1: in Flow ctrl state
 *       0: not in Flow ctrl state
 */
#define IPA_WORKING_MODE_MASK			(BIT(31))
#define IPA_USB_MODE_MASK			(0x60000000l)
#define IPA_NEED_CP_THROUGH_PCIE_MASK		(BIT(28))
#define IPA_SW_PAUSE_IPA_MASK			(BIT(27))
#define IPA_HW_READY_FOR_CHECK_MASK		(BIT(26))
#define IPA_SW_RESUME_IPA_MASK			(BIT(25))
#define IPA_SW_DEBUG_MEM_ADDRH_MASK		(0x1FE0000l)
#define IPA_CP_WORK_STATUS			(BIT(16))
#define IPA_WIAP_DL_FLOW_CTL_MASK		(BIT(15))
#define IPA_WIFI_DL_FLOW_CTL_MASK		(BIT(14))
#define IPA_USB_DL_FLOW_CTL_MASK		(BIT(13))
#define IPA_SDIO_DL_FLOW_CTL_MASK		(BIT(12))
#define IPA_MAP_RX_DL_FLOW_CTL_MASK		(BIT(11))
#define IPA_PCIE_DL_FLOW_CTL_MASK		(BIT(10))
#define IPA_PCIE_CH0_DL_FLOW_CTL_MASK		(BIT(9))
#define IPA_PCIE_CH1_DL_FLOW_CTL_MASK		(BIT(8))
#define IPA_PCIE_CH2_DL_FLOW_CTL_MASK		(BIT(7))
#define IPA_PCIE_CH3_DL_FLOW_CTL_MASK		(BIT{6})
#define IPA_CP_UL_FLOW_CTL_MASK			(BIT(5))
#define IPA_MAP_RX_UL_FLOW_CTL_MASK		(BIT(4))
#define IPA_MAP_PCIE_CH0_UL_FLOW_CTL_MASK	(BIT(3))
#define IPA_MAP_PCIE_CH1_UL_FLOW_CTL_MASK	(BIT(2))
#define IPA_MAP_PCIE_CH2_UL_FLOW_CTL_MASK	(BIT(1))
#define IPA_MAP_PCIE_CH3_UL_FLOW_CTL_MASK	(BIT(0))
#define IPA_MODE_N_FLOWCTRL			(0x00l)

/*One receiver fifo correspond to many sender fifo*/

/**
 * USB_SDIO_flowctl_Src
 *   [30:16]: USB_DL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 *   [14:0]: SDIO_DL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in USB_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 */
#define USB_DL_SRC_BLK_MAP_DL			~(BIT(16))
#define USB_DL_SRC_BLK_MAP_CTL0			~(BIT(17))
#define USB_DL_SRC_BLK_MAP_CTL1			~(BIT(18))
#define USB_DL_SRC_BLK_MAP_CTL2			~(BIT(19))
#define USB_DL_SRC_BLK_MAP_CTL3			~(BIT(20))
#define USB_DL_SRC_BLK_CP_DL			~(BIT(21))
#define USB_DL_SRC_BLK_PCIE_UL			~(BIT(22))
#define USB_DL_SRC_BLK_SDIO_UL			~(BIT(23))
#define USB_DL_SRC_BLK_MAP_UL			~(BIT(24))
#define USB_DL_SRC_BLK_PCIE_CTL0_UL		~(BIT(25))
#define USB_DL_SRC_BLK_PCIE_CTL1_UL		~(BIT(26))
#define USB_DL_SRC_BLK_PCIE_CTL2_UL		~(BIT(27))
#define USB_DL_SRC_BLK_PCIE_CTL3_UL		~(BIT(28))
#define USB_DL_SRC_BLK_WIFI_DL			~(BIT(29))
#define USB_DL_SRC_BLK_WIAP_DL			~(BIT(30))

#define SDIO_DL_SRC_BLK_MAP_DL			~(BIT(0))
#define SDIO_DL_SRC_BLK_MAP_CTL0		~(BIT(1))
#define SDIO_DL_SRC_BLK_MAP_CTL1		~(BIT(2))
#define SDIO_DL_SRC_BLK_MAP_CTL2		~(BIT(3))
#define SDIO_DL_SRC_BLK_MAP_CTL3		~(BIT(4))
#define SDIO_DL_SRC_BLK_CP_DL			~(BIT(5))
#define SDIO_DL_SRC_BLK_PCIE_UL			~(BIT(6))
#define SDIO_DL_SRC_BLK_USB_UL			~(BIT(7))
#define SDIO_DL_SRC_BLK_MAP_UL			~(BIT(8))
#define SDIO_DL_SRC_BLK_PCIE_CTL0_UL		~(BIT(9))
#define SDIO_DL_SRC_BLK_PCIE_CTL1_UL		~(BIT(10))
#define SDIO_DL_SRC_BLK_PCIE_CTL2_UL		~(BIT(11))
#define SDIO_DL_SRC_BLK_PCIE_CTL3_UL		~(BIT(12))
#define SDIO_DL_SRC_BLK_WIFI_DL			~(BIT(13))
#define SDIO_DL_SRC_BLK_WIAP_DL			~(BIT(14))

#define IPA_USB_SDIO_FLOWCTL_SRC		(0x04l)

/**
 * mAP_RX_DL_PCIE_UL_flowctl_Src
 *   [30:16]: mAP_RX_DL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in USB_UL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 *   [14:0]: PCIE_DL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in USB_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 */
#define MAP_RX_DL_SRC_BLK_USB_UL		~(BIT(16))
#define MAP_RX_DL_SRC_BLK_MAP_CTL0		~(BIT(17))
#define MAP_RX_DL_SRC_BLK_MAP_CTL1		~(BIT(18))
#define MAP_RX_DL_SRC_BLK_MAP_CTL2		~(BIT(19))
#define MAP_RX_DL_SRC_BLK_MAP_CTL3		~(BIT(20))
#define MAP_RX_DL_SRC_BLK_CP_DL			~(BIT(21))
#define MAP_RX_DL_SRC_BLK_PCIE_UL		~(BIT(22))
#define MAP_RX_DL_SRC_BLK_SDIO_UL		~(BIT(23))
#define MAP_RX_DL_SRC_BLK_MAP_UL		~(BIT(24))
#define MAP_RX_DL_SRC_BLK_PCIE_CTL0_UL		~(BIT(25))
#define MAP_RX_DL_SRC_BLK_PCIE_CTL1_UL		~(BIT(26))
#define MAP_RX_DL_SRC_BLK_PCIE_CTL2_UL		~(BIT(27))
#define MAP_RX_DL_SRC_BLK_PCIE_CTL3_UL		~(BIT(28))
#define MAP_RX_DL_SRC_BLK_WIFI_DL		~(BIT(29))
#define MAP_RX_DL_SRC_BLK_WIAP_DL		~(BIT(30))
#define TO_PCIE_NO_MAC_HEADER			BIT(31)

#define PCIE_DL_SRC_BLK_MAP_DL			~(BIT(0))
#define PCIE_DL_SRC_BLK_MAP_CTL0		~(BIT(1))
#define PCIE_DL_SRC_BLK_MAP_CTL1		~(BIT(2))
#define PCIE_DL_SRC_BLK_MAP_CTL2		~(BIT(3))
#define PCIE_DL_SRC_BLK_MAP_CTL3		~(BIT(4))
#define PCIE_DL_SRC_BLK_CP_DL			~(BIT(5))
#define PCIE_DL_SRC_BLK_USB_UL			~(BIT(6))
#define PCIE_DL_SRC_BLK_SDIO_UL			~(BIT(7))
#define PCIE_DL_SRC_BLK_MAP_UL			~(BIT(8))
#define PCIE_DL_SRC_BLK_PCIE_CTL0_UL		~(BIT(9))
#define PCIE_DL_SRC_BLK_PCIE_CTL1_UL		~(BIT(10))
#define PCIE_DL_SRC_BLK_PCIE_CTL2_UL		~(BIT(11))
#define PCIE_DL_SRC_BLK_PCIE_CTL3_UL		~(BIT(12))
#define PCIE_DL_SRC_BLK_WIFI_DL			~(BIT(13))
#define PCIE_DL_SRC_BLK_WIAP_DL			~(BIT(14))
#define FROM_PCIE_NO_MAC_HEADER			BIT(15)

#define IPA_MAP_RX_DL_PCIE_UL_FLOWCTL_SRC	(0x08l)

/**
 * PCIE_Ch0_1_DL_flowctl_Src
 *   [30:16]: PCIE_Ch0_DL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL:
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in USB_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 *   [14:0]: PCIE_Ch1_DL_FLOW_Ctl_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in USB_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 */
#define PCIE_CH0_DL_SRC_BLK_MAP_DL		(BIT(16))
#define PCIE_CH0_DL_SRC_BLK_MAP_CTL0		(BIT(17))
#define PCIE_CH0_DL_SRC_BLK_MAP_CTL1		(BIT(18))
#define PCIE_CH0_DL_SRC_BLK_MAP_CTL2		(BIT(19))
#define PCIE_CH0_DL_SRC_BLK_MAP_CTL3		(BIT(20))
#define PCIE_CH0_DL_SRC_BLK_CP_DL		(BIT(21))
#define PCIE_CH0_DL_SRC_BLK_PCIE_UL		(BIT(22))
#define PCIE_CH0_DL_SRC_BLK_SDIO_UL		(BIT(23))
#define PCIE_CH0_DL_SRC_BLK_MAP_UL		(BIT(24))
#define PCIE_CH0_DL_SRC_BLK_USB_UL		(BIT(25))
#define PCIE_CH0_DL_SRC_BLK_PCIE_CTL1_UL	(BIT(26))
#define PCIE_CH0_DL_SRC_BLK_PCIE_CTL2_UL	(BIT(27))
#define PCIE_CH0_DL_SRC_BLK_PCIE_CTL3_UL	(BIT(28))
#define PCIE_CH0_DL_SRC_BLK_WIFI_DL		(BIT(29))
#define PCIE_CH0_DL_SRC_BLK_WIAP_DL		(BIT(30))

#define PCIE_CH1_DL_SRC_BLK_MAP_DL		(BIT(0))
#define PCIE_CH1_DL_SRC_BLK_MAP_CTL0		(BIT(1))
#define PCIE_CH1_DL_SRC_BLK_MAP_CTL1		(BIT(2))
#define PCIE_CH1_DL_SRC_BLK_MAP_CTL2		(BIT(3))
#define PCIE_CH1_DL_SRC_BLK_MAP_CTL3		(BIT(4))
#define PCIE_CH1_DL_SRC_BLK_CP_DL		(BIT(5))
#define PCIE_CH1_DL_SRC_BLK_PCIE_UL		(BIT(6))
#define PCIE_CH1_DL_SRC_BLK_SDIO_UL		(BIT(7))
#define PCIE_CH1_DL_SRC_BLK_MAP_UL		(BIT(8))
#define PCIE_CH1_DL_SRC_BLK_PCIE_CTL0_UL	(BIT(9))
#define PCIE_CH1_DL_SRC_BLK_USB_UL		(BIT(10))
#define PCIE_CH1_DL_SRC_BLK_PCIE_CTL2_UL	(BIT(11))
#define PCIE_CH1_DL_SRC_BLK_PCIE_CTL3_UL	(BIT(12))
#define PCIE_CH1_DL_SRC_BLK_WIFI_DL		(BIT(13))
#define PCIE_CH1_DL_SRC_BLK_WIAP_DL		(BIT(14))

#define IPA_PCIE_CH0_1_DL_FLOWCTL_SRC		(0x0Cl)

/**
 * PCIE_Ch2_3_DL_flowctl_Src
 *   [30:16]: PCIE_Ch2_DL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL:
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in USB_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 *   [14:0]: PCIE_Ch3_DL_FLOW_Ctl_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in USB_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 */
#define PCIE_CH2_DL_SRC_BLK_MAP_DL		(BIT(16))
#define PCIE_CH2_DL_SRC_BLK_MAP_CTL0		(BIT(17))
#define PCIE_CH2_DL_SRC_BLK_MAP_CTL1		(BIT(18))
#define PCIE_CH2_DL_SRC_BLK_MAP_CTL2		(BIT(19))
#define PCIE_CH2_DL_SRC_BLK_MAP_CTL3		(BIT(20))
#define PCIE_CH2_DL_SRC_BLK_CP_DL		(BIT(21))
#define PCIE_CH2_DL_SRC_BLK_PCIE_UL		(BIT(22))
#define PCIE_CH2_DL_SRC_BLK_SDIO_UL		(BIT(23))
#define PCIE_CH2_DL_SRC_BLK_MAP_UL		(BIT(24))
#define PCIE_CH2_DL_SRC_BLK_PCIE_CTL0_UL	(BIT(25))
#define PCIE_CH2_DL_SRC_BLK_PCIE_CTL1_UL	(BIT(26))
#define PCIE_CH2_DL_SRC_BLK_USB_DL		(BIT(27))
#define PCIE_CH2_DL_SRC_BLK_PCIE_CTL3_UL	(BIT(28))
#define PCIE_CH2_DL_SRC_BLK_WIFI_DL		(BIT(29))
#define PCIE_CH2_DL_SRC_BLK_WIAP_DL		(BIT(30))

#define PCIE_CH3_DL_SRC_BLK_MAP_DL		(BIT(0))
#define PCIE_CH3_DL_SRC_BLK_MAP_CTL0		(BIT(1))
#define PCIE_CH3_DL_SRC_BLK_MAP_CTL1		(BIT(2))
#define PCIE_CH3_DL_SRC_BLK_MAP_CTL2		(BIT(3))
#define PCIE_CH3_DL_SRC_BLK_MAP_CTL3		(BIT(4))
#define PCIE_CH3_DL_SRC_BLK_CP_DL		(BIT(5))
#define PCIE_CH3_DL_SRC_BLK_PCIE_UL		(BIT(6))
#define PCIE_CH3_DL_SRC_BLK_SDIO_UL		(BIT(7))
#define PCIE_CH3_DL_SRC_BLK_MAP_UL		(BIT(8))
#define PCIE_CH3_DL_SRC_BLK_PCIE_CTL0_UL	(BIT(9))
#define PCIE_CH3_DL_SRC_BLK_PCIE_CTL1_UL	(BIT(10))
#define PCIE_CH3_DL_SRC_BLK_PCIE_CTL2_UL	(BIT(11))
#define PCIE_CH3_DL_SRC_BLK_USB_UL		(BIT(12))
#define PCIE_CH3_DL_SRC_BLK_WIFI_DL		(BIT(13))
#define PCIE_CH3_DL_SRC_BLK_WIAP_DL		(BIT(14))

#define IPA_PCIE_CH2_3_DL_FLOWCTL_SRC		(0x10l)

/**
 * CP_mAP_TX_UL_flowctl_Src
 *   [30:16]: CP_UL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in USB_UL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 *   [14:0]: mAP_RX_UL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in USB_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 */
#define CP_UL_SRC_BLK_MAP_DL			~(BIT(16))
#define CP_UL_SRC_BLK_MAP_CTL0			~(BIT(17))
#define CP_UL_SRC_BLK_MAP_CTL1			~(BIT(18))
#define CP_UL_SRC_BLK_MAP_CTL2			~(BIT(19))
#define CP_UL_SRC_BLK_MAP_CTL3			~(BIT(20))
#define CP_UL_SRC_BLK_USB_UL			~(BIT(21))
#define CP_UL_SRC_BLK_PCIE_UL			~(BIT(22))
#define CP_UL_SRC_BLK_SDIO_UL			~(BIT(23))
#define CP_UL_SRC_BLK_MAP_UL			~(BIT(24))
#define CP_UL_SRC_BLK_PCIE_CTL0_UL		~(BIT(25))
#define CP_UL_SRC_BLK_PCIE_CTL1_UL		~(BIT(26))
#define CP_UL_SRC_BLK_PCIE_CTL2_UL		~(BIT(27))
#define CP_UL_SRC_BLK_PCIE_CTL3_UL		~(BIT(28))
#define CP_UL_SRC_BLK_WIFI_DL			~(BIT(29))
#define CP_UL_SRC_BLK_WIAP_DL			~(BIT(30))

#define MAP_RX_UL_SRC_BLK_MAP_DL		~(BIT(0))
#define MAP_RX_UL_SRC_BLK_MAP_CTL0		~(BIT(1))
#define MAP_RX_UL_SRC_BLK_MAP_CTL1		~(BIT(2))
#define MAP_RX_UL_SRC_BLK_MAP_CTL2		~(BIT(3))
#define MAP_RX_UL_SRC_BLK_MAP_CTL3		~(BIT(4))
#define MAP_RX_UL_SRC_BLK_CP_DL			~(BIT(5))
#define MAP_RX_UL_SRC_BLK_PCIE_UL		~(BIT(6))
#define MAP_RX_UL_SRC_BLK_SDIO_UL		~(BIT(7))
#define MAP_RX_UL_SRC_BLK_USB_UL		~(BIT(8))
#define MAP_RX_UL_SRC_BLK_PCIE_CTL0_UL		~(BIT(9))
#define MAP_RX_UL_SRC_BLK_PCIE_CTL1_UL		~(BIT(10))
#define MAP_RX_UL_SRC_BLK_PCIE_CTL2_UL		~(BIT(11))
#define MAP_RX_UL_SRC_BLK_PCIE_CTL3_UL		~(BIT(12))
#define MAP_RX_UL_SRC_BLK_WIFI_DL		~(BIT(13))
#define MAP_RX_UL_SRC_BLK_WIAP_DL		~(BIT(14))

#define IPA_CP_MAP_TX_UL_FLOWCTL_SRC		(0x14l)

/**
 * mAP_PCIE_Ch0_1_UL_flowctl_Src
 *   [30:16]: mAP_PCIE_Ch0_UL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in USB_UL
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 *   [14:0]: mAP_PCIE_Ch1_UL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in USB_UL
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 */
#define MAP_PCIE_CH0_UL_SRC_BLK_MAP_DL		(BIT(16))
#define MAP_PCIE_CH0_UL_SRC_BLK_USB_UL		(BIT(17))
#define MAP_PCIE_CH0_UL_SRC_BLK_MAP_CTL1	(BIT(18))
#define MAP_PCIE_CH0_UL_SRC_BLK_MAP_CTL2	(BIT(19))
#define MAP_PCIE_CH0_UL_SRC_BLK_MAP_CTL3	(BIT(20))
#define MAP_PCIE_CH0_UL_SRC_BLK_CP_DL		(BIT(21))
#define MAP_PCIE_CH0_UL_SRC_BLK_PCIE_UL		(BIT(22))
#define MAP_PCIE_CH0_UL_SRC_BLK_SDIO_UL		(BIT(23))
#define MAP_PCIE_CH0_UL_SRC_BLK_MAP_UL		(BIT(24))
#define MAP_PCIE_CH0_UL_SRC_BLK_PCIE_CTL0_UL	(BIT(25))
#define MAP_PCIE_CH0_UL_SRC_BLK_PCIE_CTL1_UL	(BIT(26))
#define MAP_PCIE_CH0_UL_SRC_BLK_PCIE_CTL2_UL	(BIT(27))
#define MAP_PCIE_CH0_UL_SRC_BLK_PCIE_CTL3_UL	(BIT(28))
#define MAP_PCIE_CH0_UL_SRC_BLK_WIFI_DL		(BIT(29))
#define MAP_PCIE_CH0_UL_SRC_BLK_WIAP_DL		(BIT(30))

#define MAP_PCIE_CH1_UL_SRC_BLK_MAP_DL		(BIT(0))
#define MAP_PCIE_CH1_UL_SRC_BLK_MAP_CTL0	(BIT(1))
#define MAP_PCIE_CH1_UL_SRC_BLK_USB_UL		(BIT(2))
#define MAP_PCIE_CH1_UL_SRC_BLK_MAP_CTL2	(BIT(3))
#define MAP_PCIE_CH1_UL_SRC_BLK_MAP_CTL3	(BIT(4))
#define MAP_PCIE_CH1_UL_SRC_BLK_CP_DL		(BIT(5))
#define MAP_PCIE_CH1_UL_SRC_BLK_PCIE_UL		(BIT(6))
#define MAP_PCIE_CH1_UL_SRC_BLK_SDIO_UL		(BIT(7))
#define MAP_PCIE_CH1_UL_SRC_BLK_MAP_UL		(BIT(8))
#define MAP_PCIE_CH1_UL_SRC_BLK_PCIE_CTL0_UL	(BIT(9))
#define MAP_PCIE_CH1_UL_SRC_BLK_PCIE_CTL1_UL	(BIT(10))
#define MAP_PCIE_CH1_UL_SRC_BLK_PCIE_CTL2_UL	(BIT(11))
#define MAP_PCIE_CH1_UL_SRC_BLK_PCIE_CTL3_UL	(BIT(12))
#define MAP_PCIE_CH1_UL_SRC_BLK_WIFI_DL		(BIT(13))
#define MAP_PCIE_CH1_UL_SRC_BLK_WIAP_DL		(BIT(14))

#define IPA_MAP_PCIE_CH0_1_UL_FLOWCTL_SRC	(0x18l)

/**
 * mAP_PCIE_Ch2_3_UL_flowctl_Src
 *   [30:16]: mAP_PCIE_Ch2_UL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in USB_UL
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 *   [14:0]: mAP_PCIE_Ch3_UL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in USB_UL
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in WiAP_DL
 */
#define MAP_PCIE_CH2_UL_SRC_BLK_MAP_DL		(BIT(16))
#define MAP_PCIE_CH2_UL_SRC_BLK_MAP_CTL0	(BIT(17))
#define MAP_PCIE_CH2_UL_SRC_BLK_MAP_CTL1	(BIT(18))
#define MAP_PCIE_CH2_UL_SRC_BLK_USB_UL		(BIT(19))
#define MAP_PCIE_CH2_UL_SRC_BLK_MAP_CTL3	(BIT(20))
#define MAP_PCIE_CH2_UL_SRC_BLK_CP_DL		(BIT(21))
#define MAP_PCIE_CH2_UL_SRC_BLK_PCIE_UL		(BIT(22))
#define MAP_PCIE_CH2_UL_SRC_BLK_SDIO_UL		(BIT(23))
#define MAP_PCIE_CH2_UL_SRC_BLK_MAP_UL		(BIT(24))
#define MAP_PCIE_CH2_UL_SRC_BLK_PCIE_CTL0_UL	(BIT(25))
#define MAP_PCIE_CH2_UL_SRC_BLK_PCIE_CTL1_UL	(BIT(26))
#define MAP_PCIE_CH2_UL_SRC_BLK_PCIE_CTL2_UL	(BIT(27))
#define MAP_PCIE_CH2_UL_SRC_BLK_PCIE_CTL3_UL	(BIT(28))
#define MAP_PCIE_CH2_UL_SRC_BLK_WIFI_DL		(BIT(29))
#define MAP_PCIE_CH2_UL_SRC_BLK_WIAP_DL		(BIT(30))

#define MAP_PCIE_CH3_UL_SRC_BLK_MAP_DL		(BIT(0))
#define MAP_PCIE_CH3_UL_SRC_BLK_MAP_CTL0	(BIT(1))
#define MAP_PCIE_CH3_UL_SRC_BLK_MAP_CTL1	(BIT(2))
#define MAP_PCIE_CH3_UL_SRC_BLK_MAP_CTL2	(BIT(3))
#define MAP_PCIE_CH3_UL_SRC_BLK_USB_UL		(BIT(4))
#define MAP_PCIE_CH3_UL_SRC_BLK_CP_DL		(BIT(5))
#define MAP_PCIE_CH3_UL_SRC_BLK_PCIE_UL		(BIT(6))
#define MAP_PCIE_CH3_UL_SRC_BLK_SDIO_UL		(BIT(7))
#define MAP_PCIE_CH3_UL_SRC_BLK_MAP_UL		(BIT(8))
#define MAP_PCIE_CH3_UL_SRC_BLK_PCIE_CTL0_UL	(BIT(9))
#define MAP_PCIE_CH3_UL_SRC_BLK_PCIE_CTL1_UL	(BIT(10))
#define MAP_PCIE_CH3_UL_SRC_BLK_PCIE_CTL2_UL	(BIT(11))
#define MAP_PCIE_CH3_UL_SRC_BLK_PCIE_CTL3_UL	(BIT(12))
#define MAP_PCIE_CH3_UL_SRC_BLK_WIFI_DL		(BIT(13))
#define MAP_PCIE_CH3_UL_SRC_BLK_WIAP_DL		(BIT(14))

#define IPA_MAP_PCIE_CH2_3_UL_FLOWCTL_SRC	(0x1Cl)

/**
 * WIFI_WiAP_DL_flowctl_Src
 *   [30:16]: WIFI_DL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in USB_DL
 *     bit14: block flow ctl status update in WiAP_DL
 *   [14:0]: WiAP_DL_FLOW_CTL_to_Src_blk:
 *     bit0: block flow ctl status update in mAP_DL
 *     bit1: block flow ctl status update in mAP_CTL0
 *     bit2: block flow ctl status update in mAP_CTL1
 *     bit3: block flow ctl status update in mAP_CTL2
 *     bit4: block flow ctl status update in mAP_CTL3
 *     bit5: block flow ctl status update in CP_DL
 *     bit6: block flow ctl status update in PCIE_UL
 *     bit7: block flow ctl status update in SDIO_UL
 *     bit8: block flow ctl status update in mAP_UL
 *     bit9: block flow ctl status update in PCIE_CTL0_UL
 *     bit10: block flow ctl status update in PCIE_CTL1_UL
 *     bit11: block flow ctl status update in PCIE_CTL2_UL
 *     bit12: block flow ctl status update in PCIE_CTL3_UL
 *     bit13: block flow ctl status update in WIFI_DL
 *     bit14: block flow ctl status update in USB_DL
 */
#define WIFI_DL_SRC_BLK_MAP_DL			~(BIT(16))
#define WIFI_DL_SRC_BLK_MAP_CTL0		~(BIT(17))
#define WIFI_DL_SRC_BLK_MAP_CTL1		~(BIT(18))
#define WIFI_DL_SRC_BLK_MAP_CTL2		~(BIT(19))
#define WIFI_DL_SRC_BLK_MAP_CTL3		~(BIT(20))
#define WIFI_DL_SRC_BLK_CP_DL			~(BIT(21))
#define WIFI_DL_SRC_BLK_PCIE_UL			~(BIT(22))
#define WIFI_DL_SRC_BLK_SDIO_UL			~(BIT(23))
#define WIFI_DL_SRC_BLK_MAP_UL			~(BIT(24))
#define WIFI_DL_SRC_BLK_PCIE_CTL0_UL		~(BIT(25))
#define WIFI_DL_SRC_BLK_PCIE_CTL1_UL		~(BIT(26))
#define WIFI_DL_SRC_BLK_PCIE_CTL2_UL		~(BIT(27))
#define WIFI_DL_SRC_BLK_PCIE_CTL3_UL		~(BIT(28))
#define WIFI_DL_SRC_BLK_USB_DL			~(BIT(29))
#define WIFI_DL_SRC_BLK_WIAP_DL			~(BIT(30))

#define WIAP_DL_SRC_BLK_MAP_DL			~(BIT(0))
#define WIAP_DL_SRC_BLK_MAP_CTL0		~(BIT(1))
#define WIAP_DL_SRC_BLK_MAP_CTL1		~(BIT(2))
#define WIAP_DL_SRC_BLK_MAP_CTL2		~(BIT(3))
#define WIAP_DL_SRC_BLK_MAP_CTL3		~(BIT(4))
#define WIAP_DL_SRC_BLK_CP_DL			~(BIT(5))
#define WIAP_DL_SRC_BLK_PCIE_UL			~(BIT(6))
#define WIAP_DL_SRC_BLK_USB_UL			~(BIT(7))
#define WIAP_DL_SRC_BLK_MAP_UL			~(BIT(8))
#define WIAP_DL_SRC_BLK_PCIE_CTL0_UL		~(BIT(9))
#define WIAP_DL_SRC_BLK_PCIE_CTL1_UL		~(BIT(10))
#define WIAP_DL_SRC_BLK_PCIE_CTL2_UL		~(BIT(11))
#define WIAP_DL_SRC_BLK_PCIE_CTL3_UL		~(BIT(12))
#define WIAP_DL_SRC_BLK_WIFI_DL			~(BIT(13))
#define WIAP_DL_SRC_BLK_USB_DL			~(BIT(14))
#define IPA_WIFI_WIAP_DL_FLOWCTL_SRC		(0x20l)

/**
 * AXI_Outstanding_n_cg:
 *   [31:15]: Reserved
 *   [14]: rf_axi_mst_cg_en
 *   [13:8]: rf_axi_os_wr
 *   [7:6]: Reserved
 *   [5:0]: rt_axi_os_rd
 */
#define IPA_RF_AXI_MST_CG_EN_MASK		(BIT(14))
#define IPA_RF_AXI_OS_WR_MASK			(0x00003F00l)
#define IPA_RF_AXI_OS_RD_MASK			(0x0000003Fl)
#define IPA_AXI_OUTSTANDING_N_CG		(0x24l)

/**
 * cache_qos_prot_cfg:
 *   [31:28]: rf_cache_wr
 *   [27:24]: rf_cache_rd
 *   [23:20]: rf_qos_wr
 *   [19:16]: rf_qos_rd
 *   [15:6]: Reserved
 *   [5:3]: rf_prot_wr
 *   [2:0]: rf_prot_rd
 */
#define IPA_RF_CACHE_WR				(0xF0000000l)
#define IPA_RF_CACHE_RD				(0x0F000000l)
#define IPA_RF_QOS_WR				(0x00F00000l)
#define IPA_RF_QOS_RD				(0x000F0000l)
#define IPA_RF_PROT_WR				(0x00000038l)
#define IPA_RF_PROT_RD				(0x00000007l)
#define IPA_CACHE_QOS_PROT_CFG			(0x28l)

/**
 * Hash_TimeStep:
 *   Hash_Reg_Update is set by SW. the counter will be cleared. HAsh_Reg_Donw is
 *   set by HW. the counter will start counting. Timer counts 1ms for one step.
 */
#define IPA_HASH_TIMESTEP			(0x2Cl)

/**
 * SW_Debug_Mem_AddrL:
 *   LSB 32 bits of SW debug memory, used with SW_pause
 */
#define IPA_SW_DEBUG_MEM_ADDRL			(0x30l)

/**
 * internal_fifo_clr:
 *   [31]: TCP_fin_leave_2_ap
 *   [30]: TCP_syn_leave_2_ap
 *   [29]: TCP_rst_leave_2_ap
 *   [28]: TCP_psh_leave_2_ap
 *   [27]: TCP_ack_leave_2_ap
 *   [26]: TCP_urg_leave_2_ap
 *       1: when fin bit set in TCP, leave to AP process
 *       0: processed by route table
 *   [25]: CP_DL_flow_ctrl_recover
 *       write 1 then 0, CP DL exit from SW initial state
 *   [24:18]: Reserved
 *   [17]: N_Pkt_Rt_info_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [16]: Cline_BUF_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [15]: TR_BUF_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [14]: Rtn_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [13]: Out_Rx_FIFO_No_Chn_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [12]: ln_Rx_Prefetch_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [11]: Rx_FIFO_No_Chn_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [10]: Out_Rx_Prefetch_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [9]: Node_Info_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [8]: L2_L3_L4_HD_BUF_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [7]: Read_Write_Route_Node_fifo_re_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [6]: Read_Write_Base_Node_fifo_re_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [5]: Read_Write_Byps_Direct_Node_fifo_req_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [4]: Byte_Cnt_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [3]: Byte_Cnt_flg_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [2]: Data_Cpy_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [1]: Data_Cpy_Node_Store_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 *   [0]: set_buf_FIFO_clr
 *     1: Clear FIFO. need to write 1, then write 0
 */
#define IPA_TCP_FIN_LEAVE_2_AP_MASK		(BIT(31))
#define IPA_TCP_SYN_LEAVE_2_AP_MASK		(BIT(30))
#define IPA_TCP_RST_LEAVE_2_AP_MASK		(BIT(29))
#define IPA_TCP_PSH_LEAVE_2_AP_MASK		(BIT(28))
#define IPA_TCP_ACK_LEAVE_2_AP_MASK		(BIT(27))
#define IPA_TCP_URG_LEAVE_2_AP_MASK		(BIT(26))
#define IPA_CP_DL_FLOW_CTRL_RECOVER_MASK	(BIT(25))
#define IPA_N_PKT_RT_INFO_FIFO_CLR_MASK		(BIT(17))
#define IPA_CLINE_BUF_CLR_MASK			(BIT(16))
#define IPA_TR_BUF_CLR_MASK			(BIT(15))
#define IPA_RTN_FIFO_CLR_MASK			(BIT(14))
#define IPA_OUT_RX_FIFO_NO_CHN_FIFO_CLR_MASK \
			(BIT(13))

#define IPA_LN_RX_PREFETCH_FIFO_CLR_MASK	(BIT(12))
#define IPA_RX_FIFO_NO_CHN_FIFO_CLR_MASK	(BIT(11))
#define IPA_OUT_RX_PREFETCH_FIFO_CLR_MASK	(BIT(10))
#define IPA_NODE_INFO_FIFO_CLR_MASK		(BIT(9))
#define IPA_L2_L3_L4_HD_BUF_CLR_MASK		(BIT(8))
#define IPA_READ_WRITE_ROUTE_NODE_FIFO_RE_CLR_MASK \
			(BIT(7))

#define IPA_READ_WRITE_BASE_NODE_FIFO_RE_CLE_MASK \
			(BIT(6))

#define IPA_READ_WRITE_BYPS_DIRECT_NODE_FIFO_REQ_CLR_MASK \
			(BIT(5))

#define IPA_BYTE_CNT_FIFO_CLR_MASK		(BIT(4))
#define IPA_BYTE_CNT_FLG_FIFO_CLR_MASK		(BIT(3))
#define IPA_DATA_CPY_FIFO_CLR_MASK		(BIT(2))
#define IPA_DATA_CPY_NODE_STORE_FIFO_CLR_MASK \
			(BIT(1))

#define IPA_SET_BUF_FIFO_CLR_MASK		(BIT(0))
#define IPA_INTERNAL_FIFO_CLR_BIT_GROUP_MASK \
			(0x0003FFFFl)

#define IPA_INTERNAL_FIFO_CLR			(0x38l)

/**
 * axi_mst_chn_priority
 *   2'b00: channel priority is 0
 *   2'b01: channel priority is 1
 *   2'b10: channel priority is 2
 *   2'b11: channel priority is 3
 *   3 is the highest priority. 0 is the lowest priority
 */
#define IPA_AXI_WR_CHN5_PRIORITY_MASK		(0x03000000l)
#define IPA_AXI_WR_CHN4_PRIORITY_MASK		(0x00C00000l)
#define IPA_AXI_WR_CHN3_PRIORITY_MASK		(0x00300000l)
#define IPA_AXI_WR_CHN2_PRIORITY_MASK		(0x000C0000l)
#define IPA_AXI_WR_CHN1_PRIORITY_MASK		(0x00030000l)
#define IPA_AXI_RD_CHN5_PRIORITY_MASK		(0x00000300l)
#define IPA_AXI_RD_CHN4_PRIORITY_MASK		(0x000000C0l)
#define IPA_AXI_RD_CHN3_PRIORITY_MASK		(0x00000030l)
#define IPA_AXI_RD_CHN2_PRIORITY_MASK		(0x0000000Cl)
#define IPA_AXI_RD_CHN1_PRIORITY_MASK		(0x00000003l)

#define IPA_AXI_MST_CHN_PRIORITY		(0x3Cl)

/**
 * mAP_Interrupt_Src_En
 *   0: Interrupt source not selected for mAP Interrupt.
 *   1: Interrupt source selected for mAP Interrupt.
 */
#define IPA_CP_DL_DROP_CNT_CLR_MASK		(BIT(31))
#define IPA_CP_UL_DROP_CNT_CLR_MASK		(BIT(30))
#define IPA_USB_DL_INTERRUPT_SELECTED_MASK	(BIT(17))
#define IPA_USB_UL_INTERRUPT_SELECTED_MASK	(BIT(16))
#define IPA_SDIO_DL_INTERRUPT_SELECTED_MASK	(BIT(15))
#define IPA_SDIO_UL_INTERRUPT_SELECTED_MASK	(BIT(14))
#define IPA_PCIE_DL_INTERRUPT_SELECTED_MASK	(BIT(13))
#define IPA_PCIE_UL_INTERRUPT_SELECTED_MASK	(BIT(12))
#define IPA_PCIE_DL_CH0_INTERRUPT_SELECTED_MASK	(BIT(11))
#define IPA_PCIE_UL_CH0_INTERRUPT_SELECTED_MASK	(BIT(10))
#define IPA_PCIE_DL_CH1_INTERRUPT_SELECTED_MASK	(BIT(9))
#define IPA_PCIE_UL_CH1_INTERRUPT_SELECTED_MASK	(BIT(8))
#define IPA_PCIE_DL_CH2_INTERRUPT_SELECTED_MASK	(BIT(7))
#define IPA_PCIE_UL_CH2_INTERRUPT_SELECTED_MASK	(BIT(6))
#define IPA_PCIE_DL_CH3_INTERRUPT_SELECTED_MASK	(BIT(5))
#define IPA_PCIE_UL_CH3_INTERRUPT_SELECTED_MASK	(BIT(4))
#define IPA_WIFI_DL_INTERRUPT_SELECTED_MASK	(BIT(3))
#define IPA_WIFI_UL_INTERRUPT_SELECTED_MASK	(BIT(2))
#define IPA_WIAP_UL_INTERRUPT_SELECTED_MASK	(BIT(1))
#define IPA_WIAP_DL_INTERRUPT_SELECTED_MASK	(BIT(0))
#define IPA_INTERRUPT_SELECTED_GROUP		(0x3FFFFl)

#define IPA_MAP_INTERRUPT_SRC_EN		(0x40l)

/**
 * mAP_Interrupt_Status
 *   0: Do not have Interrupt.
 *   1: have Interrupt.
 */
#define IPA_MAP_DL_RX_INTERRUPT_MASK		(BIT(29))
#define IPA_MAP_DL_TX_INTERRUPT_MASK		(BIT(28))
#define IPA_MAP_UL_RX_INTERRUPT_MASK		(BIT(27))
#define IPA_MAP_UL_TX_INTERRUPT_MASK		(BIT(26))
#define IPA_MAP_PCIE_DL_CH0_INTERRUPT_MASK	(BIT(25))
#define IPA_MAP_PCIE_DL_CH1_INTERRUPT_MASK	(BIT(24))
#define IPA_MAP_PCIE_DL_CH2_INTERRUPT_MASK	(BIT(23))
#define IPA_MAP_PCIE_DL_CH3_INTERRUPT_MASK	(BIT(22))
#define IPA_MAP_PCIE_UL_CH0_INTERRUPT_MASK	(BIT(21))
#define IPA_MAP_PCIE_UL_CH1_INTERRUPT_MASK	(BIT(20))
#define IPA_MAP_PCIE_UL_CH2_INTERRUPT_MASK	(BIT(19))
#define IPA_MAP_PCIE_UL_CH3_INTERRUPT_MASK	(BIT(18))
#define IPA_USB_DL_INTERRUPT_MASK		(BIT(17))
#define IPA_USB_UL_INTERRUPT_MASK		(BIT(16))
#define IPA_SDIO_DL_INTERRUPT_MASK		(BIT(15))
#define IPA_SDIO_UL_INTERRUPT_MASK		(BIT(14))
#define IPA_PCIE_DL_INTERRUPT_MASK		(BIT(13))
#define IPA_PCIE_UL_INTERRUPT_MASK		(BIT(12))
#define IPA_PCIE_DL_CH0_INTERRUPT_MASK		(BIT(11))
#define IPA_PCIE_UL_CH0_INTERRUPT_MASK		(BIT(10))
#define IPA_PCIE_DL_CH1_INTERRUPT_MASK		(BIT(9))
#define IPA_PCIE_UL_CH1_INTERRUPT_MASK		(BIT(8))
#define IPA_PCIE_DL_CH2_INTERRUPT_MASK		(BIT(7))
#define IPA_PCIE_UL_CH2_INTERRUPT_MASK		(BIT(6))
#define IPA_PCIE_DL_CH3_INTERRUPT_MASK		(BIT(5))
#define IPA_PCIE_UL_CH3_INTERRUPT_MASK		(BIT(4))
#define IPA_WIFI_DL_INTERRUPT_MASK		(BIT(3))
#define IPA_WIFI_UL_INTERRUPT_MASK		(BIT(2))
#define IPA_WIAP_UL_INTERRUPT_MASK		(BIT(1))
#define IPA_WIAP_DL_INTERRUPT_MASK		(BIT(0))

#define IPA_MAP_INTERRUPT_STATUS		(0x44l)

/**
 * cp_configuration:
 *   2'b00: rx fifo meet flow control condition occur
 *   2'b01: tx fifo is full
 *   2'b10: rx fifo meet flow control condition occur, or tx fifo is full
 *   2'b11: reserved
 */
#define IPA_CP_DL_FLOW_CTRL_SET			(0xC0000000l)

/**
 * can be set to 0x4 ~ 0x7, the default value is 0x4
 */
#define IPA_CP_DL_CUR_TERM_NUM			(0x3E000000l)

/**
 * used for Bypass mode, details in Src/Dst define table in IPA Design Spec.
 */
#define IPA_CP_DL_DST_TERM_NUM			(0x01F00000l)

/**
 * priority can be 0x0 ~ 0xF
 * 15 is the highest priority, 1 is the lowest priority
 */
#define IPA_CP_DL_PRIORITY			(0x000F0000l)

/**
 * 2'b00: rx fifo meet flow control condition occur
 * 2'b01: tx fifo is full
 * 2'b10: rx fifo meet flow control condition occur, or tx fifo is full
 * 2'b11: reserved
 */
#define IPA_CP_UL_FLOW_CTRL_SEL			(0x0000C000l)

/**
 * can be set to 0x4 ~ 0x7
 */
#define IPA_CP_UL_CUR_TERM_NUM			(0x00003E00l)

/**
 * do not need set
 */
#define IPA_CP_UL_DST_TERM_NUM			(0x000001F0l)

/**
 * priority can be 0x0 ~ 0xF
 * 15 is the highest priority, 0 is the lowest priority.
 */
#define IPA_CP_DL_FLOW_CTRL_SEL_MASK		0xc0000000l
#define IPA_CP_DL_FLOW_CTRL_SEL_OFFSET		30
#define IPA_CP_DL_CUR_TERM_NUM_MASK		0x3e000000l
#define IPA_CP_DL_CUR_TERM_NUM_OFFSET		25
#define IPA_CP_DL_DST_TERM_NUM_MASK		0x01f00000l
#define IPA_CP_DL_DST_TERM_NUM_OFFSET		20
#define IPA_CP_DL_PRIORITY_MASK			0x000f0000l
#define IPA_CP_DL_PRIORITY_OFFSET		16

#define IPA_CP_UL_FLOW_CTRL_SEL_MASK		0x0000c000l
#define IPA_CP_UL_FLOW_CTRL_SEL_OFFSET		14
#define IPA_CP_UL_CUR_TERM_NUM_MASK		0x00003e00l
#define IPA_CP_UL_CUR_TERM_NUM_OFFSET		9
#define IPA_CP_UL_DST_TERM_NUM_MASK		0x000001f0l
#define IPA_CP_UL_DST_TERM_NUM_OFFSET		4
#define IPA_CP_UL_PRIORITY_MASK			0x0000000fl
#define IPA_CP_UL_PRIORITY_OFFSET		0
#define IPA_CP_CONFIGURATION			0x48l

/**
 * when drop in cp DL channel, counter plus 1.
 */
#define IPA_CP_DL_DROP_CNT			(0x4Cl)

/**
 * when drop in cp UL channel, counter plus 1.
 */
#define IPA_CP_UL_DROP_CNT			(0x50l)

/**
 * cp_dl_ul_flow_ctl_watermark
 * [31:16]: watermark for exit flow control
 * [15:0]: watermark for exit flow control
 */
#define IPA_CP_DL_RX_FIFO_EXIT_FLOW_CTRL_WATERMARK_MASK	(0xFFFF0000l)
#define IPA_CP_UL_RX_FIFO_EXIT_FLOW_CTRL_WATERMARK_MASK	(0x0000FFFFl)
#define IPA_CP_DL_UL_FLOW_CTL_WATERMARK			(0x54l)

/**
 * USB_SDIO_flowctl_Src_sts
 * [30:16]: USB_DL_FLOW_CTL_to_Src_sts
 * [14:0]: SDIO_DL_FLOW_CTL_to_Src_sts
 */
#define USB_DL_SRC_STS_MAP_DL			(BIT(16))
#define USB_DL_SRC_STS_MAP_CTL0			(BIT(17))
#define USB_DL_SRC_STS_MAP_CTL1			(BIT(18))
#define USB_DL_SRC_STS_MAP_CTL2			(BIT(19))
#define USB_DL_SRC_STS_MAP_CTL3			(BIT(20))
#define USB_DL_SRC_STS_CP_DL			(BIT(21))
#define USB_DL_SRC_STS_PCIE_UL			(BIT(22))
#define USB_DL_SRC_STS_SDIO_UL			(BIT(23))
#define USB_DL_SRC_STS_MAP_UL			(BIT(24))
#define USB_DL_SRC_STS_PCIE_CTL0_UL		(BIT(25))
#define USB_DL_SRC_STS_PCIE_CTL1_UL		(BIT(26))
#define USB_DL_SRC_STS_PCIE_CTL2_UL		(BIT(27))
#define USB_DL_SRC_STS_PCIE_CTL3_UL		(BIT(28))
#define USB_DL_SRC_STS_WIFI_DL			(BIT(29))
#define USB_DL_SRC_STS_WIAP_DL			(BIT(30))

#define SDIO_DL_SRC_STS_MAP_DL			(BIT(0))
#define SDIO_DL_SRC_STS_MAP_CTL0		(BIT(1))
#define SDIO_DL_SRC_STS_MAP_CTL1		(BIT(2))
#define SDIO_DL_SRC_STS_MAP_CTL2		(BIT(3))
#define SDIO_DL_SRC_STS_MAP_CTL3		(BIT(4))
#define SDIO_DL_SRC_STS_CP_DL			(BIT(5))
#define SDIO_DL_SRC_STS_PCIE_UL			(BIT(6))
#define SDIO_DL_SRC_STS_USB_UL			(BIT(7))
#define SDIO_DL_SRC_STS_MAP_UL			(BIT(8))
#define SDIO_DL_SRC_STS_PCIE_CTL0_UL		(BIT(9))
#define SDIO_DL_SRC_STS_PCIE_CTL1_UL		(BIT(10))
#define SDIO_DL_SRC_STS_PCIE_CTL2_UL		(BIT(11))
#define SDIO_DL_SRC_STS_PCIE_CTL3_UL		(BIT(12))
#define SDIO_DL_SRC_STS_WIFI_DL			(BIT(13))
#define SDIO_DL_SRC_STS_WIAP_DL			(BIT(14))

#define IPA_USB_SDIO_FLOWCTL_SRC_STS		(0x58l)

/**
 * mAP_RX_DL_PCIE_UL_flowctl_Src_sts
 * [30:16]: mAP_RX_DL_FLOW_CTL_to_Src_sts
 * [14:0]: PCIE_DL_FLOW_CTL_to_Src_sts
 */
#define MAP_RX_DL_SRC_STS_USB_UL		(BIT(16))
#define MAP_RX_DL_SRC_STS_MAP_CTL0		(BIT(17))
#define MAP_RX_DL_SRC_STS_MAP_CTL1		(BIT(18))
#define MAP_RX_DL_SRC_STS_MAP_CTL2		(BIT(19))
#define MAP_RX_DL_SRC_STS_MAP_CTL3		(BIT(20))
#define MAP_RX_DL_SRC_STS_CP_DL			(BIT(21))
#define MAP_RX_DL_SRC_STS_PCIE_UL		(BIT(22))
#define MAP_RX_DL_SRC_STS_SDIO_UL		(BIT(23))
#define MAP_RX_DL_SRC_STS_MAP_UL		(BIT(24))
#define MAP_RX_DL_SRC_STS_PCIE_CTL0_UL		(BIT(25))
#define MAP_RX_DL_SRC_STS_PCIE_CTL1_UL		(BIT(26))
#define MAP_RX_DL_SRC_STS_PCIE_CTL2_UL		(BIT(27))
#define MAP_RX_DL_SRC_STS_PCIE_CTL3_UL		(BIT(28))
#define MAP_RX_DL_SRC_STS_WIFI_DL		(BIT(29))
#define MAP_RX_DL_SRC_STS_WIAP_DL		(BIT(30))

#define PCIE_DL_SRC_STS_MAP_DL			(BIT(0))
#define PCIE_DL_SRC_STS_MAP_CTL0		(BIT(1))
#define PCIE_DL_SRC_STS_MAP_CTL1		(BIT(2))
#define PCIE_DL_SRC_STS_MAP_CTL2		(BIT(3))
#define PCIE_DL_SRC_STS_MAP_CTL3		(BIT(4))
#define PCIE_DL_SRC_STS_CP_DL			(BIT(5))
#define PCIE_DL_SRC_STS_USB_UL			(BIT(6))
#define PCIE_DL_SRC_STS_SDIO_UL			(BIT(7))
#define PCIE_DL_SRC_STS_MAP_UL			(BIT(8))
#define PCIE_DL_SRC_STS_PCIE_CTL0_UL		(BIT(9))
#define PCIE_DL_SRC_STS_PCIE_CTL1_UL		(BIT(10))
#define PCIE_DL_SRC_STS_PCIE_CTL2_UL		(BIT(11))
#define PCIE_DL_SRC_STS_PCIE_CTL3_UL		(BIT(12))
#define PCIE_DL_SRC_STS_WIFI_DL			(BIT(13))
#define PCIE_DL_SRC_STS_WIAP_DL			(BIT(14))

#define IPA_MAP_RX_DL_PCIE_DL_FLOWCTL_SRC_STS	(0x5Cl)

/**
 * PCIE_Ch0_1_DL_flowctl_Src_sts
 * [30:16]: PCIE_Ch0_DL_FLOW_CTL_to_Src_sts
 * [14:0]: PCIE_Ch1_DL_FLOW_CTL_to_Src_sts
 */
#define PCIE_CH0_DL_SRC_STS_MAP_DL		(BIT(16))
#define PCIE_CH0_DL_SRC_STS_MAP_CTL0		(BIT(17))
#define PCIE_CH0_DL_SRC_STS_MAP_CTL1		(BIT(18))
#define PCIE_CH0_DL_SRC_STS_MAP_CTL2		(BIT(19))
#define PCIE_CH0_DL_SRC_STS_MAP_CTL3		(BIT(20))
#define PCIE_CH0_DL_SRC_STS_CP_DL		(BIT(21))
#define PCIE_CH0_DL_SRC_STS_PCIE_UL		(BIT(22))
#define PCIE_CH0_DL_SRC_STS_SDIO_UL		(BIT(23))
#define PCIE_CH0_DL_SRC_STS_MAP_UL		(BIT(24))
#define PCIE_CH0_DL_SRC_STS_USB_UL		(BIT(25))
#define PCIE_CH0_DL_SRC_STS_PCIE_CTL1_UL	(BIT(26))
#define PCIE_CH0_DL_SRC_STS_PCIE_CTL2_UL	(BIT(27))
#define PCIE_CH0_DL_SRC_STS_PCIE_CTL3_UL	(BIT(28))
#define PCIE_CH0_DL_SRC_STS_WIFI_DL		(BIT(29))
#define PCIE_CH0_DL_SRC_STS_WIAP_DL		(BIT(30))

#define PCIE_CH1_DL_SRC_STS_MAP_DL		(BIT(0))
#define PCIE_CH1_DL_SRC_STS_MAP_CTL0		(BIT(1))
#define PCIE_CH1_DL_SRC_STS_MAP_CTL1		(BIT(2))
#define PCIE_CH1_DL_SRC_STS_MAP_CTL2		(BIT(3))
#define PCIE_CH1_DL_SRC_STS_MAP_CTL3		(BIT(4))
#define PCIE_CH1_DL_SRC_STS_CP_DL		(BIT(5))
#define PCIE_CH1_DL_SRC_STS_PCIE_UL		(BIT(6))
#define PCIE_CH1_DL_SRC_STS_SDIO_UL		(BIT(7))
#define PCIE_CH1_DL_SRC_STS_MAP_UL		(BIT(8))
#define PCIE_CH1_DL_SRC_STS_PCIE_CTL0_UL	(BIT(9))
#define PCIE_CH1_DL_SRC_STS_USB_UL		(BIT(10))
#define PCIE_CH1_DL_SRC_STS_PCIE_CTL2_UL	(BIT(11))
#define PCIE_CH1_DL_SRC_STS_PCIE_CTL3_UL	(BIT(12))
#define PCIE_CH1_DL_SRC_STS_WIFI_DL		(BIT(13))
#define PCIE_CH1_DL_SRC_STS_WIAP_DL		(BIT(14))

#define IPA_PCIE_CH0_1_DL_FLOWCTL_SRC_STS	(0x60l)

/**
 * PCIE_Ch2_3_DL_flowctl_Src_sts
 * [30:16]: PCIE_Ch2_DL_FLOW_CTL_to_Src_sts
 * [14:0]: PCIE_Ch3_DL_FLOW_CTL_to_Src_sts
 */
#define PCIE_CH2_DL_SRC_STS_MAP_DL		(BIT(16))
#define PCIE_CH2_DL_SRC_STS_MAP_CTL0		(BIT(17))
#define PCIE_CH2_DL_SRC_STS_MAP_CTL1		(BIT(18))
#define PCIE_CH2_DL_SRC_STS_MAP_CTL2		(BIT(19))
#define PCIE_CH2_DL_SRC_STS_MAP_CTL3		(BIT(20))
#define PCIE_CH2_DL_SRC_STS_CP_DL		(BIT(21))
#define PCIE_CH2_DL_SRC_STS_PCIE_UL		(BIT(22))
#define PCIE_CH2_DL_SRC_STS_SDIO_UL		(BIT(23))
#define PCIE_CH2_DL_SRC_STS_MAP_UL		(BIT(24))
#define PCIE_CH2_DL_SRC_STS_PCIE_CTL0_UL	(BIT(25))
#define PCIE_CH2_DL_SRC_STS_PCIE_CTL1_UL	(BIT(26))
#define PCIE_CH2_DL_SRC_STS_USB_DL		(BIT(27))
#define PCIE_CH2_DL_SRC_STS_PCIE_CTL3_UL	(BIT(28))
#define PCIE_CH2_DL_SRC_STS_WIFI_DL		(BIT(29))
#define PCIE_CH2_DL_SRC_STS_WIAP_DL		(BIT(30))

#define PCIE_CH3_DL_SRC_STS_MAP_DL		(BIT(0))
#define PCIE_CH3_DL_SRC_STS_MAP_CTL0		(BIT(1))
#define PCIE_CH3_DL_SRC_STS_MAP_CTL1		(BIT(2))
#define PCIE_CH3_DL_SRC_STS_MAP_CTL2		(BIT(3))
#define PCIE_CH3_DL_SRC_STS_MAP_CTL3		(BIT(4))
#define PCIE_CH3_DL_SRC_STS_CP_DL		(BIT(5))
#define PCIE_CH3_DL_SRC_STS_PCIE_UL		(BIT(6))
#define PCIE_CH3_DL_SRC_STS_SDIO_UL		(BIT(7))
#define PCIE_CH3_DL_SRC_STS_MAP_UL		(BIT(8))
#define PCIE_CH3_DL_SRC_STS_PCIE_CTL0_UL	(BIT(9))
#define PCIE_CH3_DL_SRC_STS_PCIE_CTL1_UL	(BIT(10))
#define PCIE_CH3_DL_SRC_STS_PCIE_CTL2_UL	(BIT(11))
#define PCIE_CH3_DL_SRC_STS_USB_UL		(BIT(12))
#define PCIE_CH3_DL_SRC_STS_WIFI_DL		(BIT(13))
#define PCIE_CH3_DL_SRC_STS_WIAP_DL		(BIT(14))

#define IPA_PCIE_CH2_3_DL_FLOWCTL_SRC_STS	(0x64l)

/**
 * CP_mAP_TX_UL_flowctl_Src_sts
 * [30:16]: CP_UL_FLOW_CTL_to_Src_sts
 * [14:0]: mAP_RX_UL_FLOW_CTL_to_Src_sts
 */
#define CP_UL_SRC_STS_MAP_DL			(BIT(16))
#define CP_UL_SRC_STS_MAP_CTL0			(BIT(17))
#define CP_UL_SRC_STS_MAP_CTL1			(BIT(18))
#define CP_UL_SRC_STS_MAP_CTL2			(BIT(19))
#define CP_UL_SRC_STS_MAP_CTL3			(BIT(20))
#define CP_UL_SRC_STS_USB_UL			(BIT(21))
#define CP_UL_SRC_STS_PCIE_UL			(BIT(22))
#define CP_UL_SRC_STS_SDIO_UL			(BIT(23))
#define CP_UL_SRC_STS_MAP_UL			(BIT(24))
#define CP_UL_SRC_STS_PCIE_CTL0_UL		(BIT(25))
#define CP_UL_SRC_STS_PCIE_CTL1_UL		(BIT(26))
#define CP_UL_SRC_STS_PCIE_CTL2_UL		(BIT(27))
#define CP_UL_SRC_STS_PCIE_CTL3_UL		(BIT(28))
#define CP_UL_SRC_STS_WIFI_DL			(BIT(29))
#define CP_UL_SRC_STS_WIAP_DL			(BIT(30))

#define MAP_RX_UL_SRC_STS_MAP_DL		(BIT(0))
#define MAP_RX_UL_SRC_STS_MAP_CTL0		(BIT(1))
#define MAP_RX_UL_SRC_STS_MAP_CTL1		(BIT(2))
#define MAP_RX_UL_SRC_STS_MAP_CTL2		(BIT(3))
#define MAP_RX_UL_SRC_STS_MAP_CTL3		(BIT(4))
#define MAP_RX_UL_SRC_STS_CP_DL			(BIT(5))
#define MAP_RX_UL_SRC_STS_PCIE_UL		(BIT(6))
#define MAP_RX_UL_SRC_STS_SDIO_UL		(BIT(7))
#define MAP_RX_UL_SRC_STS_USB_UL		(BIT(8))
#define MAP_RX_UL_SRC_STS_PCIE_CTL0_UL		(BIT(9))
#define MAP_RX_UL_SRC_STS_PCIE_CTL1_UL		(BIT(10))
#define MAP_RX_UL_SRC_STS_PCIE_CTL2_UL		(BIT(11))
#define MAP_RX_UL_SRC_STS_PCIE_CTL3_UL		(BIT(12))
#define MAP_RX_UL_SRC_STS_WIFI_DL		(BIT(13))
#define MAP_RX_UL_SRC_STS_WIAP_DL		(BIT(14))

#define IPA_CP_MAP_TX_UL_FLOWCTL_SRC_STS	(0x68l)

/**
 * mAP_PCIE_Ch0_1_UL_flowctl_Src_sts
 * [30:16]: mAP_PCIE_Ch0_UL_FLOW_CTL_to_Src_sts
 * [14:0]: mAP_PCIE_Ch1_UL_FLOW_CTL_to_Src_sts
 */
#define MAP_PCIE_CH0_UL_SRC_STS_MAP_DL		(BIT(16))
#define MAP_PCIE_CH0_UL_SRC_STS_USB_UL		(BIT(17))
#define MAP_PCIE_CH0_UL_SRC_STS_MAP_CTL1	(BIT(18))
#define MAP_PCIE_CH0_UL_SRC_STS_MAP_CTL2	(BIT(19))
#define MAP_PCIE_CH0_UL_SRC_STS_MAP_CTL3	(BIT(20))
#define MAP_PCIE_CH0_UL_SRC_STS_CP_DL		(BIT(21))
#define MAP_PCIE_CH0_UL_SRC_STS_PCIE_UL		(BIT(22))
#define MAP_PCIE_CH0_UL_SRC_STS_SDIO_UL		(BIT(23))
#define MAP_PCIE_CH0_UL_SRC_STS_MAP_UL		(BIT(24))
#define MAP_PCIE_CH0_UL_SRC_STS_PCIE_CTL0_UL	(BIT(25))
#define MAP_PCIE_CH0_UL_SRC_STS_PCIE_CTL1_UL	(BIT(26))
#define MAP_PCIE_CH0_UL_SRC_STS_PCIE_CTL2_UL	(BIT(27))
#define MAP_PCIE_CH0_UL_SRC_STS_PCIE_CTL3_UL	(BIT(28))
#define MAP_PCIE_CH0_UL_SRC_STS_WIFI_DL		(BIT(29))
#define MAP_PCIE_CH0_UL_SRC_STS_WIAP_DL		(BIT(30))

#define MAP_PCIE_CH1_UL_SRC_STS_MAP_DL		(BIT(0))
#define MAP_PCIE_CH1_UL_SRC_STS_MAP_CTL0	(BIT(1))
#define MAP_PCIE_CH1_UL_SRC_STS_USB_UL		(BIT(2))
#define MAP_PCIE_CH1_UL_SRC_STS_MAP_CTL2	(BIT(3))
#define MAP_PCIE_CH1_UL_SRC_STS_MAP_CTL3	(BIT(4))
#define MAP_PCIE_CH1_UL_SRC_STS_CP_DL		(BIT(5))
#define MAP_PCIE_CH1_UL_SRC_STS_PCIE_UL		(BIT(6))
#define MAP_PCIE_CH1_UL_SRC_STS_SDIO_UL		(BIT(7))
#define MAP_PCIE_CH1_UL_SRC_STS_MAP_UL		(BIT(8))
#define MAP_PCIE_CH1_UL_SRC_STS_PCIE_CTL0_UL	(BIT(9))
#define MAP_PCIE_CH1_UL_SRC_STS_PCIE_CTL1_UL	(BIT(10))
#define MAP_PCIE_CH1_UL_SRC_STS_PCIE_CTL2_UL	(BIT(11))
#define MAP_PCIE_CH1_UL_SRC_STS_PCIE_CTL3_UL	(BIT(12))
#define MAP_PCIE_CH1_UL_SRC_STS_WIFI_DL		(BIT(13))
#define MAP_PCIE_CH1_UL_SRC_STS_WIAP_DL		(BIT(14))

#define IPA_MAP_PCIE_CH0_1_UL_FLOWCTL_SRC_STS	(0x6Cl)

/**
 * mAP_PCIE_Ch2_3_UL_flowctl_Src_sts
 * [30:16]: mAP_PCIE_Ch2_UL_FLOW_CTL_to_Src_sts
 * [14:0]: mAP_PCIE_Ch3_UL_FLOW_CTL_to_Src_sts
 */
#define MAP_PCIE_CH2_UL_SRC_STS_MAP_DL		(BIT(16))
#define MAP_PCIE_CH2_UL_SRC_STS_MAP_CTL0	(BIT(17))
#define MAP_PCIE_CH2_UL_SRC_STS_MAP_CTL1	(BIT(18))
#define MAP_PCIE_CH2_UL_SRC_STS_USB_UL		(BIT(19))
#define MAP_PCIE_CH2_UL_SRC_STS_MAP_CTL3	(BIT(20))
#define MAP_PCIE_CH2_UL_SRC_STS_CP_DL		(BIT(21))
#define MAP_PCIE_CH2_UL_SRC_STS_PCIE_UL		(BIT(22))
#define MAP_PCIE_CH2_UL_SRC_STS_SDIO_UL		(BIT(23))
#define MAP_PCIE_CH2_UL_SRC_STS_MAP_UL		(BIT(24))
#define MAP_PCIE_CH2_UL_SRC_STS_PCIE_CTL0_UL	(BIT(25))
#define MAP_PCIE_CH2_UL_SRC_STS_PCIE_CTL1_UL	(BIT(26))
#define MAP_PCIE_CH2_UL_SRC_STS_PCIE_CTL2_UL	(BIT(27))
#define MAP_PCIE_CH2_UL_SRC_STS_PCIE_CTL3_UL	(BIT(28))
#define MAP_PCIE_CH2_UL_SRC_STS_WIFI_DL		(BIT(29))
#define MAP_PCIE_CH2_UL_SRC_STS_WIAP_DL		(BIT(30))

#define MAP_PCIE_CH3_UL_SRC_STS_MAP_DL		(BIT(0))
#define MAP_PCIE_CH3_UL_SRC_STS_MAP_CTL0	(BIT(1))
#define MAP_PCIE_CH3_UL_SRC_STS_MAP_CTL1	(BIT(2))
#define MAP_PCIE_CH3_UL_SRC_STS_MAP_CTL2	(BIT(3))
#define MAP_PCIE_CH3_UL_SRC_STS_USB_UL		(BIT(4))
#define MAP_PCIE_CH3_UL_SRC_STS_CP_DL		(BIT(5))
#define MAP_PCIE_CH3_UL_SRC_STS_PCIE_UL		(BIT(6))
#define MAP_PCIE_CH3_UL_SRC_STS_SDIO_UL		(BIT(7))
#define MAP_PCIE_CH3_UL_SRC_STS_MAP_UL		(BIT(8))
#define MAP_PCIE_CH3_UL_SRC_STS_PCIE_CTL0_UL	(BIT(9))
#define MAP_PCIE_CH3_UL_SRC_STS_PCIE_CTL1_UL	(BIT(10))
#define MAP_PCIE_CH3_UL_SRC_STS_PCIE_CTL2_UL	(BIT(11))
#define MAP_PCIE_CH3_UL_SRC_STS_PCIE_CTL3_UL	(BIT(12))
#define MAP_PCIE_CH3_UL_SRC_STS_WIFI_DL		(BIT(13))
#define MAP_PCIE_CH3_UL_SRC_STS_WIAP_DL		(BIT(14))

#define IPA_MAP_PCIE_CH2_3_UL_FLOWCTL_SRC_STS	(0x70l)

/**
 * WIFI_WiAP_DL_flowctl_Src_sts
 * [30:16]: WIFI_DL_FLOW_CTL_to_Src_sts
 * [14:0]: WiAP_DL_FLOW_CTL_to_Src_sts
 */
#define WIFI_DL_SRC_STS_MAP_DL			~(BIT(16))
#define WIFI_DL_SRC_STS_MAP_CTL0		~(BIT(17))
#define WIFI_DL_SRC_STS_MAP_CTL1		~(BIT(18))
#define WIFI_DL_SRC_STS_MAP_CTL2		~(BIT(19))
#define WIFI_DL_SRC_STS_MAP_CTL3		~(BIT(20))
#define WIFI_DL_SRC_STS_CP_DL			~(BIT(21))
#define WIFI_DL_SRC_STS_PCIE_UL			~(BIT(22))
#define WIFI_DL_SRC_STS_SDIO_UL			~(BIT(23))
#define WIFI_DL_SRC_STS_MAP_UL			~(BIT(24))
#define WIFI_DL_SRC_STS_PCIE_CTL0_UL		~(BIT(25))
#define WIFI_DL_SRC_STS_PCIE_CTL1_UL		~(BIT(26))
#define WIFI_DL_SRC_STS_PCIE_CTL2_UL		~(BIT(27))
#define WIFI_DL_SRC_STS_PCIE_CTL3_UL		~(BIT(28))
#define WIFI_DL_SRC_STS_USB_DL			~(BIT(29))
#define WIFI_DL_SRC_STS_WIAP_DL			~(BIT(30))

#define WIAP_DL_SRC_STS_MAP_DL			~(BIT(0))
#define WIAP_DL_SRC_STS_MAP_CTL0		~(BIT(1))
#define WIAP_DL_SRC_STS_MAP_CTL1		~(BIT(2))
#define WIAP_DL_SRC_STS_MAP_CTL2		~(BIT(3))
#define WIAP_DL_SRC_STS_MAP_CTL3		~(BIT(4))
#define WIAP_DL_SRC_STS_CP_DL			~(BIT(5))
#define WIAP_DL_SRC_STS_PCIE_UL			~(BIT(6))
#define WIAP_DL_SRC_STS_USB_UL			~(BIT(7))
#define WIAP_DL_SRC_STS_MAP_UL			~(BIT(8))
#define WIAP_DL_SRC_STS_PCIE_CTL0_UL		~(BIT(9))
#define WIAP_DL_SRC_STS_PCIE_CTL1_UL		~(BIT(10))
#define WIAP_DL_SRC_STS_PCIE_CTL2_UL		~(BIT(11))
#define WIAP_DL_SRC_STS_PCIE_CTL3_UL		~(BIT(12))
#define WIAP_DL_SRC_STS_WIFI_DL			~(BIT(13))
#define WIAP_DL_SRC_STS_USB_DL			~(BIT(14))

#define IPA_WIFI_WIAP_DL_FLOWCTL_SRC_STS	(0x74l)

/**
 * Hash_Table_Base_Addr_Low
 */
#define IPA_HASH_TABLE_BASE_ADDR_LOW		(0x78l)

/**
 * Hash_Table_Switch_ctrl
 *   [25:18]: high 8bits Base Addr of Hash Table
 *   [17:2]: Hash_Reg_Len
 *   [1]: Hash_Reg_Update
 *   [0]: Hash_Reg_Done
 */
#define HASH_REG_BASE_HIGH_OFFSET		18l
#define HASH_REG_LEN_OFFSET			2l

#define IPA_HASH_REG_BASE_HIGH_MASK		(0x03FC0000l)
#define IPA_HASH_REG_LEN_MASK			(0x0003FFFCl)
#define IPA_HASH_REG_UPDATE_MASK		(0x00000002l)
#define IPA_HASH_REG_DONE_MASK			(0x00000001l)
#define IPA_HASH_TABLE_SWITCH_CTL		(0x7Cl)

/**
 * ori_id_invld_node_info_l
 *   [31:0]: id invalid node content, original
 */
#define IPA_ORI_ID_INVLD_NODE_INFO_L		(0x1000l)

/**
 * ori_id_invld_node_info_h
 *   [17:0]: id invalid node content, original
 */
#define IPA_ORI_ID_INVLD_NODE_INFO_H		(0x1004l)

/**
 * route_id_invld_node_info_l
 *   [31:0]: id invalid node content, after route
 */
#define IPA_ROUTE_ID_INVLD_NODE_INFO_L		(0x1008l)

/**
 * route_id_invld_node_info_h
 *   [17:0]: id invalid node content, after route
 */
#define IPA_ROUTE_ID_INVLD_NODE_INFO_H		(0x100Cl)

/**
 * ipa_free_fifo_fatal_int_sts
 *   [31:10]: Reserved
 *   [9]: pcie ul ch3 free fifo empty during transfer
 *   [8]: pcie ul ch2 free fifo empty during transfer
 *   [7]: pcie ul ch1 free fifo empty during transfer
 *   [6]: pcie ul ch0 free fifo empty during transfer
 *   [5]: device to ap free fifo empty during transfer
 *   [4]: pcie dl ch3 free fifo empty during transfer
 *   [3]: pcie dl ch2 free fifo empty during transfer
 *   [2]: pcie dl ch1 free fifo empty during transfer
 *   [1]: pcie dl ch0 free fifo empty during transfer
 *   [0]: CP to AP dl free fifo empty during transfer
 */
#define IPA_FREE_FIFO_FATAL_INT_STS		(0x1010l)

/**
 * ipa_free_fifo_fatal_int_clr
 *   [31:10]: Reserved
 *   [9:0]: ipa free fifo fatal int clear
 */
#define IPA_FREE_FIFO_FATAL_INT_CLR		(0x1014l)

/**
 * ipa_free_fifo_fatal_int_en
 *   [31:10]: Reserved
 *   [9:0]: ipa free fifo fatal int enable
 */
#define IPA_FREE_FIFO_FATAL_INT_EN		(0x1018l)

/**
 * ipa_ctrl
 *   [31:16]: Reserved
 *   [15] HW int enable
 *   [14:1]: cm4_int_mask
 *   [0]: ul wiap dma enable
 */
#define IPA_WIAP_UL_DMA_EN_MASK	(0x01l)
#define ENABLE_PCIE_MEM_INTR_MODE_MASK BIT(15)

#define MAP_UL_DEV_FIFO_FREE_FIFO_FATAL_INTERRUPT_FOR_CM4_MASK \
			BIT(14)

#define MAP_DL_CP_FIFO_FREE_FIFO_FATAL_INTERRUPT_FOR_CM4_MASK \
			BIT(13)

#define MAP_DL_CP_FIFO_INTERRUPT_FOR_CM4_MASK	BIT(12)
#define mAP_DL_DEV_FIFO_INTERRUPT_MASK_FOR_CM4	BIT(11)
#define mAP_UL_DEV_FIFO_INTERRUPT_MASK_FOR_CM4	BIT(10)
#define mAP_UL_CP_FIFO_INTERRUPT_MASK_FOR_CM4	BIT(9)
#define ROUTE_ID_INVALID_INTERRUPT_MASK_FOR_CM4	BIT(8)
#define ORI_ID_INVALID_INTERRUPT_MASK_FOR_CM4	BIT(7)
#define USB_DL_INTERRUPT_MASK_FOR_CM4		BIT(6)
#define USB_UL_INTERRUPT_MASK_FOR_CM4		BIT(5)
#define WIFI_DL_INTERRUPT_MASK_FOR_CM4		BIT(4)
#define WIFI_UL_INTERRUPT_MASK_FOR_CM4		BIT(3)
#define WIAP_UL_INTERRUPT_MASK_FOR_CM4		BIT(2)
#define WIAP_DL_INTERRUPT_MASK_FOR_CM4		BIT(1)

#define IPA_CTRL				(0x101Cl)

/**
 * ipa_dfs_ctrl0
 *   [31:11]: Reserved
 *   [10:1]: ipa dfs count period, step 1 ms
 *   [0]: ipa dfs enable
 */
#define IPA_DFS_PERIOD_MASK			(0x7FEl)
#define IPA_DFS_EN_MASK				(0x1l)
#define IPA_DFS_CTRL0				(0x1020l)

/**
 * ipa_dfs_ctrl1
 *   [31:24]: ipa_dfs_th3
 *   [23:16]: ipa_dfs_th2
 *   [15:8]: ipa_dfs_th1
 *   [7:0]: ipa_dfs_th0
 */

#define IPA_DFS_TH3_MASK			(0xFF000000l)
#define IPA_DFS_TH2_MASK			(0x00FF0000l)
#define IPA_DFS_TH1_MASK			(0x0000FF00l)
#define IPA_DFS_TH0_MASK			(0x000000FFl)
#define IPA_DFS_CTRL1				(0x1024l)

#define IPA_INPUT_CHN_WRAP_DBG0			(0x1028l)
#define IPA_INPUT_CHN_WRAP_DBG1			(0x102Cl)
#define IPA_INPUT_CHN_WRAP_DBG2			(0x1030l)

#define IPA_OUTPUT_CHN_WRAP_DBG0		(0x1034l)
#define IPA_OUTPUT_CHN_WRAP_DBG1		(0x1038l)
#define IPA_OUTPUT_CHN_WRAP_DBG2		(0x103Cl)
#define IPA_OUTPUT_CHN_WRAP_DBG3		(0x1040l)

#define IPA_DATA_CHN_PROC_DBG0			(0x1044l)
#define IPA_DATA_CHN_PROC_DBG1			(0x1048l)
#define IPA_DATA_CHN_PROC_DBG2			(0x104Cl)
#define IPA_DATA_CHN_PROC_DBG3			(0x1050l)
#define IPA_DATA_CHN_PROC_DBG4			(0x1054l)

#define IPA_ROUTE_REG_DBG0			(0x1058l)
#define IPA_ROUTE_REG_DBG1			(0x105Cl)

#define IPA_BUS_MONITOR_COND			(0x1060l)
#define IPA_BUS_MONITOR_MASK			(0x1064l)

#define IPA_BUS_MONITOR_CFG			(0x1068l)
#define IPA_BUS_MONITOR_SOFT_RDATA		(0x106Cl)

#define IPA_DUMMY_REG				(0x1070l)

#define IPA_PCIE_DL_TX_FIFO_INT_ADDR_LOW	0x1078
#define IPA_PCIE_DL_TX_FIFO_INT_ADDR_HIGH	0x107c
#define IPA_PCIE_DL_TX_FIFO_INT_ADDR_HIGH_MASK	GENMASK(7, 0)
#define IPA_PCIE_DL_TX_FIFO_INT_PATTERN		0x1080

#define IPA_PCIE_DL_RX_FIFO_INT_ADDR_LOW	0x1084
#define IPA_PCIE_DL_RX_FIFO_INT_ADDR_HIGH	0x1088
#define IPA_PCIE_DL_RX_FIFO_INT_ADDR_HIGH_MASK	GENMASK(7, 0)
#define IPA_PCIE_DL_RX_FIFO_INT_PATTERN		0x108c

#define IPA_PCIE_UL_RX_FIFO_INT_ADDR_LOW	0x1090
#define IPA_PCIE_UL_RX_FIFO_INT_ADDR_HIGH	0x1094
#define IPA_PCIE_UL_RX_FIFO_INT_ADDR_HIGH_MASK	GENMASK(7, 0)
#define IPA_PCIE_UL_RX_FIFO_INT_PATTERN		0x1098

#define IPA_PCIE_UL_TX_FIFO_INT_ADDR_LOW	0x109c
#define IPA_PCIE_UL_TX_FIFO_INT_ADDR_HIGH	0x10a0
#define IPA_PCIE_UL_TX_FIFO_INT_ADDR_HIGH_MASK	GENMASK(7, 0)
#define IPA_PCIE_UL_TX_FIFO_INT_PATTERN		0x10a4

enum ipa_mode_e {
	normal_mode,
	bypass_mode,
	mode_max,
};

/**
 * 3 is the highest priority, 0  is the lowest priority
 */
enum channel_priority_e {
	chan_prio_0,
	chan_prio_1,
	chan_prio_2,
	chan_prio_3,
};

/**
 * Description: Set ipa work mode , normal/bypass
 * Input:
 *   is_bypass: normal_mode/bypass_mode
 * Output:
 *   TRUE: Set successfully
 *   FALSE: Set failed.
 */
static inline u32 ipa_phy_set_work_mode(void __iomem *reg_base,
					u8 is_bypass)
{
	u32 flag = TRUE, ret;

	switch (is_bypass) {
	case normal_mode:
		ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
		ret &= (~IPA_WORKING_MODE_MASK);
		writel_relaxed(ret, reg_base + IPA_MODE_N_FLOWCTRL);
		ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
		if (ret & IPA_WORKING_MODE_MASK)
			flag = FALSE;
		else
			flag = TRUE;
		break;
	case bypass_mode:
		ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
		ret |= IPA_WORKING_MODE_MASK;
		writel_relaxed(ret, reg_base + IPA_MODE_N_FLOWCTRL);
		ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
		if (ret & IPA_WORKING_MODE_MASK)
			flag = TRUE;
		else
			flag = FALSE;
		break;
	default:
		pr_err("don't have this mode type\n");
		break;
	}

	return flag;
}

/**
 * Description: Get all fifo interrupt status, to check which one
 *              trigger interrupt.
 * Output:
 *   TRUE: Interrupt trigger status.
 */
static inline u32 ipa_phy_get_int_status(void __iomem *reg_base)
{
	u32 ret;

	ret = readl_relaxed(reg_base + IPA_MAP_INTERRUPT_STATUS);

	return ret;
}

/**
 * Description: Set ipa pause or resume
 * Input:
 *   @TRUE: resume ipa.
 *   @FALSE: pause ipa.
 */
static inline u32 ipa_phy_ctrl_ipa_action(void __iomem *reg_base, u32 enable)
{
	u32 ret, timeout = 500;

	if (enable) {
		ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
		ret &= (~IPA_SW_PAUSE_IPA_MASK);
		writel_relaxed(ret, reg_base + IPA_MODE_N_FLOWCTRL);

		ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
		ret |= (IPA_SW_RESUME_IPA_MASK);
		writel_relaxed(ret, reg_base + IPA_MODE_N_FLOWCTRL);

		do {
			ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
			if (!(ret & IPA_HW_READY_FOR_CHECK_MASK))
				break;

			cpu_relax();
		} while (--timeout > 0);

		if (!timeout) {
			pr_err("sipa phy resume ipa failed\n");
			return 0;
		}
	} else {
		ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
		ret &= (~IPA_SW_RESUME_IPA_MASK);
		writel_relaxed(ret, reg_base + IPA_MODE_N_FLOWCTRL);

		ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
		ret |= IPA_SW_PAUSE_IPA_MASK;
		writel_relaxed(ret, reg_base + IPA_MODE_N_FLOWCTRL);

		do {
			ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
			if (ret & IPA_HW_READY_FOR_CHECK_MASK)
				break;

			cpu_relax();
		} while (--timeout > 0);

		if (!timeout) {
			pr_err("sipa phy pause ipa failed\n");
			return 0;
		}
	}

	return TRUE;
}

static inline bool ipa_phy_get_resume_status(void __iomem *reg_base)
{
	return (IPA_SW_RESUME_IPA_MASK &
		readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL));
}

static inline bool ipa_phy_get_pause_status(void __iomem *reg_base)
{
	return (IPA_SW_PAUSE_IPA_MASK &
		readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL));
}

static inline u32 ipa_phy_enable_cp_through_pcie(void __iomem *reg_base,
						 u32 enable)
{
	u32 ret;

	if (enable) {
		ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
		ret |= (IPA_NEED_CP_THROUGH_PCIE_MASK);
		writel_relaxed(ret, reg_base + IPA_MODE_N_FLOWCTRL);
	} else {
		ret = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
		ret &= (~IPA_NEED_CP_THROUGH_PCIE_MASK);
		writel_relaxed(ret, reg_base + IPA_MODE_N_FLOWCTRL);
	}

	return TRUE;
}

/**
 * Description: Get hw ready status
 * Output:
 *   @0: HW is running.
 *   @1: HW is paused and ready for check.
 */
static inline u32 ipa_phy_get_hw_ready_to_check_sts(void __iomem *reg_base)
{
	u32 tmp = 0;

	tmp = readl_relaxed(reg_base + IPA_MODE_N_FLOWCTRL);
	tmp &= IPA_HW_READY_FOR_CHECK_MASK;

	if (tmp)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Switch hash table.
 * Input:
 *   @addrl: Lower 32 bits Base Addr of Hash Table.
 *   @addrh: High 8 bits Base Addr of Hash Table.
 *   @len: Hash Table length
 */
static inline u32 ipa_phy_hash_table_switch(void __iomem *reg_base,
					    u32 addr_l, u32 addr_h,
					    u32 len)
{
	u32 tmp;

	writel_relaxed(addr_l, reg_base + IPA_HASH_TABLE_BASE_ADDR_LOW);

	tmp = readl_relaxed(reg_base + IPA_HASH_TABLE_SWITCH_CTL);
	tmp &= (~IPA_HASH_REG_BASE_HIGH_MASK);
	tmp |= (addr_h << HASH_REG_BASE_HIGH_OFFSET);
	writel_relaxed(tmp, reg_base + IPA_HASH_TABLE_SWITCH_CTL);

	tmp = readl_relaxed(reg_base + IPA_HASH_TABLE_SWITCH_CTL);
	tmp &= (~IPA_HASH_REG_LEN_MASK);
	tmp |= (len << HASH_REG_LEN_OFFSET);
	writel_relaxed(tmp, reg_base + IPA_HASH_TABLE_SWITCH_CTL);

	tmp = readl_relaxed(reg_base + IPA_HASH_TABLE_SWITCH_CTL);
	tmp |= IPA_HASH_REG_UPDATE_MASK;
	writel_relaxed(tmp, reg_base + IPA_HASH_TABLE_SWITCH_CTL);

	tmp = readl_relaxed(reg_base + IPA_HASH_TABLE_SWITCH_CTL);

	while ((tmp & IPA_HASH_REG_DONE_MASK) == ZERO)
		tmp = readl_relaxed(reg_base + IPA_HASH_TABLE_SWITCH_CTL);

	tmp = readl_relaxed(reg_base + IPA_HASH_TABLE_SWITCH_CTL);
	tmp &= (~IPA_HASH_REG_UPDATE_MASK);
	writel_relaxed(tmp, reg_base + IPA_HASH_TABLE_SWITCH_CTL);

	return TRUE;
}

/**
 * Description: Switch hash table.
 * Input:
 *   @addrl: Lower 32 bits Base Addr of Hash Table.
 *   @addrh: High 8 bits Base Addr of Hash Table.
 *   @len: Hash Table length
 */
static inline u32 ipa_phy_get_hash_table(void __iomem *reg_base,
					 u32 *addr_l, u32 *addr_h,
					 u32 *len)
{
	u32 tmp;

	*addr_l = readl_relaxed(reg_base + IPA_HASH_TABLE_BASE_ADDR_LOW);

	tmp = readl_relaxed(reg_base + IPA_HASH_TABLE_SWITCH_CTL);
	tmp &= IPA_HASH_REG_BASE_HIGH_MASK;
	*addr_h = (tmp >> HASH_REG_BASE_HIGH_OFFSET);

	tmp = readl_relaxed(reg_base + IPA_HASH_TABLE_SWITCH_CTL);
	tmp &= (IPA_HASH_REG_LEN_MASK);
	*len = (tmp >> HASH_REG_LEN_OFFSET);

	return TRUE;
}

/**
 * Description: Control fifo interrupt source selected for mAP Interrupt.
 * Input:
 *   @enable: TRUE/FALSE to decide enable or disable.
 *   @mask: Which fifo interrupt should be selected for map interrupt.
 */
static inline u32 ipa_phy_map_interrupt_src_en(void __iomem *reg_base,
					       u32 enable, u32 mask)
{
	u32 tmp = 0;

	if (enable) {
		tmp = readl_relaxed(reg_base + IPA_MAP_INTERRUPT_SRC_EN);
		tmp |= mask;
		writel_relaxed(tmp, reg_base + IPA_MAP_INTERRUPT_SRC_EN);
		tmp = readl_relaxed(reg_base + IPA_MAP_INTERRUPT_SRC_EN);
		if (tmp & mask)
			return TRUE;
		else
			return FALSE;
	} else {
		tmp = readl_relaxed(reg_base + IPA_MAP_INTERRUPT_SRC_EN);
		tmp &= (~mask);
		writel_relaxed(tmp, reg_base + IPA_MAP_INTERRUPT_SRC_EN);
		tmp = readl_relaxed(reg_base + IPA_MAP_INTERRUPT_SRC_EN);

		if (tmp & mask)
			return FALSE;
		else
			return TRUE;
	}
}

/**
 * Description: Clear internal fifo.
 * Input:
 *   @clr_bit: Which internal fifo you want to clear , In general all.
 */
static inline u32 ipa_phy_clear_internal_fifo(void __iomem *reg_base,
					      u32 clr_bit)
{
	u32 tmp;

	clr_bit &= IPA_INTERNAL_FIFO_CLR_BIT_GROUP_MASK;
	tmp = readl_relaxed(reg_base + IPA_INTERNAL_FIFO_CLR);
	tmp |= clr_bit;
	writel_relaxed(tmp, reg_base + IPA_INTERNAL_FIFO_CLR);
	tmp &= (~clr_bit);
	writel_relaxed(tmp, reg_base + IPA_INTERNAL_FIFO_CLR);

	return TRUE;
}

static inline u32 ipa_phy_set_force_to_ap_flag(void __iomem *reg_base,
					       u32 enable, u32 bit)
{
	u32 tmp;

	if (enable) {
		tmp = readl_relaxed(reg_base + IPA_INTERNAL_FIFO_CLR);
		tmp |= bit;
		writel_relaxed(tmp, reg_base + IPA_INTERNAL_FIFO_CLR);
	} else {
		tmp = readl_relaxed(reg_base + IPA_INTERNAL_FIFO_CLR);
		tmp &= (~bit);
		writel_relaxed(tmp, reg_base + IPA_INTERNAL_FIFO_CLR);
	}

	return TRUE;
}

/**
 * Description: Set receiver's flow ctrl block for sender.
 * Input:
 *   @dst: recv fifo.
 *   @src: send fifo.
 */
static inline u32 ipa_phy_set_flow_ctrl_to_src_blk(void __iomem *reg_base,
						   u32 *dst, u32 src)
{
	u32 tmp;

	tmp = readl_relaxed(reg_base + (u64)dst);
	tmp &= src;
	writel_relaxed(tmp, reg_base + (u64)dst);
	tmp = readl_relaxed(reg_base + (u64)dst);

	if ((tmp & src) == src)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get send fifo flow ctrl interrupt status.
 * Input:
 *   @dst: Recv fifo.
 *   @src: Which send fifo that you care.
 */
static inline u32 ipa_phy_get_flow_ctrl_to_src_sts(void __iomem *reg_base,
						   u32 *dst, u32 src)
{
	u32 tmp = 0;

	tmp = readl_relaxed(reg_base + (u64)dst);

	return (tmp & src);
}

/**
 * Description: Set axi mst chn priority.
 * Input:
 *   @chan: Which chan that need to set priority.
 *   @prio: Priority value.
 */
static inline u32 ipa_phy_set_axi_mst_chn_priority(void __iomem *reg_base,
						   u32 chan, u32 prio)
{
	/*not ready*/
	u32 i = 0, size, tmp;

	size = sizeof(chan) * 8;
	for (i = 0; i < size; i++) {
		if (chan & (1 << i))
			break;
	}

	tmp = readl_relaxed(reg_base + IPA_AXI_MST_CHN_PRIORITY);

	tmp &= (~chan);
	tmp |= (prio << i);

	writel_relaxed(tmp, reg_base + IPA_AXI_MST_CHN_PRIORITY);

	return TRUE;
}

/**
 * Description: ul wiap dma enable/disable
 * Input:
 *   @enable: 1(enable), 0(disable)
 */
static inline u32 ipa_phy_ctrl_wiap_ul_dma(void __iomem *reg_base,
					   u32 enable)
{
	u32 flag, tmp;

	if (enable) {
		tmp = readl_relaxed(reg_base + IPA_CTRL);
		tmp |= IPA_WIAP_UL_DMA_EN_MASK;
		writel_relaxed(tmp, reg_base + IPA_CTRL);
		tmp = readl_relaxed(reg_base + IPA_CTRL);

		if (tmp & IPA_WIAP_UL_DMA_EN_MASK)
			flag = TRUE;
		else
			flag = FALSE;
	} else {
		tmp = readl_relaxed(reg_base + IPA_CTRL);
		tmp &= (~IPA_WIAP_UL_DMA_EN_MASK);
		writel_relaxed(tmp, reg_base + IPA_CTRL);
		tmp = readl_relaxed(reg_base + IPA_CTRL);

		if (tmp & IPA_WIAP_UL_DMA_EN_MASK)
			flag = FALSE;
		else
			flag = TRUE;
	}

	return flag;
}

static inline u32 ipa_phy_get_timestamp(void __iomem *reg_base)
{
	u32 ipa_timestamp = 0, tmp = 0;

	tmp = readl_relaxed(reg_base + IPA_HASH_TIMESTEP);

	ipa_timestamp = ((tmp & 0xFF000000l) >> 24) |
		((tmp & 0x00FF0000l) >> 8) |
		((tmp & 0x0000FF00l) << 8) |
		((tmp & 0x000000FFl) << 24);

	return ipa_timestamp;
}

static inline bool ipa_phy_enable_to_pcie_no_mac(void __iomem *reg_base,
						 bool enable)
{
	u32 tmp;
	bool flag;
	void __iomem *addr = reg_base + IPA_MAP_RX_DL_PCIE_UL_FLOWCTL_SRC;

	tmp = readl_relaxed(addr);

	if (enable)
		tmp |= TO_PCIE_NO_MAC_HEADER;
	else
		tmp &= ~TO_PCIE_NO_MAC_HEADER;

	writel_relaxed(tmp, addr);
	tmp = readl_relaxed(addr);

	if (tmp & TO_PCIE_NO_MAC_HEADER)
		flag = true;
	else
		flag = false;

	return flag == enable;
}

static inline bool ipa_phy_enable_from_pcie_no_mac(void __iomem *reg_base,
						   bool enable)
{
	u32 flag, tmp;
	void __iomem *addr = reg_base + IPA_MAP_RX_DL_PCIE_UL_FLOWCTL_SRC;

	tmp = readl_relaxed(addr);

	if (enable)
		tmp |= FROM_PCIE_NO_MAC_HEADER;
	else
		tmp &= ~FROM_PCIE_NO_MAC_HEADER;

	writel_relaxed(tmp, addr);
	tmp = readl_relaxed(addr);

	if (tmp & FROM_PCIE_NO_MAC_HEADER)
		flag = true;
	else
		flag = false;

	return flag == enable;
}

static inline bool ipa_phy_set_cp_ul_pri(void __iomem *reg_base,
					 u32 pri)
{
	u32 tmp;
	void __iomem *addr = reg_base + IPA_CP_CONFIGURATION;

	tmp = readl_relaxed(addr);
	tmp &= ~IPA_CP_UL_PRIORITY_MASK;
	tmp |= (pri << IPA_CP_UL_PRIORITY_OFFSET);
	writel_relaxed(tmp, addr);
	tmp = readl_relaxed(addr);

	if (pri == ((tmp & IPA_CP_UL_PRIORITY_MASK) >>
	     IPA_CP_UL_PRIORITY_OFFSET))
		return true;
	else
		return false;
}

static inline bool ipa_phy_set_cp_ul_dst_num(void __iomem *reg_base, u32 dst)
{
	u32 tmp;
	void __iomem *addr = reg_base + IPA_CP_CONFIGURATION;

	tmp = readl_relaxed(addr);
	tmp &= ~IPA_CP_UL_DST_TERM_NUM_MASK;
	tmp |= (dst << IPA_CP_UL_DST_TERM_NUM_OFFSET);
	writel_relaxed(tmp, addr);
	tmp = readl_relaxed(addr);

	if (dst == ((tmp & IPA_CP_UL_DST_TERM_NUM_MASK) >>
	     IPA_CP_UL_DST_TERM_NUM_OFFSET))
		return true;
	else
		return false;
}

static inline bool ipa_phy_set_cp_ul_cur_num(void __iomem *reg_base, u32 cur)
{
	u32 tmp;
	void __iomem *addr = reg_base + IPA_CP_CONFIGURATION;

	tmp = readl_relaxed(addr);
	tmp &= ~IPA_CP_UL_CUR_TERM_NUM_MASK;
	tmp |= (cur << IPA_CP_UL_CUR_TERM_NUM_OFFSET);
	writel_relaxed(tmp, addr);
	tmp = readl_relaxed(addr);

	if (cur == ((tmp & IPA_CP_UL_CUR_TERM_NUM_MASK) >>
	     IPA_CP_UL_CUR_TERM_NUM_OFFSET))
		return true;
	else
		return false;
}

static inline bool ipa_phy_set_cp_ul_flow_ctrl_mode(void __iomem *reg_base,
						u32 mode)
{
	u32 tmp;
	void __iomem *addr = reg_base + IPA_CP_CONFIGURATION;

	tmp = readl_relaxed(addr);
	tmp &= ~IPA_CP_UL_FLOW_CTRL_SEL_MASK;
	tmp |= (mode << IPA_CP_UL_FLOW_CTRL_SEL_OFFSET);
	writel_relaxed(tmp, addr);
	tmp = readl_relaxed(addr);

	if (mode == ((tmp & IPA_CP_UL_FLOW_CTRL_SEL_MASK) >>
	      IPA_CP_UL_FLOW_CTRL_SEL_OFFSET))
		return true;
	else
		return false;
}

static inline bool ipa_phy_set_cp_dl_pri(void __iomem *reg_base, u32 pri)
{
	u32 tmp;
	void __iomem *addr = reg_base + IPA_CP_CONFIGURATION;

	tmp = readl_relaxed(addr);
	tmp &= ~IPA_CP_DL_PRIORITY_MASK;
	tmp |= (pri << IPA_CP_DL_PRIORITY_OFFSET);
	writel_relaxed(tmp, addr);
	tmp = readl_relaxed(addr);

	if (pri == ((tmp & IPA_CP_DL_PRIORITY_MASK) >>
	     IPA_CP_DL_PRIORITY_OFFSET))
		return true;
	else
		return false;
}

static inline bool ipa_phy_set_cp_dl_dst_num(void __iomem *reg_base, u32 dst)
{
	u32 tmp;
	void __iomem *addr = reg_base + IPA_CP_CONFIGURATION;

	tmp = readl_relaxed(addr);
	tmp &= ~IPA_CP_DL_DST_TERM_NUM_MASK;
	tmp |= (dst << IPA_CP_DL_DST_TERM_NUM_OFFSET);
	writel_relaxed(tmp, addr);
	tmp = readl_relaxed(addr);

	if (dst == ((tmp & IPA_CP_DL_DST_TERM_NUM_MASK) >>
	     IPA_CP_DL_DST_TERM_NUM_OFFSET))
		return true;
	else
		return false;
}

static inline bool ipa_phy_set_cp_dl_cur_num(void __iomem *reg_base, u32 cur)
{
	u32 tmp;
	void __iomem *addr = reg_base + IPA_CP_CONFIGURATION;

	tmp = readl_relaxed(addr);
	tmp &= ~IPA_CP_DL_CUR_TERM_NUM_MASK;
	tmp |= (cur << IPA_CP_DL_CUR_TERM_NUM_OFFSET);
	writel_relaxed(tmp, addr);
	tmp = readl_relaxed(addr);

	if (cur == ((tmp & IPA_CP_DL_CUR_TERM_NUM_MASK) >>
	     IPA_CP_DL_CUR_TERM_NUM_OFFSET))
		return true;
	else
		return false;
}

static inline bool ipa_phy_set_cp_dl_flow_ctrl_mode(void __iomem *reg_base,
					u32 mode)
{
	u32 tmp;
	void __iomem *addr = reg_base + IPA_CP_CONFIGURATION;

	tmp = readl_relaxed(addr);
	tmp &= ~IPA_CP_DL_FLOW_CTRL_SEL_MASK;
	tmp |= (mode << IPA_CP_DL_FLOW_CTRL_SEL_OFFSET);
	writel_relaxed(tmp, addr);
	tmp = readl_relaxed(addr);

	if (mode == ((tmp & IPA_CP_DL_FLOW_CTRL_SEL_MASK) >>
	      IPA_CP_DL_FLOW_CTRL_SEL_OFFSET))
		return true;
	else
		return false;
}

static inline bool ipa_phy_ctrl_cp_work(void __iomem *reg_base,
					bool enable)
{
	u32 tmp;
	bool flag;
	void __iomem *addr = reg_base + IPA_MODE_N_FLOWCTRL;

	tmp = readl_relaxed(addr);
	if (enable)
		tmp |= IPA_CP_WORK_STATUS;
	else
		tmp &= ~IPA_CP_WORK_STATUS;

	writel_relaxed(tmp, addr);
	tmp = readl_relaxed(addr);
	if (tmp & IPA_CP_WORK_STATUS)
		flag = true;
	else
		flag = false;

	return flag == enable;
}

static inline void ipa_phy_enable_pcie_mem_intr(void __iomem *reg_base,
						bool enable)
{
	u32 tmp;

	if (enable) {
		tmp = readl_relaxed(reg_base + IPA_CTRL);
		tmp |= ENABLE_PCIE_MEM_INTR_MODE_MASK;
		writel_relaxed(tmp, reg_base + IPA_CTRL);
	} else {
		tmp = readl_relaxed(reg_base + IPA_CTRL);
		tmp &= ~ENABLE_PCIE_MEM_INTR_MODE_MASK;
		writel_relaxed(tmp, reg_base + IPA_CTRL);
	}
}

#endif
