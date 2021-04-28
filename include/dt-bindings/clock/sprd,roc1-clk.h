/*
 * SPDX-License-Identifier: (GPL-2.0+ OR MIT)
 *
 * Spreadtrum ROC1 platform clocks
 *
 * Copyright (C) 2018, Spreadtrum Communications Inc.
 */

#ifndef _DT_BINDINGS_CLK_ROC1_H_
#define _DT_BINDINGS_CLK_ROC1_H_

#define CLK_ISPPLL_GATE			0
#define CLK_DPLL0_GATE			1
#define CLK_DPLL1_GATE			2
#define CLK_LPLL_GATE			3
#define CLK_TWPLL_GATE			4
#define CLK_GPLL_GATE			5
#define CLK_RPLL_GATE			6
#define CLK_MPLL0_GATE			7
#define CLK_MPLL1_GATE			8
#define CLK_MPLL2_GATE			9
#define CLK_APLL_GATE			10
#define CLK_PMU_GATE_NUM		(CLK_APLL_GATE + 1)

#define CLK_DPLL0			0
#define CLK_DPLL1			1
#define CLK_ANLG_PHY_G0_NUM		(CLK_DPLL1 + 1)

#define CLK_MPLL1			0
#define CLK_ANLG_PHY_G4_NUM		(CLK_MPLL1 + 1)

#define CLK_RPLL			0
#define CLK_AUDIO_GATE			1
#define CLK_ANLG_PHY_G5_NUM		(CLK_AUDIO_GATE + 1)

#define CLK_MPLL2			0
#define CLK_ANLG_PHY_G9_NUM		(CLK_MPLL2 + 1)

#define CLK_TWPLL			0
#define CLK_TWPLL_768M			1
#define CLK_TWPLL_384M			2
#define CLK_TWPLL_192M			3
#define CLK_TWPLL_96M			4
#define CLK_TWPLL_48M			5
#define CLK_TWPLL_24M			6
#define CLK_TWPLL_12M			7
#define CLK_TWPLL_512M			8
#define CLK_TWPLL_256M			9
#define CLK_TWPLL_128M			10
#define CLK_TWPLL_64M			11
#define CLK_TWPLL_307M2			12
#define CLK_TWPLL_219M4			13
#define CLK_TWPLL_170M6			14
#define CLK_TWPLL_153M6			15
#define CLK_TWPLL_76M8			16
#define CLK_TWPLL_51M2			17
#define CLK_TWPLL_38M4			18
#define CLK_TWPLL_19M2			19
#define CLK_LPLL			20
#define CLK_LPLL_614M4			21
#define CLK_LPLL_409M6			22
#define CLK_LPLL_245M76			23
#define CLK_GPLL			24
#define CLK_ISPPLL			25
#define CLK_ISPPLL_702M			26
#define CLK_ISPPLL_468M			27
#define CLK_APLL			28
#define CLK_ANLG_PHY_G12_NUM		(CLK_APLL + 1)

#define CLK_MPLL0			0
#define CLK_ANLG_PHY_G17_NUM		(CLK_MPLL0 + 1)

#define CLK_AP_APB		0
#define CLK_ICU			1
#define CLK_AP_UART0		2
#define CLK_AP_UART1		3
#define CLK_AP_UART2		4
#define CLK_AP_I2C0		5
#define CLK_AP_I2C1		6
#define CLK_AP_I2C2		7
#define CLK_AP_I2C3		8
#define CLK_AP_I2C4		9
#define CLK_AP_SPI0		10
#define CLK_AP_SPI1		11
#define CLK_AP_SPI2		12
#define CLK_AP_SPI3		13
#define CLK_AP_IIS0		14
#define CLK_AP_IIS1		15
#define CLK_AP_IIS2		16
#define CLK_AP_SIM		17
#define CLK_AP_CE		18
#define CLK_AP_SDIO0_2X		19
#define CLK_AP_SDIO1_2X		20
#define CLK_AP_EMMC_2X		21
#define CLK_AP_UFS		22
#define CLK_AP_VSP		23
#define CLK_AP_DISPC0		24
#define CLK_AP_DISPC0_DPI	25
#define CLK_AP_DSI_RXESC	26
#define CLK_AP_DSI_LANEBYTE	27
#define CLK_AP_VDSP		28
#define CLK_AP_VDSP_EDAP	29
#define CLK_AP_VDSP_M0		30
#define CLK_AP_CLK_NUM	(CLK_AP_VDSP_M0 + 1)

