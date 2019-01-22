#include <cassert>
#include <type_traits>
#include <CL/cl.h>
#include "ims2tif.hpp"

using namespace ims;

struct cl_context_deleter
{
	using pointer = cl_context;
	void operator()(pointer p) noexcept { clReleaseContext(p); }
};
using cl_context_ptr = std::unique_ptr<std::remove_pointer_t<cl_context>, cl_context_deleter>;

struct cl_mem_deleter
{
	using pointer = cl_mem;
	void operator()(pointer p) noexcept { clReleaseMemObject(p); }
};
using cl_mem_ptr = std::unique_ptr<std::remove_pointer_t<cl_mem>, cl_mem_deleter>;

struct cl_command_queue_deleter
{
	using pointer = cl_command_queue;
	void operator()(pointer p) noexcept { clReleaseCommandQueue(p); }
};
using cl_command_queue_ptr = std::unique_ptr<std::remove_pointer_t<cl_command_queue>, cl_command_queue_deleter>;

struct cl_program_deleter
{
	using pointer = cl_program;
	void operator()(pointer p) noexcept { clReleaseProgram(p); }
};
using cl_program_ptr = std::unique_ptr<std::remove_pointer_t<cl_program>, cl_program_deleter>;

struct cl_kernel_deleter
{
	using pointer = cl_kernel;
	void operator()(pointer p) noexcept { clReleaseKernel(p); }
};
using cl_kernel_ptr = std::unique_ptr<std::remove_pointer_t<cl_kernel>, cl_kernel_deleter>;

static void CL_CALLBACK err_proc(const char *errinfo, const void *private_info, size_t cb, void *user)
{
	fprintf(stderr, "%s\n", errinfo);
}

struct clstate
{
	size_t xs, ys, zs;
	size_t nchan;
	hid_t timepoint;

	cl_context_ptr  ctx;
	cl_command_queue_ptr dev_queue;

	constexpr static size_t max_channels = 5;
	std::array<cl_mem_ptr, max_channels> channel_buffers;
	std::array<std::unique_ptr<uint16_t[]>, max_channels> host_channel_buffers;
	std::array<cl_event, max_channels> channel_events;

	cl_mem_ptr output_buffer;
	std::array<cl_event, max_channels> interleave_events;
};

static const char *kernel =
"__global unsigned short *get_pixel(__global unsigned short *data, uint4 coord, uint4 dims)\n"
"{\n"
"	const size_t pixsize = dims.w;\n"
"	const size_t rowsize = dims.x * pixsize;\n"
"	const size_t slicesize = dims.y * rowsize;\n"
"\n"
"	return data + (coord.z * slicesize) + (coord.y * rowsize) + (coord.x * pixsize) + coord.w;\n"
"}\n"
"\n"
"__kernel void interleave(__global unsigned short *contig, __global unsigned short *chan, unsigned int c, uint4 dims)\n"
"{\n"
"	uint4 coords = {get_global_id(0), get_global_id(1), get_global_id(2), c};\n"
""
"	uint4 ccoords = coords;\n"
"	ccoords.w = 0;\n"
"	uint4 cdims = dims;\n"
"	cdims.w = 1;\n"
"\n"
"	*get_pixel(contig, coords, dims) = *get_pixel(chan, ccoords, cdims);\n"
"}\n";

template <typename T>
cl_int clSetKernelArg2(cl_kernel kern, cl_uint arg_index, T val)
{
	return clSetKernelArg(kern, arg_index, sizeof(T), &val);
}

#include <cstring>
static cl_kernel_ptr build_kernel(cl_context ctx, cl_device_id dev, cl_int *err) noexcept
{
	*err = CL_SUCCESS;

	size_t len = 0;
	cl_program_ptr pg(clCreateProgramWithSource(ctx, 1, &kernel, &len, err));
	if(!pg)
		return nullptr;

	if(clBuildProgram(pg.get(), 1, &dev, "-cl-std=CL1.2", nullptr, nullptr) != CL_SUCCESS)
	{
		char log[2048];
		memset(log, 0, sizeof(log));
		clGetProgramBuildInfo(pg.get(), dev, CL_PROGRAM_BUILD_LOG, sizeof(log) - 1, log, nullptr);
		fprintf(stderr, "%s\n", log);
		return nullptr;
	}

	cl_kernel_ptr kern(clCreateKernel(pg.get(), "interleave", err));
	if(!kern)
		return nullptr;

	return kern;
}

static void fill_images(clstate& s)
{
	s.channel_events.fill(nullptr);

	for(size_t c = 0; c < s.nchan; ++c)
	{
		if(read_channel(s.timepoint, c, s.host_channel_buffers[c].get(), s.xs, s.ys, s.zs) < 0)
			throw hdf5_exception();
		cl_int err;

		err = clEnqueueWriteBuffer(
			s.dev_queue.get(),
			s.channel_buffers[c].get(),
			CL_FALSE,
			0,
			s.xs * s.ys * s.zs * sizeof(uint16_t),
			s.host_channel_buffers[c].get(),
			0,
			nullptr,
			s.channel_events.data() + c
		);
		assert(err == CL_SUCCESS);
	}
}

