#!/bin/bash -e
green="\e[0;32m"
red="\e[0;31m"
blue="\e[0;34m"
end_color="\e[0m"

[ "${ARCH}" != "" ] || ARCH=arm64

[ "${BOARD_SHORT}" != "" ] || BOARD_SHORT=${BOARD}
[ "${BOARD_SHORT}" != "" ] || BOARD_SHORT=n1

[ "X$GIT_SOURCE_HOST" != "X" ] || GIT_SOURCE_HOST=github.com
[ "X$GIT_TARGET_HOST" != "X" ] || GIT_TARGET_HOST=$GIT_HOST
[ "X$GIT_SOURCE_USER" != "X" ] || GIT_SOURCE_USER=scpcom
[ "X$GIT_TARGET_USER" != "X" ] || GIT_TARGET_USER=$GIT_USER
[ "X$GIT_TARGET_USER" != "X" ] || GIT_TARGET_USER=$GIT_SOURCE_USER

if [ "X$GIT_TARGET_HOST" = "X" ]; then
  GIT_TARGET_HOST=$GIT_SOURCE_HOST
fi

if [ "X$GIT_RELEASES_URL" = "X" ]; then
  GIT_RELEASES_URL=https://$GIT_TARGET_HOST
fi

GIT_SOURCE_USER_URL=https://$GIT_SOURCE_HOST/$GIT_SOURCE_USER
GIT_TARGET_USER_URL=https://$GIT_TARGET_HOST/$GIT_TARGET_USER
GIT_USER_URL=$GIT_TARGET_USER_URL

[ "X$GIT_REF" = "X" ] && GIT_REF="develop"

BUILDDIR="/build"

echo "${blue}Board: ${BOARD_SHORT}${end_color}"

bs=${BUILDDIR}/sdk-prepare-checkout-stamp
if [ ! -e $bs ]; then
  echo "\n${green}Checking out SDK for ${BOARD_SHORT}${end_color}\n"
  mkdir ${BUILDDIR}
  cd ${BUILDDIR}
  wget -N https://seafile.servator.de/sbc/odroid/build/next/linux-6.12-sbc-build.tar.gz
  tar xzf linux-6.12-sbc-build.tar.gz
  # Next line is optional to speed-up clone and reduze size
  sed -i s/'git clone -b mainline-${lxmmmkb}'/'git clone --depth=100 -b sbc-${lxmmmkb}.y'/g prepare-linux.sh
  # Disable uneeded parts
  sed -i 's|if .*e drivers/gpu/arm/midgard .*; then|if false ; then|g' prepare-linux.sh
  touch $bs
fi

bs=${BUILDDIR}/sdk-prepare-patch-stamp
if [ ! -e $bs ]; then
  echo "\n${green}Patching SDK for ${BOARD_SHORT}${end_color}\n"
  apt-get update
  cd ${BUILDDIR}
  [ "${ARCH}" != "riscv64" ] || CROSS_GCC=-riscv64-linux-gnu CROSS_DEBARCH=-riscv64-cross bash -e build-deps.sh
  [ "${ARCH}" != "arm64"   ] || CROSS_GCC=-aarch64-linux-gnu CROSS_DEBARCH=-arm64-cross bash -e build-deps.sh
  [ "${ARCH}" != "armhf"   ] || CROSS_GCC=-arm-linux-gnueabihf CROSS_DEBARCH=-armhf-cross bash -e build-deps.sh
  apt-get install -y git u-boot-tools
  touch $bs
fi

bs=${BUILDDIR}/sdk-compile-stamp
if [ ! -e $bs ]; then
  echo "\n${green}Building SDK for ${BOARD_SHORT}${end_color}\n"
  cd ${BUILDDIR}
  . ./build-env.sh
  lxmmmkb=`echo ${lxdebkb} | cut -d '.' -f 1-2`
  rm -f linux-${lxmmmkb}-sbc-build.tar.gz
  echo "Creating linux-${lxmmmkb}-sbc-build.tar.gz..."
  tar czvf linux-${lxmmmkb}-sbc-build.tar.gz build-deps.sh build-env.sh build-linux-arm64.sh *-linux.sh *-dtb.sh *-image.sh *-uinitrd.sh *-mali*-driver.sh *-mali-dev.sh build-vdec-fw.sh build-wlan-fw.sh patches patches-optional mali450-meson/DEBIAN
  [ "${BOARD_SHORT}" != "spacemit" ] || sed -i s/'git clone --depth=100 -b sbc-${lxmmmkb}.y'/'git clone --depth=100 -b '${BOARD_SHORT}'-${lxmmmkb}.y'/g prepare-linux.sh
  sed -i 's|https://github.com/scpcom|'${GIT_USER_URL}'|g' prepare-linux.sh
  sed -i 's|https://github.com/LibreELEC|'${GIT_USER_URL}'|g' build-wlan-fw.sh
  sed -i 's|https://github.com/LibreELEC|'${GIT_USER_URL}'|g' build-vdec-fw.sh
  bash -e prepare-linux.sh -- ${BOARD_SHORT}
  KDEB_DATE=$(cd linux && git log -1 --format="%at" | xargs -I{} date -d @{} +%Y%m%d) ; sed -i 's|KDEB_PKGVERSION="${VERSION}.${PATCHLEVEL}.${SUBVERSION}-........"|KDEB_PKGVERSION="${VERSION}.${PATCHLEVEL}.${SUBVERSION}-'$KDEB_DATE'"|g' build-linux.sh
  bash -e config-linux.sh -- ${BOARD_SHORT}
  if [ -e build-linux-${ARCH}.sh ]; then
    bash -e build-linux-${ARCH}.sh -- ${BOARD_SHORT}
  else
    bash -e build-linux.sh -- ${BOARD_SHORT}
  fi
  bash -e clean-linux.sh -- ${BOARD_SHORT}
  touch $bs
fi

bs=${BUILDDIR}/sdk-output-stamp
if [ ! -e $bs ]; then
  echo "\n${green}Packing Image for ${BOARD_SHORT}${end_color}\n"
  cd ${BUILDDIR} && cp -p linux-image-*.zip /output/
  cd ${BUILDDIR} && [ "${BOARD_SHORT}" != "n1" ] || cp -p linux-*-sbc-build.tar.gz /output/
  echo "\n${green}Image for ${BOARD_SHORT} is linux-image-${BOARD_SHORT}-${ARCH}.zip${end_color}\n"
  touch $bs
fi

