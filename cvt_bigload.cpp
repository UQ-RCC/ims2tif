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

static int chan_read_bigload(hid_t tp, size_t channel, uint16_t *data, size_t z, size_t xs, size_t ys, size_t zs, hsize_t nchan)
{
	char cbuf[32];
	sprintf(cbuf, "Channel %zu", channel);
	h5g_ptr chan(H5Gopen2(tp, cbuf, H5P_DEFAULT));
	if(!chan)
		return -1;

	h5d_ptr d(H5Dopen2(chan.get(), "Data", H5P_DEFAULT));
	if(!d)
		return -1;

	if(H5Dread(d.get(), H5T_NATIVE_UINT16, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0)
		return -1;

	return 0;
}

/* Hard-coded RGB planar-to-contig. Handy for debugging the TIFF writing code. */
static void planar_to_contig_3chan(uint16_t *planar, size_t xs, size_t ys, size_t zs, size_t num_channels, uint16_t *contig)
{
	if(num_channels != 3)
		throw std::runtime_error("wut");

	size_t chansize = xs * ys * zs;

	uint16_t *rroot = planar + (0 * chansize); /* red root */
	uint16_t *groot = planar + (1 * chansize); /* green root */
	uint16_t *broot = planar + (2 * chansize); /* blue root */

	size_t imgsize = xs * ys * num_channels;
	for(size_t z = 0; z < zs; ++z)
	{
		uint16_t *rstart = rroot + (z * xs * ys);
		uint16_t *gstart = groot + (z * xs * ys);
		uint16_t *bstart = broot + (z * xs * ys);

		uint16_t *imgstart = contig + (z * imgsize);

		for(size_t i = 0; i < xs * ys * 3; i += 3)
		{
			imgstart[i + 0] = *rstart++;
			imgstart[i + 1] = *gstart++;
			imgstart[i + 2] = *bstart++;
		}
	}
}

/* TODO: Optimise this. Or just do it on a GPU. */
static void planar_to_contig(uint16_t *planar, size_t xs, size_t ys, size_t zs, size_t num_channels, uint16_t *contig)
{
	size_t chansize = xs * ys * zs;
	size_t imgsize = xs * ys * num_channels;

	for(size_t z = 0; z < zs; ++z)
	{
		uint16_t *imgstart = contig + (z * imgsize);

		for(size_t i = 0; i < xs * ys; ++i)
		{
			for(size_t c = 0; c < num_channels; ++c)
			{
				uint16_t *croot = planar + (c * chansize);
				uint16_t *cstart = croot + (z * xs * ys) + i;
				*imgstart++ = *cstart;
			}
		}
	}
}

void ims::converter_bigload(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page, size_t maxPage)
{
	const size_t chansize = xs * ys * zs;
	const size_t bufsize = xs * ys * zs * nchan;

	/* Need 2 buffers. */
	std::unique_ptr<uint16_t[]> buffer = std::make_unique<uint16_t[]>(bufsize * 2);

	uint16_t *imgbuf = buffer.get();
	uint16_t *contigbuf = buffer.get() + bufsize;

	for(size_t z = 0; z < zs; ++z)
	{
		/* Read the channel data. It's planar, so we have to read the entire timepoint. */
		for(size_t c = 0; c < nchan; ++c)
		{
			uint16_t *chanstart = imgbuf + (chansize * c);
			if(chan_read_bigload(timepoint, c, chanstart, z, xs, ys, zs, nchan) < 0)
				throw hdf5_exception();
		}

		planar_to_contig(imgbuf, xs, ys, zs, nchan, contigbuf);
	}

	for(size_t z = 0; z < zs; ++z)
	{
		uint16_t *imgstart = contigbuf + (z * (xs * ys * nchan));
		tiff_write_page_contig(tiff, xs, ys, nchan, page, maxPage, imgstart);
	}
}
