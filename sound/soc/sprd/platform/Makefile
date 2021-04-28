# SPRD Platform Support
SPRD_SOUND_TREE			:= sound/soc/sprd/
SPRD_AUDIO_ASSISTANT_TREE	:= drivers/misc/sprd_audio/
SPRD_MCDT_TREE			:= drivers/misc/sprd_audio/mcdt/
ccflags-y	+= -I$(SPRD_AUDIO_ASSISTANT_TREE)/audiomem/ \
		   -I$(SPRD_AUDIO_ASSISTANT_TREE)/audiosipc/ \
		   -I$(SPRD_MCDT_TREE)


ifneq ($(CONFIG_SND_SOC_SPRD_AUDIO_TWO_STAGE_DMAENGINE_SURPPORT),)
	snd-soc-sprd-audio-dma-objs       := sprd-2stage-dmaengine-pcm.o
else
	snd-soc-sprd-audio-dma-objs := sprd-dmaengine-pcm.o \
	sprd-platform-pcm-routing.o sprd-compr-2stage-dma.o
endif

obj-$(CONFIG_SND_SOC_SPRD)       += snd-soc-sprd-audio-dma.o
