cmake_minimum_required (VERSION 3.16.0)
project (util LANGUAGES C CXX)

# dependencies
find_package(HDF5 REQUIRED)

add_subdirectory(fmt EXCLUDE_FROM_ALL)

# main library
add_library(util src/util/gnuplot.cpp src/util/hash.cpp src/util/memory.cpp src/util/numerics.cpp src/util/sampler.cpp src/util/stats.cpp)
target_compile_features(util PUBLIC cxx_std_17)
target_include_directories(util PUBLIC src)
target_link_libraries(util PUBLIC fmt::fmt)
target_compile_options(util PRIVATE -Wall -Wextra -Werror -pedantic)

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
	include(FetchContent)

	FetchContent_Declare(
	  Catch2
	  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
	  GIT_TAG        v3.0.0-preview4
	)

	FetchContent_MakeAvailable(Catch2)

	add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
	add_link_options(-fsanitize=address)

	add_executable(tests tests/hash_map.cpp tests/hash.cpp tests/iterator.cpp tests/memory.cpp tests/numerics.cpp tests/random.cpp tests/threadpool.cpp tests/vector.cpp)
	target_compile_features(tests PRIVATE cxx_std_17)
	target_link_libraries(tests PRIVATE Catch2::Catch2WithMain util)
	target_compile_options(tests PRIVATE -Wall -Wextra -Werror -g -Wno-parentheses)

endif()
