#
# Makefile for the sprd staging files
#
obj-y					+= mailbox/
obj-y					+= sfp/
obj-$(CONFIG_SPRD_SIPA)			+= sipa/
obj-$(CONFIG_SPRD_PAM_IPA)		+= pam_ipa/
obj-$(CONFIG_SPRD_SIPA_DELEGATE)	+= sipa_delegate/
obj-$(CONFIG_SPRD_IPA_SYS)		+= sipa_sys/
obj-$(CONFIG_SPRD_DATA_TRANSPORTER)	+= log-tp.o
log-tp-objs				+= log-tp-main.o log-fw.o
obj-$(CONFIG_SPRD_MODEM_DEBUG)		+= debug_log/