#define CLK_AON_APB		0
#define CLK_AON_ADI		1
#define CLK_AON_AUX0		2
#define CLK_AON_AUX1		3
#define CLK_AON_AUX2		4
#define CLK_AON_PROBE		5
#define CLK_AON_PWM0		6
#define CLK_AON_PWM1		7
#define CLK_AON_PWM2		8
#define CLK_AON_PWM3		9
#define CLK_AON_EFUSE		10
#define CLK_AON_UART0		11
#define CLK_AON_UART1		12
#define CLK_AON_THM0		13
#define CLK_AON_THM1		14
#define CLK_AON_THM2		15
#define CLK_AON_THM3		16
#define CLK_AON_I2C		17
#define CLK_AON_I2C1		18
#define CLK_AON_I2S		19
#define CLK_AON_SCC		20
#define CLK_APCPU_DAP		21
#define CLK_APCPU_DAP_MTCK	22
#define CLK_APCPU_TS		23
#define CLK_DEBUG_TS		24
#define CLK_DSI_TEST_S		25
#define CLK_DJTAG_TCK		26
#define CLK_DJTAG_TCK_HW	27
#define CLK_AON_TMR		28
#define CLK_PMU			29
#define CLK_DEBOUNCE		30
#define CLK_APCPU_PMU		31
#define CLK_TOP_DVFS		32
#define CLK_26M_PMU		33
#define CLK_OTG_UTMI		34
#define CLK_OTG_REF		35
#define CLK_CSSYS		36
#define CLK_CSSYS_PUB		37
#define CLK_CSSYS_APB		38
#define CLK_AP_AXI		39
#define CLK_AP_MM		40
#define CLK_AP_GPU		41
#define CLK_AP_AI		42
#define CLK_SDIO2_2X		43
#define CLK_ANALOG_IO_APB	44
#define CLK_DMC_REF		45
#define CLK_EMC			46
#define CLK_USB			47
#define CLK_EFUSE_ESE		48
#define CLK_AAPC_TEST		49
#define CLK_USB_SUSPEND		50
#define CLK_ESE_SYS		51
#define CLK_AON_CLK_NUM		(CLK_ESE_SYS + 1)

#define CLK_AUDCP_WDG_EB		0
#define CLK_AUDCP_RTC_WDG_EB		1
#define CLK_AUDCP_TMR0_EB		2
#define CLK_AUDCP_TMR1_EB		3
#define CLK_AUDCP_APB_GATE_NUM		(CLK_AUDCP_TMR1_EB + 1)

#define CLK_AUDCP_IIS0_EB		0
#define CLK_AUDCP_IIS1_EB		1
#define CLK_AUDCP_IIS2_EB		2
#define CLK_AUDCP_IIS3_EB		3
#define CLK_AUDCP_UART_EB		4
#define CLK_AUDCP_DMA_CP_EB		5
#define CLK_AUDCP_DMA_AP_EB		6
#define CLK_AUDCP_SRC48K_EB		7
#define CLK_AUDCP_MCDT_EB		8
#define CLK_AUDCP_VBCIFD_EB		9
#define CLK_AUDCP_VBC_EB		10
#define CLK_AUDCP_SPLK_EB		11
#define CLK_AUDCP_ICU_EB		12
#define CLK_AUDCP_DMA_AP_ASHB_EB	13
#define CLK_AUDCP_DMA_CP_ASHB_EB	14
#define CLK_AUDCP_AUD_EB		15
#define CLK_AUDCP_VBC_24M_EB		16
#define CLK_AUDCP_TMR_26M_EB		17
#define CLK_AUDCP_DVFS_ASHB_EB		18
#define CLK_AUDCP_AHB_GATE_NUM		(CLK_AUDCP_DVFS_ASHB_EB + 1)

