#include <cstring>
#include "ims2tif.hpp"

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