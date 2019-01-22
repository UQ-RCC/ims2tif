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

#include <cstring>
#include <array>
#include <tiffio.h>
#include "ims2tif.hpp"

std::optional<std::string> ims::hdf5_read_attribute(hid_t id, const char *name) noexcept
{
	h5a_ptr att(H5Aopen_by_name(id, ".", name, H5P_DEFAULT, H5P_DEFAULT));
	if(!att)
		return std::optional<std::string>();

	h5s_ptr attsize(H5Aget_space(att.get()));
	if(!attsize)
		return std::optional<std::string>();

	hsize_t size;
	if(H5Sget_simple_extent_dims(attsize.get(), &size, nullptr) < 0)
		return std::optional<std::string>();

	std::string s(size, '\0');
	if(H5Aread(att.get(), H5T_C_S1, &s[0]) < 0)
		return std::optional<std::string>();

	return s;
}

std::optional<size_t> ims::hdf5_read_uint_attribute(hid_t id, const char *name) noexcept
{
	std::optional<std::string> s = hdf5_read_attribute(id, name);
	if(!s)
		return std::optional<size_t>();

	size_t v;
	if(sscanf(s->c_str(), "%zu", &v) != 1)
		return std::optional<size_t>();

	return v;
}

int ims::read_channel(hid_t tp, size_t channel, uint16_t *data, size_t xs, size_t ys, size_t zs) noexcept
{
	char cbuf[32];
	sprintf(cbuf, "Channel %zu", channel);
	h5g_ptr chan(H5Gopen2(tp, cbuf, H5P_DEFAULT));
	if(!chan)
		return -1;

	h5d_ptr d(H5Dopen2(chan.get(), "Data", H5P_DEFAULT));
	if(!d)
		return -1;

	h5s_ptr dataspace(H5Dget_space(d.get()));
	if(!dataspace)
		return -1;

	/* Sometimes if the dataset isn't POT, it's padded up to the next POT. Account for this. */
	hsize_t offset[3] = {0, 0, 0};
	hsize_t count[3] = {zs, ys, xs};
	hsize_t stride[3] = {1, 1, 1};
	hsize_t blocksize[3] = {1, 1, 1};
	if(H5Sselect_hyperslab(dataspace.get(), H5S_SELECT_SET, offset, stride, count, blocksize) < 0)
		return -1;

	if(H5Dread(d.get(), H5T_NATIVE_UINT16, H5S_ALL, dataspace.get(), H5P_DEFAULT, data) < 0)
		return -1;

	return 0;
}

void ims::tiff_write_page_contig(TIFF *tiff, size_t w, size_t h, size_t num_channels, size_t page, size_t maxPage, uint16_t *data)
{
	TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, static_cast<uint32_t>(w));
	TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, static_cast<uint32_t>(h));
	TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	TIFFSetField(tiff, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
	TIFFSetField(tiff, TIFFTAG_PAGENUMBER, static_cast<uint16_t>(page), static_cast<uint16_t>(maxPage));
	TIFFSetField(tiff, TIFFTAG_RESOLUTIONUNIT, static_cast<uint16_t>(1));

	TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, static_cast<uint16_t>(PLANARCONFIG_CONTIG));


	TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, static_cast<uint16_t>(num_channels));
	for(size_t i = 0; i < num_channels; ++i)
	{
		TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, static_cast<uint16_t>(SAMPLEFORMAT_UINT));
		TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, static_cast<uint32_t>(16));
	}

	TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, static_cast<uint16_t>(PHOTOMETRIC_RGB));

	/* Extra samples */
	if(num_channels > 3)
	{
		uint16_t extra[2] = {EXTRASAMPLE_UNSPECIFIED, EXTRASAMPLE_UNSPECIFIED};
		TIFFSetField(tiff, TIFFTAG_EXTRASAMPLES, num_channels - 3, extra);
	}

	for(size_t j = 0; j < h; ++j)
	{
		if(TIFFWriteScanline(tiff, data + (w * j) * num_channels, j, 0) < 0)
			throw tiff_exception();
	}

    if(!TIFFWriteDirectory(tiff))
		throw tiff_exception();
}