#define CLK_AI_TOP_APB_EB	0
#define CLK_AI_DVFS_APB_EB	1
#define CLK_AI_MMU_APB_EB	2
#define CLK_AI_CAMBRICON_EB	3
#define CLK_AI_POWERVR_EB	4
#define CLK_AI_GATE_NUM		(CLK_AI_POWERVR_EB + 1)

#define CLK_CAMBRICON		0
#define CLK_POWERVR		1
#define CLK_AI_MTX		2
#define CLK_CAMBRICON_CFG	3
#define CLK_POWERVR_CFG		4
#define CLK_DVFS_AI		5
#define CLK_AI_NUM		(CLK_DVFS_AI + 1)

#define CLK_DSI_EB		0
#define CLK_DISPC_EB		1
#define CLK_VSP_EB		2
#define CLK_VDMA_EB		3
#define CLK_DMA_PUB_EB		4
#define CLK_DMA_SEC_EB		5
#define CLK_ICU_EB		6
#define CLK_AP_AHB_CKG_EB	7
#define CLK_BUSM_CLK_EB		8
#define CLK_AP_AHB_GATE_NUM     (CLK_BUSM_CLK_EB + 1)

#define CLK_IPA_USB_EB		0
#define CLK_IPA_USB_SUSPEND_EB	1
#define CLK_IPA_USB_REF_EB	2
#define CLK_IPA_TOP_EB		3
#define CLK_PAM_USB_EB		4
#define CLK_PAM_IPA_EB		5
#define CLK_PAM_WIFI_EB		6
#define CLK_IPA_PCIE3_EB	7
#define CLK_IPA_PCIE2_EB	8
#define CLK_BP_PAM_U3_EB	9
#define CLK_BP_PAM_IPA_EB	10
#define CLK_BP_PAM_TOP_EB	11
#define CLK_IPA_INTC_EB		12
#define CLK_IPA_BM_DBG_EB	13
#define CLK_IPA_UART_EB		14
#define CLK_IPA_TIMER_EB	15
#define CLK_IPA_WDG_EB		16
#define CLK_IPA_CM4_EB		17
#define CLK_IPA_GATE_NUM	(CLK_IPA_CM4_EB + 1)

#define CLK_IPA_CORE		0
#define CLK_IPA_MTX		1
#define CLK_IPA_APB		2
#define CLK_PCIE2_AUX		3
#define CLK_PCIE3_AUX		4
#define CLK_USB_REF		5
#define CLK_USB_PIPE		6
#define CLK_USB_UTMI		7
#define CLK_PCIE2_PIPE		8
#define CLK_PCIE3_PIPE		9
#define CLK_IPA_UART		10
#define CLK_IPA_CLK_NUM		(CLK_IPA_UART + 1)

#define CLK_GPU_CORE_EB			0
#define CLK_GPU_CORE			1
#define CLK_GPU_MEM_EB			2
#define CLK_GPU_MEM			3
#define CLK_GPU_SYS_EB			4
#define CLK_GPU_SYS			5
#define CLK_GPU_CLK_NUM			(CLK_GPU_SYS + 1)

