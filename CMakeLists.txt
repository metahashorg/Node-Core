cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
project(metanet VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -march=corei7-avx -O0  -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=corei7-avx -Ofast -Wall -Wextra -pedantic -Wshadow -Wfloat-equal -Wconversion -Wlogical-op -Wduplicated-cond")

set(CMAKE_BUILD_TYPE "Release")
# set(CMAKE_BUILD_TYPE "Debug")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH  ${CMAKE_BINARY_DIR}/lib)

include(GNUInstallDirs)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package (Threads)

add_subdirectory(libs)
add_subdirectory(metalibs)
add_subdirectory(mhsupport)

add_subdirectory(core_service)
