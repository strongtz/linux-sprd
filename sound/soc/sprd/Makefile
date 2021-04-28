# SPRD Sound Card Support

SPRD_SOUND_TREE				:= sound/soc/sprd/
SPRD_AUDIO_ASSISTANT_TREE		:= drivers/misc/sprd_audio/
ccflags-y				+= -I$(SPRD_SOUND_TREE)/codec/sprd/
subdir-ccflags-y			+= -I$(SPRD_SOUND_TREE)/include/ \
					   -I$(SPRD_AUDIO_ASSISTANT_TREE)/agdsp_access/

obj-$(CONFIG_SND_SOC_SPRD)		+= sprd-asoc-common.o sprd-asoc-card-utils.o \
					   sprd-asoc-card-utils-legacy.o \
					   sprd-asoc-card-utils-hook.o
obj-$(CONFIG_SND_SOC_SPRD)		+= platform/
obj-$(CONFIG_SND_SOC_SPRD)		+= codec/dummy-codec/
obj-$(CONFIG_SND_SOC_SPRD_VBC_V4)	+= dai/vbc/v4/

obj-$(CONFIG_SND_SOC_SPRD_VBC_R1P0V3)	+= dai/vbc/r1p0v3/
obj-$(CONFIG_SND_SOC_SPRD_VAUDIO)	+= dai/vaudio/

obj-$(CONFIG_SND_SOC_SPRD_I2S)		+= dai/i2s/
obj-$(CONFIG_SND_SOC_SPRD_CODEC_SC2730)	+= codec/sprd/sc2730/
obj-$(CONFIG_SND_SOC_SPRD_CODEC_SC2721)	+= codec/sprd/sc2721/
obj-$(CONFIG_SND_SOC_SPRD_PA_UCP1301)	+= codec/sprd/ucp1301/

# Machines
obj-$(CONFIG_SND_SOC_SPRD_VBC_V4_SC2730)	+= vbc-rxpx-codec-sc27xx.o

obj-$(CONFIG_SND_SOC_SPRD_R1P0V3_SC2721)	+= vbc-rxpx-codec-sc27xx.o

snd-soc-i2s-dummy-objs         		:= i2s-r0p0-dummy-codec.o
obj-$(CONFIG_SND_SOC_SPRD_I2S_DUMMY)    += snd-soc-i2s-dummy.o