#define CLK_MM_CPP_EB			0
#define CLK_MM_JPG_EB			1
#define CLK_MM_DCAM_EB			2
#define CLK_MM_ISP_EB			3
#define CLK_MM_CSI2_EB			4
#define CLK_MM_CSI1_EB			5
#define CLK_MM_CSI0_EB			6
#define CLK_MM_CKG_EB			7
#define CLK_ISP_AHB_EB			8
#define CLK_MM_DVFS_EB			9
#define CLK_MM_FD_EB			10
#define CLK_MM_SENSOR2_EB		11
#define CLK_MM_SENSOR1_EB		12
#define CLK_MM_SENSOR0_EB		13
#define CLK_MM_MIPI_CSI2_EB		14
#define CLK_MM_MIPI_CSI1_EB		15
#define CLK_MM_MIPI_CSI0_EB		16
#define CLK_DCAM_AXI_EB			17
#define CLK_ISP_AXI_EB			18
#define CLK_MM_CPHY_EB			19
#define CLK_MM_GATE_CLK_NUM		(CLK_MM_CPHY_EB + 1)

#define CLK_MM_AHB			0
#define CLK_MM_MTX			1
#define CLK_SENSOR0			2
#define CLK_SENSOR1			3
#define CLK_SENSOR2			4
#define CLK_CPP				5
#define CLK_JPG				6
#define CLK_FD				7
#define CLK_DCAM_IF			8
#define CLK_DCAM_AXI			9
#define CLK_ISP				10
#define CLK_MIPI_CSI0			11
#define CLK_MIPI_CSI1			12
#define CLK_MIPI_CSI2			13
#define CLK_MM_CLK_NUM			(CLK_MIPI_CSI2 + 1)

#define CLK_SIM0_EB		0
#define CLK_IIS0_EB		1
#define CLK_IIS1_EB		2
#define CLK_IIS2_EB		3
#define CLK_AP_APB_REG_EB	4
#define CLK_SPI0_EB		5
#define CLK_SPI1_EB		6
#define CLK_SPI2_EB		7
#define CLK_SPI3_EB		8
#define CLK_I2C0_EB		9
#define CLK_I2C1_EB		10
#define CLK_I2C2_EB		11
#define CLK_I2C3_EB		12
#define CLK_I2C4_EB		13
#define CLK_UART0_EB		14
#define CLK_UART1_EB		15
#define CLK_UART2_EB		16
#define CLK_SIM0_32K_DET	17
#define CLK_SPI0_LF_IN_EB	18
#define CLK_SPI1_LF_IN_EB	19
#define CLK_SPI2_LF_IN_EB	20
#define CLK_SPI3_LF_IN_EB	21
#define CLK_SDIO0_EB		22
#define CLK_SDIO1_EB		23
#define CLK_SDIO2_EB		24
#define CLK_EMMC_EB		25
#define CLK_CE_SEC_EB		26
#define CLK_CE_PUB_EB		27
#define CLK_AP_APB_GATE_NUM     (CLK_CE_PUB_EB + 1)

