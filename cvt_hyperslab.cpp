/*
Dragonfly IMS to TiFF converter
https://github.com/UQ-RCC/ims2tif

SPDX-License-Identifier: Apache-2.0
Copyright (c) 2019 The University of Queensland

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
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
