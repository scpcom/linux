#
#  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 #
 #  This program is free software; you can redistribute it and/or
 #  modify it under the terms of the GNU General Public License
 #  as published by the Free Software Foundation; either version 2
 #  of the License, or (at your option) any later version.

 #  This program is distributed in the hope that it will be useful,
 #  but WITHOUT ANY WARRANTY; without even the implied warranty of
 #  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 #  GNU General Public License for more details.

 #  You should have received a copy of the GNU General Public License
 #  along with this program; if not, write to the Free Software
 #  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 #
#
#

 
# FPP Makefile
#
# Invoked from top source directory


EMPTY :=
ifeq ("$(shell echo -n)","-n")
  # Windows case
  MKDIR:= md
  RMDIR:= rmdir /q/s
  ECHOESCAPE :=

else
  # Unix case
  MKDIR:= mkdir -p
  RMDIR:= rm -rf
  ECHOESCAPE := $(EMPTY)\$(EMPTY)

endif

include toolchain.mk

PFE_VERSION_FILE:=c2000/version.h

all: c2000
#all: c100 c1000 c1000_wifi c2000

c2000: class_c2000 tmu_c2000 util_c2000 util_c2000_revA0 class_sbl_c2000 tmu_sbl_c2000
class_c2000 tmu_c2000 util_c2000 util_c2000_revA0 class_sbl_c2000 tmu_sbl_c2000: version

# Target dependent variables
c100: OBJ := obj_c100
c100: CHIP := c100
c100: CFLAGS:=-O3 -Otime -g
c100: CFLAGS+=-DCOMCERTO_100
c100: CFLAGS+=-DENDIAN_LITTLE
c100: CFLAGS+=$(MY_CFLAGS)
c100: ASFLAGS:=--via extasm.via -g
c100: ASMVIAVAR = --predefine$(ECHOESCAPE) $(ECHOESCAPE)"COMCERTO_100 SETL {TRUE}$(ECHOESCAPE)"

c1000: OBJ := obj_c1000
c1000_wifi: OBJ := obj_c1000_wifi
c1000 c1000_wifi: CHIP := c1000
c1000 c1000_wifi: CFLAGS:=-O3 -Otime -g
c1000 c1000_wifi: CFLAGS+=-DCOMCERTO_1000
c1000 c1000_wifi: CFLAGS+=-DENDIAN_LITTLE
c1000 c1000_wifi: ASFLAGS:=--via extasm.via -g
c1000 c1000_wifi: ASMVIAVAR = --predefine $(ECHOESCAPE)"COMCERTO_1000 SETL {TRUE}$(ECHOESCAPE)"

class_c2000: OBJ := obj_c2000/class
tmu_c2000: OBJ := obj_c2000/tmu
util_c2000: OBJ := obj_c2000/util
util_c2000_revA0: OBJ := obj_c2000/util_revA0
class_sbl_c2000: OBJ := obj_c2000/class_sbl
tmu_sbl_c2000: OBJ := obj_c2000/tmu_sbl
class_c2000 tmu_c2000 util_c2000 util_c2000_revA0 class_sbl_c2000 tmu_sbl_c2000: CHIP := c2000
class_c2000 tmu_c2000 class_sbl_c2000 tmu_sbl_c2000: CFLAGS:= -Os -g -Wall -Werror -Wno-multichar -fno-builtin
util_c2000 util_c2000_revA0:  CFLAGS:= -O3 -g -Wall -Werror -Wno-multichar -fno-builtin
class_c2000 tmu_c2000 util_c2000 util_c2000_revA0 class_sbl_c2000 tmu_sbl_c2000: CFLAGS+=-DCOMCERTO_2000
class_c2000 tmu_c2000 util_c2000 util_c2000_revA0 class_sbl_c2000 tmu_sbl_c2000: CFLAGS+=-DENDIAN_BIG

class_c2000 class_sbl_c2000: CFLAGS+=-DCOMCERTO_2000_CLASS
tmu_c2000 tmu_sbl_c2000: CFLAGS+=-DCOMCERTO_2000_TMU
util_c2000 util_c2000_revA0 : CFLAGS+=-DCOMCERTO_2000_UTIL
util_c2000_revA0: CFLAGS+=-DREVA_WA
class_c2000: CFLAGS+=-DHW_FETCH_DISABLE
class_c2000 tmu_c2000 util_c2000 util_c2000_revA0 class_sbl_c2000 tmu_sbl_c2000: ASFLAGS:= -g
class_sbl_c2000 tmu_sbl_c2000: CFLAGS+=-DCOMCERTO_2000_SBL

export CFLAGS ASFLAGS CHIP

c100 c1000 c1000_wifi:
	@-$(MKDIR) $(OBJ)
	@-echo $(ASMVIAVAR) > $(CURDIR)/$(OBJ)/extasm.via
	$(MAKE) -C $(OBJ) -f $(CURDIR)/Makefile_obj -r $(@F) SRCDIR=.. CHIPDIR=$(CHIP) APP=fpp_$@

class_c2000 tmu_c2000 util_c2000 class_sbl_c2000 tmu_sbl_c2000:
	@-$(MKDIR) $(OBJ)
	$(MAKE) -C $(OBJ) -f $(CURDIR)/Makefile_obj -r $(@F) SRCDIR=../.. CHIPDIR=$(CHIP) APP=$@

util_c2000_revA0:
	@-$(MKDIR) $(OBJ)
	$(MAKE) -C $(OBJ) -f $(CURDIR)/Makefile_obj -r $(@F) SRCDIR=../.. CHIPDIR=$(CHIP) APP=util_c2000

config: c2000_config
config_check: c2000_config_check

%_config:
	$(MAKE) -f $(CURDIR)/config.mk -r config SRCDIR=. CHIPDIR=$(@:_config=)

%_config_check:
	$(MAKE) -f $(CURDIR)/config.mk -r config_check SRCDIR=. CHIPDIR=$(@:_config_check=)

clean: clean_c100 clean_c1000 clean_c1000_wifi clean_c2000

clean_c100 clean_c1000 clean_c1000_wifi clean_c2000: clean_% :
	@-echo Making clean in obj_$*
	@-$(RMDIR) obj_$* 

.PHONY:	clean clean_c100 clean_c1000 clean_c1000_wifi clean_c2000 c100 c1000 c1000_wifi c2000

distclean:
	rm -rf pfe-bin.tar.gz

dist: c2000
	pfe_bin_files="`find obj_c2000/ -name '*.elf' -not -name '*debug.elf' `";  tar -czf pfe-bin.tar.gz $$pfe_bin_files

version:
	if [ -d .git ]; then  \
		PFE_GIT_VERSION=$$(git describe --always --tags --dirty) ; \
		printf "/*Auto-generated file. Do not edit !*/\n#ifndef PFE_VERSION_H\n\n#define PFE_VERSION_H\n\n#define PFE_VERSION \"$${PFE_GIT_VERSION}\"\n\n#endif /* PFE_VERSION_H */\n" > $(PFE_VERSION_FILE) ; \
	fi
