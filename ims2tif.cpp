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

static std::string read_attribute(hid_t id, const char *name)
{
	h5a_ptr att(H5Aopen_by_name(id, ".", name, H5P_DEFAULT, H5P_DEFAULT));
	if(!att)
		throw hdf5_exception();

	h5s_ptr attsize(H5Aget_space(att.get()));
	if(!attsize)
		throw hdf5_exception();

	hsize_t size;
	if(H5Sget_simple_extent_dims(attsize.get(), &size, nullptr) < 0)
		throw hdf5_exception();

	std::string s(size, '\0');
	if(H5Aread(att.get(), H5T_C_S1, &s[0]) < 0)
		throw hdf5_exception();

	return s;
}

static size_t read_uint_attribute(hid_t id, const char *name)
{
	std::string s = read_attribute(id, name);

	size_t v;
	if(sscanf(s.c_str(), "%zu", &v) != 1)
		;

	return v;
}

static struct timespec parse_timestamp(const char *s)
{
	/*
	 * Timestamps are of the format: 2018-05-24 10:38:17.794
	 * FIXME: What do I do about timezones?
	 */
	struct tm tm;
	uint32_t msec;
	memset(&tm, 0, sizeof(tm));
	if(sscanf(s, "%4u-%2u-%2u %2u:%2u:%2u.%3u",
		&tm.tm_year, &tm.tm_mon, &tm.tm_mday,
		&tm.tm_hour, &tm.tm_min, &tm.tm_sec,
		&msec
	) != 7)
		throw std::range_error("invalid timestamp");

	struct timespec ts;
	ts.tv_sec = mktime(&tm);
	ts.tv_nsec = msec * 1000000;

	if(ts.tv_sec == static_cast<time_t>(-1))
		throw std::range_error("invalid timestamp");

	return ts;
}

struct ims_info_t
{
	size_t channel_count;
	size_t x_size;
	size_t y_size;
	size_t z_size;

	std::vector<struct timespec> time_points;
};

static ims_info_t read_image_info(hid_t file)
{
	/* The format of these files doesn't follow the spec. Everything below is from inspecting the files manually. */

	ims_info_t imsinfo;
	h5g_ptr dsi(H5Gopen(file, "DataSetInfo", H5P_DEFAULT));

	{
		h5g_ptr image(H5Gopen2(dsi.get(), "Image", H5P_DEFAULT));
		imsinfo.x_size = read_uint_attribute(image.get(), "X");
		imsinfo.y_size = read_uint_attribute(image.get(), "Y");
		imsinfo.z_size = read_uint_attribute(image.get(), "Z");
	}

	{
		/* Is a struct in case I need any other info in the future. */
		struct it_data
		{
			size_t count;
		} tmp = {
			0
		};

		/* Count the number of channels by counting the number of "Channel %u" groups. */
		H5Literate(dsi.get(), H5_INDEX_NAME, H5_ITER_NATIVE, nullptr,[](hid_t id, const char *name, const H5L_info_t *info, void *data) {
			it_data *it = reinterpret_cast<it_data*>(data);

			uint32_t c;
			if(sscanf(name, "Channel %u\n", &c) == 1)
				++it->count;

			return 0;
		}, &tmp);

		imsinfo.channel_count = tmp.count;
	}

	/* Not all files have this unfortunately. */
#if 0
	{
		h5g_ptr cd(H5Gopen2(dsi.get(), "CustomData", H5P_DEFAULT));
		imsinfo.channel_count = read_uint_attribute(cd.get(), "NumberOfChannels");
	}
#endif
	{
		h5g_ptr ti(H5Gopen2(dsi.get(), "TimeInfo", H5P_DEFAULT));
		size_t num_time_points = read_uint_attribute(ti.get(), "FileTimePoints");

		imsinfo.time_points.reserve(num_time_points);
		for(uint32_t i = 0; i < num_time_points; ++i)
		{
			char tpbuf[32]; /* Enough for "TimePoint%u" */
			sprintf(tpbuf, "TimePoint%u", i + 1);
			std::string ts = read_attribute(ti.get(), tpbuf);
			imsinfo.time_points.push_back(parse_timestamp(ts.c_str()));
		}
	}

	return imsinfo;
}

