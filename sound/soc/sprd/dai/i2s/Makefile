# SPRD I2S Support
SPRD_SOUND_TREE := sound/soc/sprd/
KBUILD_CFLAGS   += -I$(SPRD_SOUND_TREE) \
                   -I$(SPRD_SOUND_TREE)/dai/i2s/ \
                   -I$(SPRD_SOUND_TREE)/platform
snd-soc-sprd-i2s-objs       := i2s.o
obj-$(CONFIG_SND_SOC_SPRD_I2S)       += snd-soc-sprd-i2s.o
