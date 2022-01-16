obj-$(CONFIG_DISP2_TV_GM7121) += gm7121.o

obj-$(CONFIG_DISP2_TV_AC200) += tv_acxx.o
tv_acxx-y := tv_ac200.o tv_ac200_lowlevel.o

obj-$(CONFIG_TV_DISP2_SUNXI) += tv.o
tv-y := drv_tv.o
ifeq ($(CONFIG_ARCH_SUN50IW2),y)
tv-y += de_tve_v1.o
else
ifeq ($(CONFIG_ARCH_SUN8IW7),y)
tv-y += de_tve_sun8iw7.o
else
tv-y += de_tve_sun8iw11.o
endif
endif
