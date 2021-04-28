# SPRD Audio Dsp Log Support

SPRD_SND_TREE	:= sound/soc/sprd/
SPRD_AUDIO_ASSISTANT_TREE := drivers/misc/sprd_audio/
ccflags-y += -I$(SPRD_AUDIO_ASSISTANT_TREE)/audiomem/ \
	     -I$(SPRD_AUDIO_ASSISTANT_TREE)/audiosipc/ \
	     -I$(SPRD_AUDIO_ASSISTANT_TREE)/agdsp_access/ \
	     -I$(SPRD_SND_TREE)/include

obj-$(CONFIG_SPRD_AUDIO_DSP_DUMP) +=  audio_sblock.o audio_dsp_dump.o
