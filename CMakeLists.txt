cmake_minimum_required(VERSION 3.16.0)
project(util LANGUAGES C CXX)

include(CheckCXXSourceCompiles)

option(UTIL_FFTW "link to FFTW3 for Fourier transforms" OFF)
option(UTIL_HDF5 "link to HDF5 for I/O" OFF)
set(UTIL_SIMD_BACKEND "AUTO" CACHE STRING "SIMD backend: AUTO, AVX, or GENERIC")
set_property(CACHE UTIL_SIMD_BACKEND PROPERTY STRINGS AUTO AVX GENERIC)
set(UTIL_SIMD_BACKEND_ALLOWED AUTO AVX GENERIC)

string(TOUPPER "${UTIL_SIMD_BACKEND}" UTIL_SIMD_BACKEND)
set(UTIL_SIMD_COMPILE_OPTIONS "")
set(UTIL_SIMD_COMPILE_DEFINITIONS UTIL_SIMD_BACKEND_GENERIC)

if(NOT UTIL_SIMD_BACKEND IN_LIST UTIL_SIMD_BACKEND_ALLOWED)
	message(FATAL_ERROR "UTIL_SIMD_BACKEND must be one of: AUTO, AVX, GENERIC")
endif()

if(UTIL_SIMD_BACKEND STREQUAL "AUTO")
	set(_util_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
	set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -mavx -mfma")
	check_cxx_source_compiles(
		"#include <x86intrin.h>
		int main()
		{
			__m256 a = _mm256_set1_ps(1.0f);
			__m256 b = _mm256_fmadd_ps(a, a, a);
			return (int)_mm_cvtss_f32(_mm256_castps256_ps128(b));
		}"
		UTIL_HAS_AVX_FMA
	)
	set(CMAKE_REQUIRED_FLAGS "${_util_saved_required_flags}")

	if(UTIL_HAS_AVX_FMA)
		set(UTIL_SIMD_BACKEND_RESOLVED "AVX")
	else()
		set(UTIL_SIMD_BACKEND_RESOLVED "GENERIC")
	endif()
else()
	set(UTIL_SIMD_BACKEND_RESOLVED "${UTIL_SIMD_BACKEND}")
endif()

if(UTIL_SIMD_BACKEND_RESOLVED STREQUAL "AVX")
	set(UTIL_SIMD_COMPILE_OPTIONS -mavx -mfma)
	set(UTIL_SIMD_COMPILE_DEFINITIONS UTIL_SIMD_BACKEND_AVX)
endif()

message(STATUS "util SIMD backend: ${UTIL_SIMD_BACKEND_RESOLVED} (requested: ${UTIL_SIMD_BACKEND})")

# dependencies
if(UTIL_HDF5)
	find_package(HDF5 REQUIRED)
endif()

include(cmake/CPM.cmake)

CPMAddPackage(
	NAME fmt
	GITHUB_REPOSITORY fmtlib/fmt
	GIT_TAG 0c9fce2ffefecfdce794e1859584e25877b7b592 # 11.0.2
	OPTIONS
	"FMT_SYSTEM_HEADERS ON" # mark fmt headers as "system", which silences warnings
)

CPMAddPackage(
	NAME blake3
	GITHUB_REPOSITORY BLAKE3-team/BLAKE3
	GIT_TAG 95e42b84fc4709974c7b23c7ae885989ab36c31e # 1.5.4
	DOWNLOAD_ONLY True
)
add_subdirectory(${blake3_SOURCE_DIR}/c ${blake3_BINARY_DIR})

CPMAddPackage(
	NAME cli11
	GITHUB_REPOSITORY CLIUtils/CLI11
	GIT_TAG 6c7b07a878ad834957b98d0f9ce1dbe0cb204fc9 # v2.4.2
)

# warning options
set(UTIL_WARNING_OPTIONS -Wall -Wextra -Werror)

# library
add_library(util src/util/fft.cpp src/util/gnuplot.cpp src/util/hash.cpp src/util/hdf5.cpp src/util/io.cpp src/util/json.cpp src/util/memory.cpp src/util/numerics.cpp src/util/numpy.cpp src/util/sampler.cpp src/util/stats.cpp src/util/string.cpp)
target_compile_features(util PUBLIC cxx_std_20)
set_target_properties(util PROPERTIES CXX_EXTENSIONS ON)
target_include_directories(util PUBLIC include)
target_include_directories(util PUBLIC ${blake3_SOURCE_DIR}/c)
target_link_libraries(util PUBLIC fmt::fmt BLAKE3::blake3)
target_compile_options(util PRIVATE ${UTIL_WARNING_OPTIONS} -O2)
target_compile_options(util PUBLIC ${UTIL_SIMD_COMPILE_OPTIONS})
target_compile_definitions(util PUBLIC ${UTIL_SIMD_COMPILE_DEFINITIONS})

if(UTIL_FFTW)
	target_link_libraries(util PUBLIC fftw3)
	target_compile_definitions(util PUBLIC UTIL_FFTW)
endif()

if(UTIL_HDF5)
	target_include_directories(util PRIVATE ${HDF5_INCLUDE_DIRS})
	target_link_libraries(util PUBLIC ${HDF5_LIBRARIES})
	target_compile_definitions(util PUBLIC UTIL_HDF5)
endif()

# executables
foreach(bin "json-format" "sha256sum" "random")
	add_executable(${bin} src/apps/${bin}.cpp)
	target_compile_features(${bin} PUBLIC cxx_std_20)
	target_compile_options(${bin} PRIVATE ${UTIL_WARNING_OPTIONS} -O2)
	target_link_libraries(${bin} util CLI11::CLI11)
endforeach(bin)

# install
file(GLOB files_h "src/include/*.h")
install(TARGETS util DESTINATION lib)
install(FILES ${files_h} DESTINATION include)

# unittests
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	CPMAddPackage(
		NAME Catch2
		GITHUB_REPOSITORY catchorg/Catch2
		GIT_TAG fa43b77429ba76c462b1898d6cd2f2d7a9416b14 # v3.7.1
	)

	add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
	add_link_options(-fsanitize=address)

	add_executable(tests tests/bits.cpp tests/complex.cpp tests/ddouble.cpp tests/error.cpp tests/hash_map.cpp tests/hash.cpp tests/iterator.cpp tests/json.cpp tests/linalg.cpp tests/memory.cpp tests/numerics.cpp tests/random.cpp tests/series.cpp tests/simd.cpp tests/string.cpp tests/threadpool.cpp tests/vector.cpp)
	target_compile_features(tests PRIVATE cxx_std_20)
	target_link_libraries(tests PRIVATE Catch2::Catch2WithMain util)
	target_compile_options(tests PUBLIC ${UTIL_WARNING_OPTIONS} -g)
endif()
