cmake_minimum_required(VERSION 3.10)
project(ims2tif)

enable_language(C CXX)

find_package(TIFF 4.0.9 REQUIRED)
set(HDF5_PREFER_PARALLEL FALSE)
find_package(HDF5 REQUIRED COMPONENTS C)
find_package(OpenCL REQUIRED)

add_executable(ims2tif
	ims2tif.cpp
	ims2tif.hpp
	args.cpp
	ims.cpp

	cvt_hyperslab.cpp
	cvt_bigload.cpp
	cvt_chunk.cpp
	cvt_opencl.cpp

	parg/parg.c
	parg/parg.h
)

set_property(TARGET ims2tif PROPERTY C_STANDARD 11)
set_property(TARGET ims2tif PROPERTY CXX_STANDARD 17)
set_property(TARGET ims2tif PROPERTY CXX_STANDARD_REQUIRED ON)

target_include_directories(ims2tif PRIVATE ${HDF5_INCLUDE_DIRS})
target_link_libraries(ims2tif PRIVATE ${HDF5_LIBRARIES})
target_link_libraries(ims2tif PRIVATE TIFF::TIFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	target_link_libraries(ims2tif PRIVATE stdc++fs)
endif()

target_link_libraries(ims2tif PRIVATE OpenCL::OpenCL)
