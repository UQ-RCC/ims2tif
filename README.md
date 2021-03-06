# Dragonfly IMS to TIFF converter

Converts Imaris files (IMS) from the Andor Dragonfly into TIFF stacks for use with Phoebe and ImageJ.

## Usage
```
Usage: ./ims2tif [OPTIONS] <file.ims>
Options:
  -h, --help
                          Display this message.
  -o, --outdir
                          Write TIFFs to the specified directory.
                          If unspecified, use the current directory.
  -p, --prefix
                          The prefix of the output file. If unspecified,
                          use the base name of the input file plus a trailing _.
  -m, --method
                          The conversion method to use. If unspecified, use "bigload".
                          Available methods are "bigload", "chunked", and "hyperslab".
  -f, --format
                          The output file format. If unspecified, use "bigtiff".
                          Available formats are "tiff", "bigtiff".
```

### Methods

Each method operates on one "TimePoint".

#### bigload

Load all the contiguous channels into memory, interleave them, then dump to disk.

* Fastest for smaller files
* Needs memory, uses `2 * x * y * z * nchan * sizeof(uint16_t)` bytes.
* TODO:
  - The interleaving procedure is an ideal candidate for GPU parallelisation.

#### chunked

For each chunk of dimension (Z, Y, X) in each source channel, interleave into the
destination.

* Processes several stacks at once, in multiples of the chunk Z-size.
* Uses hyperslabs to select chunks in the source and interleave it in the destination.
* Faster than `hyperslab`, not as fast as `bigload`.
* Much better memory utilisation than `hyperslab`.
  - Uses `chunk_z_size * ys * xs * nchan * sizeof(uint16_t)` bytes of memory.

#### hyperslab

For each z-stack, read a single channel, interleave it, then write.

* Slowest
* Will run on a toaster.
* Need to read datasets multiple times.
* Needs little memory, uses `x * y * nchan * sizeof(uint16_t)` bytes.
* Uses hyperslabs to select a single colour "plane" in the source and interleave it in the destination.

### Dependencies

* C++17
* libhdf5, libtiff
  - `apt install libhdf5-dev libtiff5-dev`

### To Build

```bash
CC=gcc-8 CXX=g++-8 CPPFLAGS="-O3" cmake -DCMAKE_BUILD_TYPE=MinSizeRel /path/to/ims2tif
make -j ims2tif
strip -s ims2tif
```

### To build statically:
Use crosstool-ng to build a GCC 8.1.0+ toolchain using musl. A configuration file is provided
under `ctng-toolchain`.

```bash
#!/bin/bash
set -e

export CC=/opt/x-tools/x86_64-pc-linux-musl/bin/x86_64-pc-linux-musl-gcc
export CXX=/opt/x-tools/x86_64-pc-linux-musl/bin/x86_64-pc-linux-musl-g++
export PREFIX=/home/zane/Desktop/springfield/ims2tif-static2/prefix

tar -xf tiff-4.0.9.tar.gz
tar -xf hdf5-1.10.3.tar.gz

mkdir -p tiff-build
pushd tiff-build
        ../tiff-4.0.9/configure --prefix=$PREFIX --enable-shared=no
        make # -j doesn't work
        make install
popd

mkdir -p hdf5-build
pushd hdf5-build
        ../hdf5-1.10.3/configure --prefix=$PREFIX --enable-shared=no \
                --enable-static-exec --disable-fortran --disable-hl \
                --disable-cxx --disable-java
        make -j
        make install
popd

mkdir -p ims2tif-build
pushd ims2tif-build
        cmake \
                -DCMAKE_INSTALL_PREFIX=$PREFIX \
                -DHDF5_USE_STATIC_LIBRARIES=On \
                -DHDF5_ROOT=$PREFIX \
                -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static" \
                ../../ims2tif
        make -j
    strip -s ims2tif
popd
```

## License

This project is licensed under the [Apache License, Version 2.0](https://opensource.org/licenses/Apache-2.0):

Copyright &copy; 2019 [The University of Queensland](http://uq.edu.au/)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
* * *

This project uses the [`parg`](https://github.com/jibsen/parg) library by Jørgen Ibsen which is licensed under [CC0](https://creativecommons.org/publicdomain/zero/1.0/).

* * *
This project uses the [`HDF5`](https://www.hdfgroup.org/solutions/hdf5/) library by [The HDF Group](https://www.hdfgroup.org/) which is licensed under the terms outlined at https://www.hdfgroup.org/licenses.

* * *

This project uses [`libtiff`](https://gitlab.com/libtiff/libtiff) library, which is licensed under the following:

    Copyright (c) 1988-1997 Sam Leffler
    Copyright (c) 1991-1997 Silicon Graphics, Inc.

    Permission to use, copy, modify, distribute, and sell this software and 
    its documentation for any purpose is hereby granted without fee, provided
    that (i) the above copyright notices and this permission notice appear in
    all copies of the software and related documentation, and (ii) the names of
    Sam Leffler and Silicon Graphics may not be used in any advertising or
    publicity relating to the software without the specific, prior written
    permission of Sam Leffler and Silicon Graphics.

    THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
    EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
    WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  

    IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
    ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
    OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
    WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
    LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
    OF THIS SOFTWARE.
