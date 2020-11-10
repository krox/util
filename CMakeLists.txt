cmake_minimum_required (VERSION 3.9.0)
project (util)

include_directories(src)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(HDF5 REQUIRED)

file(GLOB files_h "src/util/*.h")

add_library(util src/util/gnuplot.cpp src/util/hash.cpp src/util/numerics.cpp src/util/sampler.cpp src/util/stats.cpp)
target_link_libraries(util fmt)

add_library(util_hdf5 src/util/hdf5.cpp)
target_include_directories(util_hdf5 PRIVATE ${HDF5_INCLUDE_DIRS})
target_link_libraries(util_hdf5 ${HDF5_LIBRARIES})
target_link_libraries(util_hdf5 util)

install(TARGETS util util_hdf5 DESTINATION lib)
install(FILES ${files_h} DESTINATION include)
