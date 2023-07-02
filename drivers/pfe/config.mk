#
 #  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 #
 #  This program is free software; you can redistribute it and/or
 #  modify it under the terms of the GNU General Public License
 #  as published by the Free Software Foundation; either version 2
 #  of the License, or (at your option) any later version.
 #
 #  This program is distributed in the hope that it will be useful,
 #  but WITHOUT ANY WARRANTY; without even the implied warranty of
 #  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 #  GNU General Public License for more details.
 #
 #  You should have received a copy of the GNU General Public License
 #  along with this program; if not, write to the Free Software
 #  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 #
#
include $(SRCDIR)/$(CHIPDIR)/config.mk

CFG_ALL:=\#define CFG_ALL			(0
CFG_OPTIONS:=
CFG_H_OPTIONS:=

define add_option
CFG_H_OPTIONS+=echo '\#define$(1)		(1 <<$(2))' >> $(SRCDIR)/$(CHIPDIR)/config.h;
CFG_ALL+=| $(1)
CFG_OPTIONS+=echo '$(1):=y' >> $(SRCDIR)/$(CHIPDIR)/.config.mk;
endef

ifeq ($(CFG_LRO),y)
$(eval $(call add_option, CFG_LRO, 0))
endif

ifeq ($(CFG_WIFI_OFFLOAD),y)
$(eval $(call add_option, CFG_WIFI_OFFLOAD, 1))
endif

ifeq ($(CFG_STATS),y)
$(eval $(call add_option, CFG_STATS, 2))
endif


ifeq ($(CFG_PCAP),y)
$(eval $(call add_option, CFG_PCAP, 3))
endif


ifeq ($(CFG_MACVLAN),y)
$(eval $(call add_option, CFG_MACVLAN, 4))
endif


ifeq ($(CFG_BR_MANUAL),y)
$(eval $(call add_option, CFG_BR_MANUAL, 5))
endif


ifeq ($(CFG_STANDALONE),y)
$(eval $(call add_option, CFG_STANDALONE, 6))
endif


ifeq ($(CFG_DIAGS),y)
$(eval $(call add_option, CFG_DIAGS, 7))
endif

ifeq ($(CFG_PE_DEBUG),y)
$(eval $(call add_option, CFG_PE_DEBUG, 8))
endif


ifeq ($(CFG_IPSEC_DEBUG),y)
$(eval $(call add_option, CFG_IPSEC_DEBUG, 9))
endif


ifeq ($(CFG_DEBUG_COUNTERS),y)
$(eval $(call add_option, CFG_DEBUG_COUNTERS, 10))
endif


ifeq ($(CFG_ICC),y)
$(eval $(call add_option, CFG_ICC, 11))
endif


ifeq ($(CFG_NATPT),y)
$(eval $(call add_option, CFG_NATPT, 12))
endif

ifeq ($(CFG_WIFI_GUEST),y)
$(eval $(call add_option, CFG_WIFI_GUEST, 13))
endif


CFG_ALL+=)

define config_h
echo "#ifndef _CONFIG_H_" > $(1)
echo "#define _CONFIG_H_" >> $(1)
$(CFG_H_OPTIONS)
echo '$(CFG_ALL)' >> $(1)
echo "#endif /* _CONFIG_H_ */" >> $(1)
endef

define config_mk
echo -n > $(SRCDIR)/$(CHIPDIR)/.config.mk
$(CFG_OPTIONS)
endef

$(SRCDIR)/$(CHIPDIR)/config.h: $(SRCDIR)/$(CHIPDIR)/config.mk
	$(call config_h, $@)

$(SRCDIR)/$(CHIPDIR)/.config.mk: $(SRCDIR)/$(CHIPDIR)/config.mk
	$(call config_mk)

.PHONY: config

config_check: $(SRCDIR)/$(CHIPDIR)/config.h $(SRCDIR)/$(CHIPDIR)/.config.mk

config:
	$(call config_h, $(SRCDIR)/$(CHIPDIR)/config.h)
	$(call config_mk)

include $(SRCDIR)/$(CHIPDIR)/.config.mk
