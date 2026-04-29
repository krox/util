cmake_minimum_required(VERSION 3.21)
project(util LANGUAGES C CXX)

include(CheckCXXSourceCompiles)

option(UTIL_BUILD_APPS "build util command-line tools" ${PROJECT_IS_TOP_LEVEL})
option(UTIL_BUILD_TESTS "build util unit tests" ${PROJECT_IS_TOP_LEVEL})
# Semicolon-separated CMake list of extra flags for util-owned targets only.
set(UTIL_COMPILE_OPTIONS "" CACHE STRING "extra compiler flags applied only to util-owned targets")

# Keep system dependencies opt-in so util remains easy to build on minimal systems.
option(UTIL_FFTW "link to FFTW3 for Fourier transforms" OFF)
option(UTIL_HDF5 "link to HDF5 for I/O" OFF)
option(UTIL_LUA "link to Lua for scripting" OFF)
option(UTIL_SQLITE3 "build with SQLite3 amalgamation from sqlite.org" ${PROJECT_IS_TOP_LEVEL})
option(UTIL_ZSTD "build with vendored zstd compression support" ${PROJECT_IS_TOP_LEVEL})

# Keep unit test coverage complete: tests always exercise Lua and SQLite paths.
if(UTIL_BUILD_TESTS)
	if(NOT UTIL_LUA)
		message(STATUS "UTIL_BUILD_TESTS=ON requires Lua tests: forcing UTIL_LUA=ON")
	endif()
	if(NOT UTIL_SQLITE3)
		message(STATUS "UTIL_BUILD_TESTS=ON requires SQLite tests: forcing UTIL_SQLITE3=ON")
	endif()
	set(UTIL_LUA ON CACHE BOOL "link to Lua for scripting" FORCE)
	set(UTIL_SQLITE3 ON CACHE BOOL "build with SQLite3 amalgamation from sqlite.org" FORCE)
endif()

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

include(FetchContent)

# dependencies
if(UTIL_HDF5)
	find_package(HDF5 REQUIRED COMPONENTS C)
endif()

if(UTIL_FFTW)
	set(UTIL_FFTW_TARGET "")

	find_package(FFTW3 CONFIG QUIET)
	if(TARGET FFTW3::fftw3)
		set(UTIL_FFTW_TARGET FFTW3::fftw3)
	endif()

	if(NOT UTIL_FFTW_TARGET)
		find_package(PkgConfig QUIET)
		if(PkgConfig_FOUND)
			pkg_check_modules(FFTW3 QUIET IMPORTED_TARGET fftw3)
			if(TARGET PkgConfig::FFTW3)
				set(UTIL_FFTW_TARGET PkgConfig::FFTW3)
			endif()
		endif()
	endif()

	if(NOT UTIL_FFTW_TARGET)
		find_path(UTIL_FFTW_INCLUDE_DIR NAMES fftw3.h)
		find_library(UTIL_FFTW_LIBRARY NAMES fftw3)

		if(UTIL_FFTW_INCLUDE_DIR AND UTIL_FFTW_LIBRARY)
			add_library(util_fftw3 INTERFACE)
			target_include_directories(util_fftw3 SYSTEM INTERFACE ${UTIL_FFTW_INCLUDE_DIR})
			target_link_libraries(util_fftw3 INTERFACE ${UTIL_FFTW_LIBRARY})
			set(UTIL_FFTW_TARGET util_fftw3)
		endif()
	endif()

	if(NOT UTIL_FFTW_TARGET)
		message(FATAL_ERROR "UTIL_FFTW is ON, but FFTW3 could not be found. Install FFTW3 or configure with -DUTIL_FFTW=OFF.")
	endif()
endif()

include(cmake/CPM.cmake)

