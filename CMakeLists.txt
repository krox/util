cmake_minimum_required(VERSION 3.16.0)
project(util LANGUAGES C CXX)

# dependencies
find_package(HDF5 REQUIRED) # should be optional

include(cmake/CPM.cmake)

CPMAddPackage(
	NAME fmt
	GITHUB_REPOSITORY fmtlib/fmt
	GIT_TAG e69e5f977d458f2650bb346dadf2ad30c5320281 # v10.2.1
)

CPMAddPackage(
	NAME blake3
	GITHUB_REPOSITORY BLAKE3-team/BLAKE3
	GIT_TAG 3f396d223946f722ab060fe9377cd1cebacaf4c0 # a little after v1.4.0
	DOWNLOAD_ONLY True
)
add_subdirectory(${blake3_SOURCE_DIR}/c ${blake3_BINARY_DIR})

CPMAddPackage(
	NAME cli11
	GITHUB_REPOSITORY CLIUtils/CLI11
	GIT_TAG 291c58789c031208f08f4f261a858b5b7083e8e2 # v2.3.2
)

# main library
add_library(util src/util/gnuplot.cpp src/util/hash.cpp src/util/io.cpp src/util/json.cpp src/util/memory.cpp src/util/numerics.cpp src/util/numpy.cpp src/util/sampler.cpp src/util/stats.cpp)
target_compile_features(util PUBLIC cxx_std_20)
set_target_properties(util PROPERTIES CXX_EXTENSIONS ON)
target_include_directories(util PUBLIC include)
target_include_directories(util PUBLIC ${blake3_SOURCE_DIR}/c)
target_link_libraries(util PUBLIC fmt::fmt BLAKE3::blake3)
target_compile_options(util PRIVATE -Wall -Wextra -Wno-deprecated-declarations -Werror -O2 -march=native)

# optional hdf5 library
add_library(util_hdf5 src/util/hdf5.cpp)
target_include_directories(util_hdf5 PRIVATE ${HDF5_INCLUDE_DIRS})
target_link_libraries(util_hdf5 ${HDF5_LIBRARIES})
target_link_libraries(util_hdf5 util)

# executables
foreach(bin "json-format" "random")
	add_executable(${bin} src/apps/${bin}.cpp)
	target_compile_features(${bin} PUBLIC cxx_std_20)
	target_compile_options(${bin} PRIVATE -Wall -Wextra -Wno-deprecated-declarations -Werror -O2 -march=native)
	target_link_libraries(${bin} util CLI11::CLI11)
endforeach(bin)

# install
file(GLOB files_h "src/include/*.h")
install(TARGETS util util_hdf5 DESTINATION lib)
install(FILES ${files_h} DESTINATION include)

# unittests
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	CPMAddPackage(
		NAME Catch2
		GITHUB_REPOSITORY catchorg/Catch2
		GIT_TAG 2ab20a0e008845e02bd06248e61ca6e5ad1aba33 # v3.3.1
	)

	add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
	add_link_options(-fsanitize=address)

	add_executable(tests tests/bits.cpp tests/complex.cpp tests/ddouble.cpp tests/error.cpp tests/hash_map.cpp tests/hash.cpp tests/iterator.cpp tests/json.cpp tests/linalg.cpp tests/memory.cpp tests/numerics.cpp tests/random.cpp tests/simd.cpp tests/string.cpp tests/threadpool.cpp tests/vector.cpp)
	target_compile_features(tests PRIVATE cxx_std_20)
	target_link_libraries(tests PRIVATE Catch2::Catch2WithMain util)
	target_compile_options(tests PUBLIC -Wall -Wextra -Werror -g -Wno-parentheses -Wno-deprecated-declarations -march=native)
endif()