#define CLK_RC100M_CAL_EB	0
#define CLK_DJTAG_TCK_EB	1
#define CLK_DJTAG_EB		2
#define CLK_AUX0_EB		3
#define CLK_AUX1_EB		4
#define CLK_AUX2_EB		5
#define CLK_PROBE_EB		6
#define CLK_IPA_EB		7
#define CLK_MM_EB		8
#define CLK_SAHB_CS_EB		9
#define CLK_GPU_EB		10
#define CLK_MSPI_EB		11
#define CLK_AI_EB		12
#define CLK_APCPU_DAP_EB	13
#define CLK_AON_CSSYS_EB	14
#define CLK_CSSYS_APB_EB	15
#define CLK_CSSYS_PUB_EB	16
#define CLK_SDPHY_CFG_EB	17
#define CLK_SDPHY_REF_EB	18
#define CLK_EFUSE_EB		19
#define CLK_GPIO_EB		20
#define CLK_MBOX_EB		21
#define CLK_KPD_EB		22
#define CLK_AON_SYST_EB		23
#define CLK_AP_SYST_EB		24
#define CLK_AON_TMR_EB		25
#define CLK_DVFS_TOP_EB		26
#define CLK_OTG_UTMI_EB		27
#define CLK_OTG_PHY_EB		28
#define CLK_SPLK_EB		29
#define CLK_PIN_EB		30
#define CLK_ANA_EB		31
#define CLK_AON_APB_CKG_EB	32
#define CLK_AON_I2C1_EB		33
#define CLK_UFS_AO_EB		34
#define CLK_THM3_EB		35
#define CLK_APCPU_TS0_EB	36
#define CLK_DEBUG_FILTER_EB	37
#define CLK_AON_IIS_EB		38
#define CLK_SCC_EB		39
#define CLK_THM0_EB		40
#define CLK_THM1_EB		41
#define CLK_THM2_EB		42
#define CLK_ASIM_TOP_EB		43
#define CLK_AON_I2C0_EB		44
#define CLK_PMU_EB		45
#define CLK_ADI_EB		46
#define CLK_EIC_EB		47
#define CLK_AP_INTC0_EB		48
#define CLK_AP_INTC1_EB		49
#define CLK_AP_INTC2_EB		50
#define CLK_AP_INTC3_EB		51
#define CLK_AP_INTC4_EB		52
#define CLK_AP_INTC5_EB		53
#define CLK_AUDCP_INTC_EB	54
#define CLK_AP_TMR0_EB		55
#define CLK_AP_TMR1_EB		56
#define CLK_AP_TMR2_EB		57
#define CLK_PWM0_EB		58
#define CLK_PWM1_EB		59
#define CLK_PWM2_EB		60
#define CLK_PWM3_EB		61
#define CLK_AP_WDG_EB		62
#define CLK_APCPU_WDG_EB	63
#define CLK_SERDES_EB		64
#define CLK_ARCH_RTC_EB		65
#define CLK_KPD_RTC_EB		66
#define CLK_AON_SYST_RTC_EB	67
#define CLK_AP_SYST_RTC_EB	68
#define CLK_AON_TMR_RTC_EB	69
#define CLK_EIC_RTC_EB		70
#define CLK_EIC_RTCDV5_EB	71
#define CLK_AP_WDG_RTC_EB	72
#define CLK_AC_WDG_RTC_EB	73
#define CLK_AP_TMR0_RTC_EB	74
#define CLK_AP_TMR1_RTC_EB	75
#define CLK_AP_TMR2_RTC_EB	76
#define CLK_DCXO_LC_RTC_EB	77
#define CLK_BB_CAL_RTC_EB	78
#define CLK_DSI_CSI_TEST_EB	79
#define CLK_DJTAG_TCK_EN	80
#define CLK_DPHY_REF_EN		81
#define CLK_DMC_REF_EN		82
#define CLK_OTG_REF_EN		83
#define CLK_TSEN_EN		84
#define CLK_TMR_EN		85
#define CLK_RC100M_REF_EN	86
#define CLK_RC100M_FDK_EN	87
#define CLK_DEBOUNCE_EN		88
#define CLK_DET_32K_EB		89
#define CLK_USB_SUSPEND_EN	90
#define CLK_UFS_AO_EN		91
#define CLK_TOP_CSSYS_EN	92
#define CLK_AP_AXI_EN		93
#define CLK_SDIO0_2X_EN		94
#define CLK_SDIO0_1X_EN		95
#define CLK_SDIO1_2X_EN		96
#define CLK_SDIO1_1X_EN		97
#define CLK_SDIO2_2X_EN		98
#define CLK_SDIO2_1X_EN		99
#define CLK_EMMC_1X_EN		100
#define CLK_EMMC_2X_EN		101
#define CLK_PLL_TEST_EN		102
#define CLK_CPHY_CFG_EN		103
#define CLK_AAPC_CLK_TEST_EN	104
#define CLK_ESE_EFUSE_CTRL_EN	105
#define CLK_DEBUG_TS_EN		106
#define CLK_AON_APB_GATE_NUM    (CLK_DEBUG_TS_EN + 1)

#endif
