#include <cstring>
#include "ims2tif.hpp"

std::unique_ptr<uint8_t[]> ims::read_thumbnail(hid_t fid, size_t& size)
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

std::string ims::read_attribute(hid_t id, const char *name)
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

size_t ims::read_uint_attribute(hid_t id, const char *name)
{
	std::string s = read_attribute(id, name);

	size_t v;
	if(sscanf(s.c_str(), "%zu", &v) != 1)
		;

	return v;
}