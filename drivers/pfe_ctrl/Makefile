#
#  Copyright (C) 2007 Freescale Semiconductor, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 
PLATFORM?=C2000
UNIT_TEST?=n
UNIT_TEST_HIF?=n
#PLATFORM?=EMULATION
#PLATFORM?=PCI
#UNIT_TEST?=y
#UNIT_TEST_HIF?=y

ifeq ($(PLATFORM),C2000)
CROSS_COMPILE?=/home/upputuv/work/C2000/WiFi/porting_lsdk-10.1.45/sdk-cpe/staging_dir/toolchain-arm_v7-a_gcc-4.5-linaro_glibc-2.14_eabi/bin/arm-openwrt-linux-
KERNELDIR?=/home/upputuv/work/C2000/WiFi/porting_lsdk-10.1.45/sdk-cpe/build_dir/linux-comcerto2000_hgw/linux-3.2.26
ARCH=arm
else
ARCH=x86
KERNELDIR=/lib/modules/`uname -r`/build
endif

# The following must be a relative path, or the kernel build system will fail
PFE_DIR?=../pfe

PFE_CTRL_VERSION_FILE:=version.h


all: version config modules

modules clean:
	make CROSS_COMPILE="$(CROSS_COMPILE)" V=1 ARCH=$(ARCH) -C $(KERNELDIR) M=`pwd` $@

EXTRA_CFLAGS += -Werror -include $(PWD)/$(src)/$(PFE_DIR)/c2000/version.h -I$(src)/$(PFE_DIR)/c2000 -I$(src)/$(PFE_DIR)/common -I$(src) -DENDIAN_LITTLE -DGCC_TOOLCHAIN -DCOMCERTO_2000 -DCOMCERTO_2000_CONTROL

EXTRA_CFLAGS += -I$(srctree)/arch/arm/mach-comcerto/include

EXTRA_LDFLAGS += -T$(PWD)/$(src)/control_link.lds

obj-m += pfe.o

pfe_ctrl_objs = $(PFE_DIR)/c2000/__pfe_ctrl.o \
	$(PFE_DIR)/c2000/pfe.o \
	$(PFE_DIR)/common/control_ipv4.o \
	$(PFE_DIR)/common/control_ipv6.o \
	$(PFE_DIR)/common/ipv6.o \
	$(PFE_DIR)/common/control_mc4.o \
	$(PFE_DIR)/common/control_mc6.o \
	$(PFE_DIR)/common/multicast.o \
	$(PFE_DIR)/common/control_pppoe.o \
	$(PFE_DIR)/common/control_vlan.o \
	$(PFE_DIR)/common/control_macvlan.o \
	$(PFE_DIR)/common/query_ipv4.o \
	$(PFE_DIR)/common/query_ipv6.o \
	$(PFE_DIR)/common/query_mc.o \
	$(PFE_DIR)/common/query_pppoe.o \
	$(PFE_DIR)/common/query_vlan.o \
	$(PFE_DIR)/common/query_tunnel.o \
	$(PFE_DIR)/common/query_ipsec.o \
	$(PFE_DIR)/common/query_Rx.o \
	$(PFE_DIR)/c2000/control_expt.o \
	$(PFE_DIR)/common/control_tunnel.o \
	$(PFE_DIR)/common/module_hidrv.o \
	$(PFE_DIR)/c2000/control_storage.o \
	$(PFE_DIR)/common/layer2.o \
	$(PFE_DIR)/common/alt_conf.o \
	$(PFE_DIR)/common/control_stat.o \
	$(PFE_DIR)/c2000/control_rx.o \
	$(PFE_DIR)/c2000/control_tx.o \
	$(PFE_DIR)/common/control_socket.o \
	$(PFE_DIR)/common/control_rtp_relay.o \
	$(PFE_DIR)/common/control_common.o \
	$(PFE_DIR)/c2000/control_qm.o \
	$(PFE_DIR)/common/fppdiag_lib.o \
	$(PFE_DIR)/c2000/control_icc.o \
	$(PFE_DIR)/common/control_voicebuf.o \
	$(PFE_DIR)/common/control_ipsec.o \
	$(PFE_DIR)/c2000/control_ipsec.o \
	$(PFE_DIR)/c2000/control_ipsec_standalone.o \
	$(PFE_DIR)/common/control_bridge.o \
	$(PFE_DIR)/common/control_wifi_rx.o \
	$(PFE_DIR)/c2000/control_ipsec.o \
	$(PFE_DIR)/common/control_natpt.o \
	$(PFE_DIR)/common/control_ipv4frag.o \
	$(PFE_DIR)/common/control_ipv6frag.o \
	$(PFE_DIR)/common/control_capture.o \
	$(PFE_DIR)/c2000/control_l2tp.o \

