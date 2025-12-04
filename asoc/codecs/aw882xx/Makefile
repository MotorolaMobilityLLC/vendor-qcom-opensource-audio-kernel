
# Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
# Copyright 2020 GOODIX 
#
#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 2 as
#published by the Free Software Foundation.

# make sure that the environment variables ARCH and CROSS_COMPILE
# are set for your architecture and cross compiler.
# And that KDIR points to the kernel to build against.
#
# e.g.:
# export ARCH=arm
# export CROSS_COMPILE=arm-linux-gnueabihf-
# export KDIR=../linux


# In case of out of tree build, build as a module
# (when build inside kernel we will not enter this directory and this will have no effect)
ifeq ($(CONFIG_SND_SOC_AW882XX),)
CONFIG_SND_SOC_AW882XX := m
endif

ifneq ($(KERNELRELEASE),)

EXTRA_CFLAGS += -I$(src)/inc
EXTRA_CFLAGS += -Werror

obj-$(CONFIG_SND_SOC_AW882XX) := snd-soc-aw882xx.o

snd-soc-aw882xx-objs += src/aw882xx.o
snd-soc-aw882xx-objs += src/aw882xx_dsp.o
snd-soc-aw882xx-objs += src/aw882xx_init.o
snd-soc-aw882xx-objs += src/aw882xx_spin.o
snd-soc-aw882xx-objs += src/aw882xx_calib.o
snd-soc-aw882xx-objs += src/aw882xx_device.o
snd-soc-aw882xx-objs += src/aw882xx_monitor.o
snd-soc-aw882xx-objs += src/aw882xx_bin_parse.o
else

MAKEARCH := $(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

all:
	$(MAKEARCH) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKEARCH) -C $(KDIR) M=$(PWD) clean
	rm -f $(snd-soc-aw882xx-objs)

endif
