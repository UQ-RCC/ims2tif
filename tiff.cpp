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

static void write_thumbnail_rgba8888(TIFF *tif, size_t width, size_t height, uint8_t *data)
{
	TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, static_cast<uint32_t>(width));
	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, static_cast<uint32_t>(height));
	TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	TIFFSetField(tif, TIFFTAG_SUBFILETYPE, FILETYPE_REDUCEDIMAGE);
	TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, 1);

	TIFFSetField(tif, TIFFTAG_PLANARCONFIG, static_cast<uint16_t>(PLANARCONFIG_CONTIG));

	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
	TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, static_cast<uint16_t>(SAMPLEFORMAT_UINT));
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, static_cast<uint32_t>(8));

	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, static_cast<uint16_t>(PHOTOMETRIC_RGB));

	uint16_t s[] = {EXTRASAMPLE_ASSOCALPHA};
	TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, 1, s);

	for(size_t j = 0; j < height; ++j)
	{
		if(TIFFWriteScanline(tif, data + (width * j) * 4, j, 0) < 0)
			throw tiff_exception();
	}

	if(!TIFFWriteDirectory(tif))
		throw tiff_exception();
}

tiff_writer::tiff_writer(TIFF *tif, size_t max_pages) :
    m_tif(tif),
    m_max_pages(max_pages),
    m_thumb(nullptr),
    m_thumb_size(0),
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

    /*
    ** If we have a thumbnail, make it of the first image.
    ** https://www.asmail.be/msg0055226761.html
    ** > You can write a chain of IFDs the normal way, and whenever you write a
    ** > value n to the SubIFDs tag, the next n directories you write are appended as a
    ** > SubIFD chain instead of being appended in the normal main IFD chain.
    */
    if(m_current_page == 0 && m_thumb != nullptr)
    {
        uint64_t offset = 0;
		TIFFSetField(m_tif, TIFFTAG_SUBIFD, static_cast<uint16_t>(1), &offset);
    }

    if(!TIFFWriteDirectory(m_tif))
		throw tiff_exception();

    if(m_current_page == 0 && m_thumb != nullptr)
		write_thumbnail_rgba8888(m_tif, m_thumb_size, m_thumb_size, m_thumb);

    ++m_current_page;
}

void tiff_writer::set_thumbnail_rgba8888(uint8_t *p, size_t size)
{
    m_thumb = p;
    m_thumb_size = size;
}