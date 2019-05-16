FROM    alpine:3.9

RUN     apk add --update gcc g++ musl-dev file make git cmake

ENV		CFLAGS="-O3 -fno-stack-protector"
ENV		CXXFLAGS="-O3 -fno-stack-protector"
ENV		PREFIX=/tmp/ims2tif-prefix

WORKDIR	/tmp
COPY    hdf5-1.10.4.tar.gz /tmp
RUN     tar -xf /tmp/hdf5-1.10.4.tar.gz && \
		mkdir -p hdf5-build && \
		cd hdf5-build && \
    	../hdf5-1.10.4/configure \
			--prefix=${PREFIX} \
			--enable-shared=no \
			--enable-static-exec \
			--disable-fortran \
			--disable-cxx \
			--disable-java \
			--without-zlib && \
		make -j && \
		make install && \
		rm -rf /tmp/hdf5-*

COPY	tiff-4.0.9.tar.gz /tmp
RUN		tar -xf /tmp/tiff-4.0.9.tar.gz && \
		mkdir -p tiff-build && \
		cd tiff-build && \
		../tiff-4.0.9/configure \
			--prefix=${PREFIX} \
			--enable-shared=no \
			--disable-ccitt \
			--disable-packbits \
			--disable-lzw \
			--disable-thunder \
			--disable-next \
			--disable-logluv \
			--disable-mdi \
			--disable-zlib \
			--disable-jpeg \
			--disable-old-jpeg \
			--disable-jbig \
			--disable-lzma \
			--disable-cxx && \
		make && \
		make install && \
		rm -rf /tmp/tiff-*

COPY	build.sh /

# Run like docker run --rm -it -v$PWD:/tmp/ims2tif-build --user=1000 ims2tif-build
# The output will be in $PWD/ims2tif
ENTRYPOINT	["/build.sh"]