if(UTIL_LUA)
	CPMAddPackage(
		NAME lua
		URL https://www.lua.org/ftp/lua-5.4.6.tar.gz
		URL_HASH SHA256=7d5ea1b9cb6aa0b59ca3dde1c6adcb57ef83a1ba8e5432c0ecd06bf439b3ad88
		DOWNLOAD_ONLY True
	)

	# Build vendored Lua as a separate target so util-specific warning flags do
	# not spill into third-party C code.
	add_library(util_lua_vendor STATIC
		${lua_SOURCE_DIR}/src/lapi.c
		${lua_SOURCE_DIR}/src/lauxlib.c
		${lua_SOURCE_DIR}/src/lbaselib.c
		${lua_SOURCE_DIR}/src/lcode.c
		${lua_SOURCE_DIR}/src/lcorolib.c
		${lua_SOURCE_DIR}/src/lctype.c
		${lua_SOURCE_DIR}/src/ldblib.c
		${lua_SOURCE_DIR}/src/ldebug.c
		${lua_SOURCE_DIR}/src/ldo.c
		${lua_SOURCE_DIR}/src/ldump.c
		${lua_SOURCE_DIR}/src/lfunc.c
		${lua_SOURCE_DIR}/src/lgc.c
		${lua_SOURCE_DIR}/src/linit.c
		${lua_SOURCE_DIR}/src/liolib.c
		${lua_SOURCE_DIR}/src/llex.c
		${lua_SOURCE_DIR}/src/lmathlib.c
		${lua_SOURCE_DIR}/src/lmem.c
		${lua_SOURCE_DIR}/src/loadlib.c
		${lua_SOURCE_DIR}/src/lobject.c
		${lua_SOURCE_DIR}/src/lopcodes.c
		${lua_SOURCE_DIR}/src/loslib.c
		${lua_SOURCE_DIR}/src/lparser.c
		${lua_SOURCE_DIR}/src/lstate.c
		${lua_SOURCE_DIR}/src/lstring.c
		${lua_SOURCE_DIR}/src/lstrlib.c
		${lua_SOURCE_DIR}/src/ltable.c
		${lua_SOURCE_DIR}/src/ltablib.c
		${lua_SOURCE_DIR}/src/ltm.c
		${lua_SOURCE_DIR}/src/lundump.c
		${lua_SOURCE_DIR}/src/lutf8lib.c
		${lua_SOURCE_DIR}/src/lvm.c
		${lua_SOURCE_DIR}/src/lzio.c
	)
	target_include_directories(util_lua_vendor PUBLIC ${lua_SOURCE_DIR}/src)
	target_compile_definitions(util_lua_vendor PRIVATE LUA_USE_LINUX)
	target_link_libraries(util_lua_vendor PUBLIC m dl)
endif()

if(UTIL_SQLITE3)
	set(UTIL_SQLITE3_AMALGAMATION_VERSION "3500400" CACHE STRING "SQLite amalgamation version number from sqlite.org")
	set(UTIL_SQLITE3_AMALGAMATION_YEAR "2025" CACHE STRING "sqlite.org release year directory for the selected amalgamation")

	FetchContent_Declare(
		sqlite3_amalgamation
		URL "https://www.sqlite.org/${UTIL_SQLITE3_AMALGAMATION_YEAR}/sqlite-amalgamation-${UTIL_SQLITE3_AMALGAMATION_VERSION}.zip"
		DOWNLOAD_EXTRACT_TIMESTAMP FALSE
	)
	FetchContent_GetProperties(sqlite3_amalgamation)

	if(NOT sqlite3_amalgamation_POPULATED)
		FetchContent_Populate(sqlite3_amalgamation)
	endif()

	if(NOT EXISTS "${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c" OR NOT EXISTS "${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.h")
		message(FATAL_ERROR "Downloaded amalgamation at ${sqlite3_amalgamation_SOURCE_DIR} does not contain sqlite3.c and sqlite3.h")
	endif()
endif()

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

if(UTIL_BUILD_APPS)
	CPMAddPackage(
		NAME cli11
		GITHUB_REPOSITORY CLIUtils/CLI11
		GIT_TAG 6c7b07a878ad834957b98d0f9ce1dbe0cb204fc9 # v2.4.2
	)
endif()

CPMAddPackage(
	NAME linenoise
	GITHUB_REPOSITORY antirez/linenoise
	GIT_TAG master
	DOWNLOAD_ONLY True
)

# Vendored C sources are kept out of the util target so dev-only compiler
# options do not spill into third-party code.
add_library(util_linenoise_vendor STATIC ${linenoise_SOURCE_DIR}/linenoise.c)
target_include_directories(util_linenoise_vendor PRIVATE ${linenoise_SOURCE_DIR})

if(UTIL_ZSTD)
	CPMAddPackage(
		NAME zstd
		GITHUB_REPOSITORY facebook/zstd
		GIT_TAG 794ea1b0afca0f020f4e57b6732332231fb23c70 # v1.5.6
		SOURCE_SUBDIR build/cmake
		OPTIONS
		"ZSTD_BUILD_SHARED OFF"
		"ZSTD_BUILD_STATIC ON"
		"ZSTD_BUILD_PROGRAMS OFF"
		"ZSTD_BUILD_TESTS OFF"
		"ZSTD_BUILD_CONTRIB OFF"
	)
endif()

