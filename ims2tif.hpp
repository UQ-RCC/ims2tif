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

#ifndef _IMS2TIF_HPP
#define _IMS2TIF_HPP

#include <ctime>
#include <memory>
#include <vector>
#include <iosfwd>
#include <filesystem>
#include <optional>
#include "hdf5.h"

using TIFF = struct tiff;

namespace ims
{

/* Errors are reported to console. */
class hdf5_exception : public std::exception
{
	using std::exception::exception;
};

class tiff_exception : public std::exception
{
	using std::exception::exception;
};

struct h5_hid
{
	h5_hid(void) : _desc(H5I_INVALID_HID) {}
	h5_hid(hid_t fd) : _desc(fd) {}
	h5_hid(std::nullptr_t) : _desc(H5I_INVALID_HID) {}

	operator hid_t() { return _desc; }

	bool operator==(const h5_hid &other) const { return _desc == other._desc; }
	bool operator!=(const h5_hid &other) const { return _desc != other._desc; }
	bool operator==(std::nullptr_t) const { return _desc == H5I_INVALID_HID; }
	bool operator!=(std::nullptr_t) const { return _desc != H5I_INVALID_HID; }

	hid_t _desc;
};

struct h5f_deleter
{
	using pointer = h5_hid;
	void operator()(pointer hid) noexcept { H5Fclose(hid); }
};
using h5f_ptr = std::unique_ptr<h5f_deleter::pointer, h5f_deleter>;

struct h5g_deleter
{
	using pointer = h5_hid;
	void operator()(pointer hid) noexcept { H5Gclose(hid); }
};
using h5g_ptr = std::unique_ptr<h5g_deleter::pointer, h5g_deleter>;

struct h5a_deleter
{
	using pointer = h5_hid;
	void operator()(pointer hid) noexcept { H5Aclose(hid); }
};
using h5a_ptr = std::unique_ptr<h5a_deleter::pointer, h5a_deleter>;

struct h5s_deleter
{
	using pointer = h5_hid;
	void operator()(pointer hid) noexcept { H5Sclose(hid); }
};
using h5s_ptr = std::unique_ptr<h5s_deleter::pointer, h5s_deleter>;

struct h5d_deleter
{
	using pointer = h5_hid;
	void operator()(pointer hid) noexcept { H5Dclose(hid); }
};
using h5d_ptr = std::unique_ptr<h5d_deleter::pointer, h5d_deleter>;

struct h5p_deleter
{
	using pointer = h5_hid;
	void operator()(pointer hid) noexcept { H5Pclose(hid); }
};
using h5p_ptr = std::unique_ptr<h5p_deleter::pointer, h5p_deleter>;

struct tiff_deleter
{
	using pointer = TIFF*;
	void operator()(pointer t) noexcept;
};
using tiff_ptr = std::unique_ptr<tiff_deleter::pointer, tiff_deleter>;

enum class conversion_method_t { bigload, chunked, hyperslab };

struct args_t
{
	args_t() noexcept;

	std::filesystem::path file;
	std::string prefix;
	std::filesystem::path outdir;
	conversion_method_t method;
	bool bigtiff;
};
/* args.cpp */
int parse_arguments(int argc, char **argv, FILE *out, FILE *err, args_t *args);

std::optional<std::string> hdf5_read_attribute(hid_t id, const char *name) noexcept;

std::optional<size_t> hdf5_read_uint_attribute(hid_t id, const char *name) noexcept;

int read_channel(hid_t tp, size_t channel, uint16_t *data, size_t xs, size_t ys, size_t zs) noexcept;

void tiff_write_page_contig(TIFF *tiff, size_t w, size_t h, size_t num_channels, size_t page, size_t maxPage, uint16_t *data);

using convert_proc = void(*)(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page);

/* cvt_bigload.cpp */
void converter_bigload(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page);

/* cvt_chunk.cpp */
void converter_chunk(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page);

/* cvt_hyperslab.cpp */
void converter_hyperslab(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page);

}

#endif /* _IMS2TIF_HPP */