pfe-y += pfe_mod.o \
	pfe_hw.o \
	pfe_firmware.o \
	pfe_ctrl.o \
	pfe_ctrl_hal.o \
	pfe_hif.o \
	pfe_hif_lib.o\
	pfe_eth.o \
	pfe_pcap.o \
	pfe_vwd.o \
	pfe_perfmon.o \
	pfe_tso.o \
	pfe_sysfs.o \
	pfe_diags.o \
	$(pfe_ctrl_objs) \

pfe-$(CONFIG_COMCERTO_MSP) += pfe_mspsync.o

ifeq ($(UNIT_TEST),y)
pfe-y += pfe_unit_test.o
EXTRA_CFLAGS+=-DCONFIG_UNIT_TEST

ifeq ($(UNIT_TEST_HIF),y)
EXTRA_CFLAGS+=-DCONFIG_UNIT_TEST_HIF
endif

endif

ifeq ($(PLATFORM),C2000)
pfe-y += pfe_platform.o
EXTRA_CFLAGS+=-DCONFIG_PLATFORM_C2000
else ifeq ($(PLATFORM),PCI)
pfe-y += pfe_pci.o
EXTRA_CFLAGS+=-DCONFIG_PLATFORM_PCI
EXTRA_CFLAGS+=-DCONFIG_UTIL_DISABLED
#EXTRA_CFLAGS+=-DCONFIG_TMU_DUMMY
else ifeq ($(PLATFORM),EMULATION)
pfe-y += pfe_emulation.o
EXTRA_CFLAGS+=-DCONFIG_PLATFORM_EMULATION
endif

distclean:
	rm -rf pfe_ctrl-src.tar.gz pfe-src.tar.gz

dist: all pfe_release_tar
	pfe_ctrl_files="`find . -name '*.h'; find . -name '*.c'; find . -name '*.lds'; find . -name 'Makefile'`"; tar -czf pfe_ctrl-src.tar.gz $$pfe_ctrl_files

pfe_release_tar:
	pfe_files="`find $(PFE_DIR)/c2000 -name '*.h'; find $(PFE_DIR)/common -name '*.h'` $(PFE_DIR)/Makefile $(PFE_DIR)/config.mk $(PFE_DIR)/c2000/config.mk $(PFE_DIR)/toolchain.mk $(PFE_DIR)/license.txt $(PFE_DIR)/license_full.txt $(pfe_ctrl_objs:.o=.c)"; tar -cf pfe-src.tar $$pfe_files; tar -xf pfe-src.tar;bash pfe_licence.sh `basename $(PFE_DIR)`; rm `basename $(PFE_DIR)`/license.txt `basename $(PFE_DIR)`/license_full.txt;tar -czf pfe-src.tar.gz `basename $(PFE_DIR)`

version:
	if [ -d .git ]; then  \
		make -C $(PFE_DIR) version ; \
		PFE_CTRL_GIT_VERSION=$$(git describe --always --tags --dirty) ; \
		printf "/*Auto-generated file. Do not edit !*/\n#ifndef VERSION_H\n#define VERSION_H\n\n#define PFE_CTRL_VERSION \"$${PFE_CTRL_GIT_VERSION}\"\n\n#endif /* VERSION_H */\n" > $(PFE_CTRL_VERSION_FILE) ; \
	fi

config:
	make -C $(PFE_DIR) config_check

PFE_BIN_DIR=$(PFE_DIR)/obj_c2000/
CLASS_BIN=$(PFE_BIN_DIR)/class/class_c2000_debug.elf
UTIL_BIN=$(PFE_BIN_DIR)/util/util_c2000_debug.elf
TMU_BIN=$(PFE_BIN_DIR)/tmu/tmu_c2000_debug.elf
check_dmem:
	./check_shared_mem.py pfe.ko $(CLASS_BIN) $(TMU_BIN) $(UTIL_BIN)