/* Read a square RGBA8888 thumbnail. */
static std::unique_ptr<uint8_t[]> read_thumbnail(hid_t fid, size_t& size)
{
	h5g_ptr thmb(H5Gopen2(fid, "Thumbnail", H5P_DEFAULT));
	if(!thmb)
		return nullptr;

	h5d_ptr d(H5Dopen2(thmb.get(), "Data", H5P_DEFAULT));
	if(!d)
		return nullptr;

	h5s_ptr s(H5Dget_space(d.get()));

	int ndims = H5Sget_simple_extent_ndims(s.get());
	if(ndims != 2)
		return nullptr;

	hsize_t dims[2];
	H5Sget_simple_extent_dims(s.get(), dims, nullptr);

	/* 512x2048 = 512 * RGBA8888 */
	if(dims[1] != 4 * dims[0])
		return nullptr;

	std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(dims[0] * dims[1]);
	if(H5Dread(d.get(), H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, data.get()) < 0)
		return nullptr;

	size = dims[0];
	return data;
}

static size_t get_num_digits(size_t num)
{
	size_t c = 0;
	for(; num > 0; ++c)
		num /= 10;
	return c;
}

static std::vector<fs::path> build_output_paths(const char *prefix, const char *outdir, size_t nfiles)
{
	std::vector<fs::path> paths(nfiles);

	size_t ndigits = get_num_digits(nfiles);
	std::stringstream ss;
	fs::path out(outdir);
	for(size_t i = 0; i < nfiles; ++i)
	{
		ss.str("");
		ss << prefix << std::right << std::setfill('0') << std::setw(ndigits) << i << ".tif";
		paths[i] = out / ss.str();
	}

	return paths;
}

int main(int argc, char **argv)
{
	args_t args;
	int aret = parse_arguments(argc, argv, stdout, stderr, &args);
	if(aret != 0)
		return aret;

	convert_proc conv = nullptr;
	if(args.method == "bigload")
		conv = converter_bigload;
	else if(args.method == "chunked")
		conv = converter_chunk;
	else if(args.method == "hyperslab")
		conv = converter_hyperslab;

	if(conv == nullptr)
	{
		fprintf(stderr, "Unknown conversion method %s.\n", args.method.c_str());
		return 2;
	}

	h5f_ptr file(H5Fopen(args.file.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT));
	if(!file)
		return 1;

	/* The format of these files doesn't follow the spec. Everything below is from inspecting the files manually. */
	ims_info_t imsinfo = read_image_info(file.get());

	size_t thumb_size = 0;
	std::unique_ptr<uint8_t[]> thumb = read_thumbnail(file.get(), thumb_size);

	h5g_ptr ds(H5Gopen2(file.get(), "DataSet", H5P_DEFAULT));
	if(!ds)
		return 1;

	h5g_ptr rlevel(H5Gopen2(ds.get(), "ResolutionLevel 0", H5P_DEFAULT));
	if(!rlevel)
		return 1;

	std::vector<fs::path> paths = build_output_paths(args.prefix.c_str(), args.outdir.c_str(), imsinfo.time_points.size());

	for(size_t i = 0; i < imsinfo.time_points.size(); ++i)
	{
		/* Open the tif */
		tiff_ptr tif(TIFFOpen(paths[i].c_str(), "w"));

		tiff_writer tiffw(tif.get(), imsinfo.z_size);
		tiffw.set_thumbnail_rgba8888(thumb.get(), thumb_size);

		/* Get the timepoint */
		char tpbuf[32];
		sprintf(tpbuf, "TimePoint %zu", i);
		h5g_ptr tp(H5Gopen2(rlevel.get(), tpbuf, H5P_DEFAULT));
		if(!tp)
			return 1;

		conv(tiffw, tp.get(), imsinfo.x_size, imsinfo.y_size, imsinfo.z_size, imsinfo.channel_count);
	}

	return 0;
}
