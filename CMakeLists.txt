cmake_minimum_required (VERSION 3.16.0)
project (util LANGUAGES C CXX)

# dependencies

find_package(HDF5 REQUIRED) # should be optional

include(FetchContent)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt
  GIT_TAG        c4ee726532178e556d923372f29163bd206d7732 # v9.0.0
)
FetchContent_Declare(
	blake3
	# This is fork of the official BLAKE3 repo that includes CMake support.
	# Should switch to the official one if this gets merged:
	# https://github.com/BLAKE3-team/BLAKE3/pull/247
	GIT_REPOSITORY https://github.com/SteveGremory/BLAKE3
	GIT_TAG        a8914f5ab88a84007f8c94ae4f66ac074b158f9c
)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
  GIT_TAG        b9be5b9444772324459989177108a6a65b8b2769 # v2.2.0
)

FetchContent_MakeAvailable(fmt blake3 cli11)

add_subdirectory(${blake3_SOURCE_DIR}/c ${blake3_BINARY_DIR})

# main library

add_library(util src/util/gnuplot.cpp src/util/hash.cpp src/util/memory.cpp src/util/numerics.cpp src/util/numpy.cpp src/util/sampler.cpp src/util/stats.cpp)
target_compile_features(util PUBLIC cxx_std_20)
set_target_properties(util PROPERTIES CXX_EXTENSIONS ON)
target_include_directories(util PUBLIC src)
target_link_libraries(util PUBLIC fmt::fmt blake3-shared)
target_compile_options(util PRIVATE -Wall -Wextra -Werror -O2)

# optional hdf5 library
add_library(util_hdf5 src/util/hdf5.cpp)
target_include_directories(util_hdf5 PRIVATE ${HDF5_INCLUDE_DIRS})
target_link_libraries(util_hdf5 ${HDF5_LIBRARIES})
target_link_libraries(util_hdf5 util)

# executables
foreach(bin "json-format")
	add_executable(${bin} src/apps/${bin}.cpp)
	target_compile_features(${bin} PUBLIC cxx_std_20)
	target_link_libraries(${bin} util CLI11::CLI11)
endforeach(bin)

# install
file(GLOB files_h "src/util/*.h")
install(TARGETS util util_hdf5 DESTINATION lib)
install(FILES ${files_h} DESTINATION include)

# unittests
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	include(FetchContent)

	FetchContent_Declare(
	  Catch2
	  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
	  GIT_TAG        v3.0.0-preview4
	)

	FetchContent_MakeAvailable(Catch2)

	add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
	add_link_options(-fsanitize=address)

	add_executable(tests tests/bits.cpp tests/hash_map.cpp tests/hash.cpp tests/iterator.cpp tests/json.cpp tests/memory.cpp tests/numerics.cpp tests/random.cpp tests/threadpool.cpp tests/vector.cpp)
	target_compile_features(tests PRIVATE cxx_std_20)
	target_link_libraries(tests PRIVATE Catch2::Catch2WithMain util)
	target_compile_options(tests PRIVATE -Wall -Wextra -Werror -g -Wno-parentheses)

endif()
