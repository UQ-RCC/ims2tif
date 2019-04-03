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

#include <memory>
#include <vector>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <tiffio.h>
#include "ims2tif.hpp"

namespace fs = std::filesystem;

using namespace ims;

void ims::tiff_deleter::operator()(pointer t) noexcept { TIFFClose(t); }

struct ims_info_t
{
	size_t x;
	size_t y;
	size_t z;
	size_t c;
	size_t t;
};

static ims_info_t read_image_info(hid_t file)
{
	/* The format of these files doesn't follow the spec. Everything below is from inspecting the files manually. */

	ims_info_t imsinfo;
	h5g_ptr dsi(H5Gopen(file, "DataSetInfo", H5P_DEFAULT));
	if(!dsi)
		throw hdf5_exception();

	{
		h5g_ptr image(H5Gopen2(dsi.get(), "Image", H5P_DEFAULT));
		if(!image)
			throw hdf5_exception();

		imsinfo.x = hdf5_read_uint_attribute(image.get(), "X").value_or(0);
		imsinfo.y = hdf5_read_uint_attribute(image.get(), "Y").value_or(0);
		imsinfo.z = hdf5_read_uint_attribute(image.get(), "Z").value_or(0);

		if(imsinfo.x == 0 || imsinfo.y == 0 || imsinfo.z == 0)
			throw hdf5_exception();
	}

	{
		/*
		** Count the number of channels by counting the number of "Channel %u" groups.
		** Some files have a "CustomData/NumberOfChannels" attribute, but I can't rely on this.
		*/
		imsinfo.c = 0;
		H5Literate(dsi.get(), H5_INDEX_NAME, H5_ITER_NATIVE, nullptr, [](hid_t id, const char *name, const H5L_info_t *info, void *data) {
			ims_info_t *ims = reinterpret_cast<ims_info_t*>(data);

			uint32_t c;
			if(sscanf(name, "Channel %u\n", &c) == 1)
				++ims->c;

			return 0;
		}, &imsinfo);
	}

	{
		h5g_ptr ti(H5Gopen2(dsi.get(), "TimeInfo", H5P_DEFAULT));
		if(!ti)
			throw hdf5_exception();

		imsinfo.t = hdf5_read_uint_attribute(ti.get(), "FileTimePoints").value_or(0);
		if(imsinfo.t == 0)
			throw hdf5_exception();
	}

	return imsinfo;
}

static size_t get_num_digits(size_t num) noexcept
{
	size_t c = 0;
	for(; num > 0; ++c)
		num /= 10;
	return c;
}

static std::vector<fs::path> build_output_paths(const char *prefix, const fs::path& outdir, size_t nfiles)
{
	std::vector<fs::path> paths(nfiles);

	size_t ndigits = get_num_digits(nfiles);
	std::stringstream ss;
	for(size_t i = 0; i < nfiles; ++i)
	{
		ss.str("");
		ss << prefix << std::right << std::setfill('0') << std::setw(static_cast<int>(ndigits)) << i << ".tif";
		paths[i] = outdir / fs::u8path(ss.str());
	}

	return paths;
}

/*
 * Hack around Windows being "special" and HDF5 not playing nice.
 * On the systems we're using there'll never be non-ascii characters, so this will work.
 */
static hid_t xH5Fopen(const fs::path& path, unsigned flags, hid_t access_plist)
{
#if defined(_WIN32)
	return H5Fopen(path.u8string().c_str(), flags, access_plist);
#else
	return H5Fopen(path.c_str(), flags, access_plist);
#endif
}

static TIFF *xTIFFOpen(const fs::path& path, const char *m) noexcept
{
#if defined(_WIN32)
	return TIFFOpenW(path.c_str(), m);
#else
	return TIFFOpen(path.c_str(), m);
#endif
}

int main(int argc, char **argv)
{
	args_t args;
	int aret = parse_arguments(argc, argv, stdout, stderr, &args);
	if(aret != 0)
		return aret;

	convert_proc conv = nullptr;
	if(args.method == conversion_method_t::bigload)
		conv = converter_bigload;
	else if(args.method == conversion_method_t::chunked)
		conv = converter_chunk;
	else if(args.method == conversion_method_t::hyperslab)
		conv = converter_hyperslab;
	else
		std::terminate(); /* Will never happen. */

	h5f_ptr file(xH5Fopen(args.file, H5F_ACC_RDONLY, H5P_DEFAULT));
	if(!file)
		return 1;

	ims_info_t imsinfo = read_image_info(file.get());

	h5g_ptr ds(H5Gopen2(file.get(), "DataSet", H5P_DEFAULT));
	if(!ds)
		return 1;

	h5g_ptr rlevel(H5Gopen2(ds.get(), "ResolutionLevel 0", H5P_DEFAULT));
	if(!rlevel)
		return 1;

	/* Create the output directory if it doesn't exist. */
	std::error_code ec;
	fs::create_directories(args.outdir, ec);
	if(ec)
	{
		fprintf(stderr, "Error creating output directory: %s\n", ec.message().c_str());
		return 1;
	}

	std::vector<fs::path> paths = build_output_paths(args.prefix.c_str(), args.outdir, imsinfo.t);

	for(size_t i = 0; i < imsinfo.t; ++i)
	{
		/* Open the tif */
		tiff_ptr tif(xTIFFOpen(paths[i].c_str(), args.bigtiff ? "w8" : "w"));

		/* Get the timepoint */
		char tpbuf[32];
		sprintf(tpbuf, "TimePoint %zu", i);
		h5g_ptr tp(H5Gopen2(rlevel.get(), tpbuf, H5P_DEFAULT));
		if(!tp)
			return 1;

		conv(tif.get(), tp.get(), imsinfo.x, imsinfo.y, imsinfo.z, imsinfo.c);
	}

	return 0;
}
