ccflags-y += -Iinclude/drm
subdir-ccflags-y += -Idrivers/staging/android/ion/
subdir-ccflags-y += -Idrivers/devfreq/apsys/
subdir-ccflags-y += -I$(src)

obj-y := sprd_drm.o \
	sprd_gem.o \
	sprd_dpu.o \
	sprd_dsi.o \
	sprd_dphy.o \
	sprd_panel.o

obj-y += dummy/dummy_crtc.o
obj-y += dummy/dummy_connector.o

obj-y += disp_lib.o sprd_corner.o

obj-y += sprd_bl.o
obj-y += cabc/

obj-y += dpu/
obj-y += dsi/
obj-y += dphy/
obj-y += sysfs/

obj-y += sharkle/
obj-y += pike2/
obj-y += sharkl3/
obj-y += sharkl5/
obj-y += sharkl5Pro/
obj-y += roc1/

obj-$(CONFIG_DRM_SPRD_GSP) += gsp/

obj-$(CONFIG_DRM_SPRD_HDMI) += lt9611_i2c.o
