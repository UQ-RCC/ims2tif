#!/bin/sh -e

IMS2TIF_REPO=https://github.com/UQ-RCC/ims2tif

if [ -z ${PREFIX} ]; then
	echo "PREFIX not set, this is a build error."
	exit 1
fi

if [ $# -ne 1 ]; then
	echo "Usage: $0 <tag|commit>"
	exit 2
fi

if [ ! -d ims2tif ]; then
	git clone -n ${IMS2TIF_REPO}
fi

git -C ims2tif checkout $1

mkdir -p ims2tif-build
cd ims2tif-build

# FindTIFF.cmake seems allergic to searching the prefix
cmake \
	-DHDF5_USE_STATIC_LIBRARIES=On \
	-DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static" \
	-DTIFF_INCLUDE_DIR=${PREFIX}/include \
	-DTIFF_LIBRARY=${PREFIX}/lib/libtiff.a \
	-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=On \
	-DCMAKE_INSTALL_PREFIX=${PREFIX} \
	-DCMAKE_BUILD_TYPE=Release \
	../ims2tif
make -j ims2tif
strip -s ims2tif
