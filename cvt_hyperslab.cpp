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

#include "ims2tif.hpp"

using namespace ims;

static int chan_read_hyperslab(hid_t tp, size_t channel, uint16_t *data, size_t z, size_t xs, size_t ys, size_t zs, hsize_t nchan)
{
	char cbuf[32];
	sprintf(cbuf, "Channel %zu", channel);
	h5g_ptr chan(H5Gopen2(tp, cbuf, H5P_DEFAULT));
	if(!chan)
		return -1;

	h5d_ptr dataset(H5Dopen2(chan.get(), "Data", H5P_DEFAULT));
	if(!dataset)
		return -1;

	h5s_ptr dataspace(H5Dget_space(dataset.get()));
	if(!dataspace)
		return -1;

	hsize_t offset[3] = {z, 0, 0};
	hsize_t count[3] = {1, ys, xs};
	hsize_t stride[3] = {1, 1, 1};
	hsize_t blocksize[3] = {1, 1, 1};
	if(H5Sselect_hyperslab(dataspace.get(), H5S_SELECT_SET, offset, stride, count, blocksize) < 0)
		return -1;

	hsize_t dims[] = {1, ys, xs * nchan};
	h5s_ptr memspace(H5Screate_simple(sizeof(dims) / sizeof(dims[0]), dims, nullptr));
	if(!memspace)
		return -1;

	hsize_t mem_offset[3] = {0, 0, channel};
	hsize_t mem_stride[3] = {1, 1, nchan};
	hsize_t mem_count[3] = {1, ys, xs};
	hsize_t mem_blocksize[3] = {1, 1, 1};
	if(H5Sselect_hyperslab(memspace.get(), H5S_SELECT_SET, mem_offset, mem_stride, mem_count, nullptr) < 0)
		return -1;

	if(H5Dread(dataset.get(), H5T_NATIVE_UINT16, memspace.get(), dataspace.get(), H5P_DEFAULT, data) < 0)
		return -1;

	return 0;
}

void ims::converter_hyperslab(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page, size_t maxPage)
{
	std::unique_ptr<uint16_t[]> buffer = std::make_unique<uint16_t[]>(xs * ys * nchan);

	for(size_t z = 0; z < zs; ++z)
	{
		for(size_t c = 0; c < nchan; ++c)
		{
			if(chan_read_hyperslab(timepoint, c, buffer.get(), z, xs, ys, zs, nchan) < 0)
				throw hdf5_exception();
		}

		tiff_write_page_contig(tiff, xs, ys, nchan, page, maxPage, buffer.get());
	}
}
