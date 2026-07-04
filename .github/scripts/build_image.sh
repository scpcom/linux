#!/bin/sh
docker run --privileged -it --rm -v `pwd`/image:/output builder bash -e -c "ARCH=arm64 BOARD=n1 ./make_image.sh"