# library
add_library(util src/util/fft.cpp src/util/gnuplot.cpp src/util/hash.cpp src/util/hdf5.cpp src/util/io.cpp src/util/json.cpp src/util/linenoise.cpp src/util/memory.cpp src/util/numerics.cpp src/util/numpy.cpp src/util/sampler.cpp src/util/sqlite.cpp src/util/stats.cpp src/util/string.cpp src/util/unionfind.cpp)
target_compile_features(util PUBLIC cxx_std_20)
set_target_properties(util PROPERTIES CXX_EXTENSIONS ON)
target_include_directories(util
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${blake3_SOURCE_DIR}/c>
	PRIVATE
		${linenoise_SOURCE_DIR}
)
target_link_libraries(util PUBLIC fmt::fmt BLAKE3::blake3)
target_link_libraries(util PRIVATE util_linenoise_vendor)
target_compile_options(util PUBLIC ${UTIL_SIMD_COMPILE_OPTIONS})
target_compile_options(util PRIVATE ${UTIL_COMPILE_OPTIONS})
target_compile_definitions(util PUBLIC ${UTIL_SIMD_COMPILE_DEFINITIONS})

if(UTIL_FFTW)
	target_link_libraries(util PUBLIC ${UTIL_FFTW_TARGET})
	target_compile_definitions(util PUBLIC UTIL_FFTW)
endif()

if(UTIL_HDF5)
	target_link_libraries(util PUBLIC HDF5::HDF5)
	target_compile_definitions(util PUBLIC UTIL_HDF5)
endif()

if(UTIL_LUA)
	target_sources(util PRIVATE src/util/lua.cpp)
	target_link_libraries(util PUBLIC util_lua_vendor)
	target_compile_definitions(util PUBLIC UTIL_LUA)
endif()

if(UTIL_SQLITE3)
	add_library(util_sqlite3_vendor STATIC ${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c)
	target_include_directories(util PUBLIC ${sqlite3_amalgamation_SOURCE_DIR})
	target_link_libraries(util PRIVATE util_sqlite3_vendor)
	target_compile_definitions(util PUBLIC UTIL_SQLITE3)
endif()

if(UTIL_ZSTD)
	target_link_libraries(util PUBLIC libzstd_static)
	target_compile_definitions(util PUBLIC UTIL_ZSTD)
endif()

# executables
if(UTIL_BUILD_APPS)
	foreach(bin "json-format" "sha256sum" "random")
		add_executable(${bin} src/apps/${bin}.cpp)
		target_compile_features(${bin} PUBLIC cxx_std_20)
		target_link_libraries(${bin} util CLI11::CLI11)
		target_compile_options(${bin} PRIVATE ${UTIL_COMPILE_OPTIONS})
	endforeach(bin)
endif()

if(UTIL_BUILD_APPS AND UTIL_LUA)
	add_executable(lua-repl src/apps/lua-repl.cpp)
	target_compile_features(lua-repl PUBLIC cxx_std_20)
	target_link_libraries(lua-repl util)
	target_compile_options(lua-repl PRIVATE ${UTIL_COMPILE_OPTIONS})
endif()

# unittests

if(UTIL_BUILD_TESTS)
	include(CTest)

	if(BUILD_TESTING)
		set(UTIL_TEST_SOURCES
			tests/bits.cpp
			tests/complex.cpp
			tests/ddouble.cpp
			tests/error.cpp
			tests/hash_map.cpp
			tests/hash.cpp
			tests/io.cpp
			tests/iterator.cpp
			tests/json.cpp
			tests/linalg.cpp
			tests/memory.cpp
			tests/numerics.cpp
			tests/random.cpp
			tests/series.cpp
			tests/simd.cpp
			tests/sorted_vector.cpp
			tests/string.cpp
			tests/threadpool.cpp
			tests/unionfind.cpp
			tests/vector.cpp
			tests/lua.cpp
			tests/sqlite.cpp
		)

		CPMAddPackage(
			NAME Catch2
			GITHUB_REPOSITORY catchorg/Catch2
			GIT_TAG fa43b77429ba76c462b1898d6cd2f2d7a9416b14 # v3.7.1
		)
		include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

		add_library(util_asan INTERFACE)
		target_compile_options(util_asan INTERFACE -fsanitize=address -fno-omit-frame-pointer)
		target_link_options(util_asan INTERFACE -fsanitize=address)
		target_link_libraries(util PRIVATE util_asan)
		target_link_libraries(util_linenoise_vendor PRIVATE util_asan)
		target_link_libraries(util_sqlite3_vendor PRIVATE util_asan)

		add_executable(tests ${UTIL_TEST_SOURCES})
		target_compile_features(tests PRIVATE cxx_std_20)
		target_link_libraries(tests PRIVATE Catch2::Catch2WithMain util util_asan)
		target_compile_options(tests PRIVATE ${UTIL_COMPILE_OPTIONS})
		catch_discover_tests(tests)
	endif()
endif()
