/*
Dragonfly IMS to TiFF converter

https://github.com/UQ-RCC/ims2tif

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2018 The University of Queensland.

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <algorithm>
#include "ims2tif.hpp"

using namespace ims;

static h5g_ptr h5g_open_channel(hid_t tp, size_t c)
{
	char cbuf[32];
	sprintf(cbuf, "Channel %zu", c);
	return h5g_ptr(H5Gopen2(tp, cbuf, H5P_DEFAULT));
}

/* Get the chunk size and make sure it's the same for all channels. */
static int get_chunk_size(hid_t tp, size_t nchan, hsize_t& xs, hsize_t& ys, hsize_t& zs)
{
	xs = ys = zs = 0;

	for(size_t i = 0; i < nchan; ++i)
	{
		h5g_ptr chan = h5g_open_channel(tp, i);
		if(!chan)
			return -1;

		h5d_ptr dataset(H5Dopen2(chan.get(), "Data", H5P_DEFAULT));
		if(!dataset)
			return -1;

		h5p_ptr cparms(H5Dget_create_plist(dataset.get()));
		if(!cparms)
			return -1;

		if(H5D_CHUNKED != H5Pget_layout(cparms.get()))
			return -1;

		hsize_t dims[3];
		int r = H5Pget_chunk(cparms.get(), 3, dims);
		if(r < 0)
			return -1;

		if(i > 0 && (dims[0] != zs || dims[1] != ys || dims[2] != xs))
			return -1;

		zs = dims[0];
		ys = dims[1];
		xs = dims[2];
	}

	return 0;
}

void ims::converter_chunk(tiff_writer& tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan)
{
	hsize_t xcs, ycs, zcs;
	if(get_chunk_size(timepoint, nchan, xcs, ycs, zcs) < 0)
		throw hdf5_exception(); /* FIXME: not really */

	std::unique_ptr<uint16_t[]> buffer = std::make_unique<uint16_t[]>(zcs * ys * xs * nchan);

	hsize_t memdims[] = {zcs, ys, xs * nchan};
	h5s_ptr memspace(H5Screate_simple(sizeof(memdims) / sizeof(memdims[0]), memdims, nullptr));
	if(!memspace)
		throw hdf5_exception();

	size_t nzchunks = zs / zcs + static_cast<size_t>(!!(zs % zcs));
	size_t nychunks = ys / ycs + static_cast<size_t>(!!(ys % ycs));
	size_t nxchunks = xs / xcs + static_cast<size_t>(!!(xs % xcs));

	size_t npages = 0;

	for(size_t z = 0; z < nzchunks; ++z)
	{
		for(size_t c = 0; c < nchan; ++c)
		{
			h5g_ptr chan = h5g_open_channel(timepoint, c);
			h5d_ptr dataset(H5Dopen2(chan.get(), "Data", H5P_DEFAULT));
			h5s_ptr dspace(H5Dget_space(dataset.get()));
			for(size_t j = 0; j < nychunks; ++j)
			{
				for(size_t i = 0; i < nxchunks; ++i)
				{
					hsize_t offset[3] = {z * zcs, j * ycs, i * xcs};
					hsize_t stride[3] = {1, 1, 1};
					hsize_t count[3] = {zcs, ycs, xcs};
					hsize_t blocksize[3] = {1, 1, 1};

					// fprintf(stderr, "    offset = {%4zu, %4zu, %4zu},     stride = {%zu, %zu, %zu},     count = {%zu, %zu, %zu},     blocksize = {%zu, %zu, %zu}\n",
					// 	offset[0], offset[1], offset[2],
					// 	stride[0], stride[1], stride[2],
					// 	count[0], count[1], count[2],
					// 	blocksize[0], blocksize[1], blocksize[2]
					// );
					if(H5Sselect_hyperslab(dspace.get(), H5S_SELECT_SET, offset, stride, count, blocksize) < 0)
						throw hdf5_exception();

					hsize_t mem_offset[3] = {0, j * ycs, (i * xcs * nchan) + c};
					hsize_t mem_stride[3] = {1, 1, nchan};
					hsize_t mem_count[3] = {zcs, ycs, xcs};
					hsize_t mem_blocksize[3] = {1, 1, 1};
					// fprintf(stderr, "mem_offset = {%4zu, %4zu, %4zu}, mem_stride = {%zu, %zu, %zu}, mem_count = {%zu, %zu, %zu}, mem_blocksize = {%zu, %zu, %zu}\n",
					// 	mem_offset[0], mem_offset[1], mem_offset[2],
					// 	mem_stride[0], mem_stride[1], mem_stride[2],
					// 	mem_count[0], mem_count[1], mem_count[2],
					// 	mem_blocksize[0], mem_blocksize[1], mem_blocksize[2]
					// );
					if(H5Sselect_hyperslab(memspace.get(), H5S_SELECT_SET, mem_offset, mem_stride, mem_count, mem_blocksize) < 0)
						throw hdf5_exception();

					if(H5Dread(dataset.get(), H5T_NATIVE_UINT16, memspace.get(), dspace.get(), H5P_DEFAULT, buffer.get()) < 0)
						throw hdf5_exception();
				}
			}
		}

		/* Here, we should have zcs images to write. */
		for(size_t i = 0; i < zcs && npages < zs; ++i, ++npages)
		{
			uint16_t *imgstart = buffer.get() + (i * (xs * ys * nchan));
			tiff.write_page_contig(xs, ys, nchan, imgstart);
		}
	}
}