#define USE_OPENCL_1_2

void ims::converter_opencl(TIFF *tiff, hid_t timepoint, size_t xs, size_t ys, size_t zs, size_t nchan, size_t page, size_t maxPage)
{
	cl_uint platformIdCount = 0;
	clGetPlatformIDs(0, nullptr, &platformIdCount);

	std::vector<cl_platform_id> platformIds(platformIdCount);
	clGetPlatformIDs(platformIdCount, platformIds.data(), nullptr);

	cl_uint deviceIdCount = 0;
	clGetDeviceIDs(platformIds[0], CL_DEVICE_TYPE_ALL, 0, nullptr, &deviceIdCount);
	std::vector<cl_device_id> deviceIds(deviceIdCount);
	clGetDeviceIDs(platformIds[0], CL_DEVICE_TYPE_ALL, deviceIdCount, deviceIds.data(), nullptr);

	const cl_context_properties contextProperties[] =
	{
		CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties> (platformIds[0]),
		0, 0
	};

	cl_int error;

	clstate state;
	state.xs = xs;
	state.ys = ys;
	state.zs = zs;
	state.nchan = nchan;
	state.timepoint = timepoint;

	state.ctx.reset(clCreateContext(contextProperties, deviceIdCount, deviceIds.data(), err_proc, nullptr, &error));
	if(!state.ctx)
		throw std::exception();

#ifdef USE_OPENCL_1_2
	/* Wiener only has OpenCL 1.2 */
	state.dev_queue.reset(clCreateCommandQueue(state.ctx.get(), deviceIds[0], 0, &error));
#else
	const cl_queue_properties queueprops[] = {
		CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT,
		CL_QUEUE_SIZE,  CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE,
		0
	};

	state.dev_queue.reset(clCreateCommandQueueWithProperties(state.ctx.get(), deviceIds[0], nullptr, &error));
#endif

	assert(error == CL_SUCCESS);

	if(nchan > clstate::max_channels)
		std::terminate();

	/* Allocate an image-per-channel. */
	for(size_t i = 0; i < nchan; ++i)
	{
		state.host_channel_buffers[i] = std::make_unique<uint16_t[]>(xs * ys * zs);
		state.channel_buffers[i].reset(clCreateBuffer(
			state.ctx.get(),
			CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
			xs * ys * zs * sizeof(uint16_t),
			nullptr,
			&error
		));
		assert(state.channel_buffers[i]);
		assert(error == CL_SUCCESS);
	}

	state.output_buffer.reset(clCreateBuffer(
		state.ctx.get(),
		CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY,
		xs * ys * zs * sizeof(uint16_t) * nchan,
		nullptr,
		&error
	));
	assert(state.output_buffer);
	assert(error == CL_SUCCESS);

	cl_kernel_ptr kern = build_kernel(state.ctx.get(), deviceIds[0], &error);
	assert(kern);

	state.interleave_events.fill(nullptr);
	fill_images(state);

	//clWaitForEvents((cl_uint)nchan, state.channel_events.data());

	size_t global_work_size[3] = {state.xs, state.ys, state.zs};

	std::unique_ptr<uint16_t[]> contigbuf = std::make_unique<uint16_t[]>(xs * ys * zs * nchan);

	cl_uint4 dims = {(cl_uint)xs, (cl_uint)ys, (cl_uint)zs, (cl_uint)nchan};

	for(size_t i = 0; i < nchan; ++i)
	{
		error = clSetKernelArg2<cl_mem>(kern.get(), 0, state.output_buffer.get());
		assert(error == CL_SUCCESS);
		error = clSetKernelArg2<cl_mem>(kern.get(), 1, state.channel_buffers[i].get());
		assert(error == CL_SUCCESS);
		error = clSetKernelArg2<cl_uint>(kern.get(), 2, static_cast<cl_uint>(i));
		assert(error == CL_SUCCESS);
		error = clSetKernelArg2<cl_uint4>(kern.get(), 3, dims);
		assert(error == CL_SUCCESS);

		error = clEnqueueNDRangeKernel(
			state.dev_queue.get(),
			kern.get(),
			static_cast<cl_uint>(std::extent_v<decltype(global_work_size)>),
			nullptr,
			global_work_size,
			nullptr,
			1,
			state.channel_events.data() + i,
			state.interleave_events.data() + i
		);
		assert(error == CL_SUCCESS);
	}

	error = clEnqueueReadBuffer(
		state.dev_queue.get(),
		state.output_buffer.get(),
		CL_TRUE,
		0,
		xs * ys * zs * nchan * sizeof(uint16_t),
		contigbuf.get(),
		static_cast<cl_uint>(state.nchan),
		state.interleave_events.data(),
		nullptr
	);
	assert(error == CL_SUCCESS);

	for(size_t z = 0; z < zs; ++z)
	{
		uint16_t *imgstart = contigbuf.get() + (z * (xs * ys * nchan));
		tiff_write_page_contig(tiff, xs, ys, nchan, page, maxPage, imgstart);
	}
}

