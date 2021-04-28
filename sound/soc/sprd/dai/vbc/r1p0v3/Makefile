# SPRD VBC Support
SPRD_SOUND_TREE := sound/soc/sprd/
KBUILD_CFLAGS	+= -I$(SPRD_SOUND_TREE) \
		   -I$(SPRD_SOUND_TREE)/dai \
		   -I$(SPRD_SOUND_TREE)/dai/vbc/ \
		   -I$(SPRD_SOUND_TREE)/platform

snd-soc-sprd-vbc-r1p0v3-objs       := vbc.o
obj-$(CONFIG_SND_SOC_SPRD_VBC_R1P0V3)    += snd-soc-sprd-vbc-r1p0v3.o
