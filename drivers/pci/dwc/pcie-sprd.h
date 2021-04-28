#include <linux/regmap.h>

#include "pcie-designware.h"

#define SPRD_PCIE_PE0_PM_CTRL			0xe60
#define SPRD_PCIE_APP_CLK_PM_EN			(0x1 << 21)

#define SPRD_PCIE_PE0_PM_STS			0xe64
#define SPRD_PCIE_PM_CURRENT_STATE_MASK		(0x7 << 8)
#define SPRD_PCIE_L0s				(0x1 << 8)
#define SPRD_PCIE_L1				(0x2 << 8)
#define SPRD_PCIE_L2				(0x3 << 8)
#define SPRD_PCIE_L3				(0x4 << 8)

#define SPRD_PCIE_PE0_TX_MSG_REG		0xe80
#define SPRD_PCIE_PME_TURN_OFF_REQ		(0x1 << 19)

#define ENTER_L2_MAX_RETRIES	10

#define PCI_DEVICE_ID_SPRD_RC	0xabcd

struct sprd_pcie {
	const char *label;
	struct dw_pcie *pci;
	struct clk *pcie_eb;

#ifdef CONFIG_SPRD_IPA_INTC
	/* These irq lines are connected to ipa level2 interrupt controller */
	u32 interrupt_line;
	u32 pme_irq;
#endif

	/* These irq lines are connected to GIC */
	u32 aer_irq;

	/* this irq cames from EIC to GIC */
	int wakeup_irq;
	struct gpio_desc *gpiod_wakeup;

	/* Save sysnopsys-specific PCIe configuration registers  */
	u32 save_msi_ctrls[MAX_MSI_CTRLS][3];

	/* keep track of pcie rc state */
	unsigned int is_powered:1;
	unsigned int is_suspended:1;

	size_t label_len; /* pcie controller device length + 10 */
	char wakeup_label[0];
};

struct sprd_pcie_of_data {
	enum dw_pcie_device_mode mode;
};

int sprd_pcie_syscon_setting(struct platform_device *pdev, char *evn);
void sprd_pcie_save_dwc_reg(struct dw_pcie *pci);
void sprd_pcie_restore_dwc_reg(struct dw_pcie *pci);
int sprd_pcie_enter_pcipm_l2(struct dw_pcie *pci);
