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
#include <array>
#include <tiffio.h>

using namespace ims;

tiff_writer::tiff_writer(TIFF *tif, size_t max_pages) :
    m_tif(tif),
    m_max_pages(max_pages),
    m_current_page(0)
{}

void tiff_writer::write_page_contig(size_t w, size_t h, size_t num_channels, uint16_t *data)
{
	constexpr size_t max_samples = 16;

	TIFFSetField(m_tif, TIFFTAG_IMAGEWIDTH, static_cast<uint32_t>(w));
	TIFFSetField(m_tif, TIFFTAG_IMAGELENGTH, static_cast<uint32_t>(h));
	TIFFSetField(m_tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	TIFFSetField(m_tif, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
	TIFFSetField(m_tif, TIFFTAG_PAGENUMBER, static_cast<uint16_t>(m_current_page + 1), static_cast<uint16_t>(m_max_pages));
	TIFFSetField(m_tif, TIFFTAG_RESOLUTIONUNIT, static_cast<uint16_t>(1));

	TIFFSetField(m_tif, TIFFTAG_PLANARCONFIG, static_cast<uint16_t>(PLANARCONFIG_CONTIG));


	TIFFSetField(m_tif, TIFFTAG_SAMPLESPERPIXEL, static_cast<uint16_t>(num_channels));
	for(size_t i = 0; i < num_channels; ++i)
	{
		TIFFSetField(m_tif, TIFFTAG_SAMPLEFORMAT, static_cast<uint16_t>(SAMPLEFORMAT_UINT));
		TIFFSetField(m_tif, TIFFTAG_BITSPERSAMPLE, static_cast<uint32_t>(16));
	}

	TIFFSetField(m_tif, TIFFTAG_PHOTOMETRIC, static_cast<uint16_t>(PHOTOMETRIC_RGB));

	/* Extra samples */
	std::array<uint16_t, max_samples> extra;
	std::fill(extra.begin(), extra.end(), EXTRASAMPLE_UNSPECIFIED);
	TIFFSetField(m_tif, TIFFTAG_EXTRASAMPLES, static_cast<uint16_t>(num_channels < 3 ? 0 : num_channels - 3), extra.data());

	for(size_t j = 0; j < h; ++j)
	{
		if(TIFFWriteScanline(m_tif, data + (w * j) * num_channels, j, 0) < 0)
			throw tiff_exception();
	}

    if(!TIFFWriteDirectory(m_tif))
		throw tiff_exception();

    ++m_current_page;
}
