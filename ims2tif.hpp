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

enum class conversion_method_t { bigload, chunked, hyperslab, opencl };

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

using convert_proc = void(*)(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page, size_t maxPage);

/* cvt_bigload.cpp */
void converter_bigload(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page, size_t maxPage);

/* cvt_chunk.cpp */
void converter_chunk(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page, size_t maxPage);

/* cvt_hyperslab.cpp */
void converter_hyperslab(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page, size_t maxPage);

/* cvt_opencl.cpp */
void converter_opencl(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page, size_t maxPage);

}

#endif /* _IMS2TIF_HPP */