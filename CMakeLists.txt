cmake_minimum_required (VERSION 3.9.0)
project (util LANGUAGES C CXX)

# dependencies
find_package(HDF5 REQUIRED)

add_subdirectory(fmt EXCLUDE_FROM_ALL)

# main library
add_library(util src/util/gnuplot.cpp src/util/hash.cpp src/util/numerics.cpp src/util/sampler.cpp src/util/stats.cpp)
target_compile_features(util PUBLIC cxx_std_17)
target_include_directories(util PUBLIC src)
target_link_libraries(util PUBLIC fmt::fmt)

# optional hdf5 library
add_library(util_hdf5 src/util/hdf5.cpp)
target_include_directories(util_hdf5 PRIVATE ${HDF5_INCLUDE_DIRS})
target_link_libraries(util_hdf5 ${HDF5_LIBRARIES})
target_link_libraries(util_hdf5 util)

# install
file(GLOB files_h "src/util/*.h")
install(TARGETS util util_hdf5 DESTINATION lib)
install(FILES ${files_h} DESTINATION include)

# unittests
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	include(CTest)
	include(FetchContent)
	FetchContent_Declare(catch GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v2.13.7)
	FetchContent_MakeAvailable(catch)

	add_executable(testutil tests/testutil.cpp)
	target_compile_features(testutil PRIVATE cxx_std_17)
	target_include_directories(testutil PUBLIC src)
	target_link_libraries(testutil PRIVATE util Catch2::Catch2)
	add_test(NAME testlibtest COMMAND testutil) # Command can be a target

endif()
