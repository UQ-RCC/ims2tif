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

void ims::converter_bigload(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan)
{
	const size_t chansize = xs * ys * zs;
	const size_t bufsize = xs * ys * zs * nchan;

	/* Need 2 buffers. */
	std::unique_ptr<uint16_t[]> buffer = std::make_unique<uint16_t[]>(bufsize * 2);

	uint16_t *imgbuf = buffer.get();
	uint16_t *contigbuf = buffer.get() + bufsize;

	/* Read the channel data. It's planar, so we have to read the entire timepoint. */
	for(size_t c = 0; c < nchan; ++c)
	{
		uint16_t *chanstart = imgbuf + (chansize * c);
		if(read_channel(timepoint, c, chanstart, xs, ys, zs) < 0)
			throw hdf5_exception();
	}

	planar_to_contig(imgbuf, xs, ys, zs, nchan, contigbuf);

	for(size_t z = 0; z < zs; ++z)
	{
		uint16_t *imgstart = contigbuf + (z * (xs * ys * nchan));
		tiff_write_page_contig(tiff, xs, ys, nchan, z, zs, imgstart);
	}
}
