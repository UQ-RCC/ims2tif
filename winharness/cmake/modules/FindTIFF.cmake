add_library(TIFF::TIFF ALIAS tiff)

target_include_directories(tiff INTERFACE "${CMAKE_SOURCE_DIR}/tiff-4.0.9/libtiff")
target_include_directories(tiff INTERFACE "${CMAKE_BINARY_DIR}/tiff-4.0.9/libtiff")
