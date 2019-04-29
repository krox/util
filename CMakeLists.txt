cmake_minimum_required (VERSION 3.9.0)
project (util)

include_directories(src)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB files_cpp "src/util/*.cpp")
file(GLOB files_h "src/util/*.h")

add_library(util ${files_cpp})
target_link_libraries(util fmt)

install(TARGETS util DESTINATION lib)
install(FILES ${files_h} DESTINATION include)
