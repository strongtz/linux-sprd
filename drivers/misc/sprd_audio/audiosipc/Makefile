# SPRD Audio SIPC Support

SPRD_SND_TREE   := sound/soc/sprd/
SPRD_AUDIO_ASSISTANT_TREE := drivers/misc/sprd_audio/
ccflags-y	+= -Idrivers/staging/sprd/modem/include/ \
		   -I$(SPRD_AUDIO_ASSISTANT_TREE)/audiomem/ \
		   -I$(SPRD_SND_TREE)/include/

obj-$(CONFIG_SPRD_AUDIO_SIPC) += audio-sipc.o audio-smsg.o audio-pipe.o
