subdir-ccflags-y := \
	-I$(src)/include \
	-I$(src)/boot \
	-I$(src)/platform \
	-I$(src)/pcie \
	-I$(src)/platform/rf

obj-$(CONFIG_SC23XX) += wcn_bus.o
obj-$(CONFIG_WCN_SIPC) += sipc/
obj-$(CONFIG_WCN_BOOT) += platform/
obj-$(CONFIG_SC2342_INTEG) += boot/
obj-$(CONFIG_WCN_PLATFORM) += platform/
obj-$(CONFIG_SDIOHAL) += sdio/
obj-$(CONFIG_WCN_PCIE) += pcie/
obj-$(CONFIG_WCN_SLP) += sleep/
