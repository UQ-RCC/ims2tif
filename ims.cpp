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