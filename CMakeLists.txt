cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
project(metanet VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -march=x86-64 -mtune=generic -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=x86-64 -mtune=generic -Ofast -Wall")

set(CMAKE_BUILD_TYPE "Release")
#set(CMAKE_BUILD_TYPE "Debug")

set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.66.0 REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
        message(FATAL_ERROR "Minimum required gcc version is 8.0")
    endif ()
endif ()

include(GNUInstallDirs)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads)

add_subdirectory(version)
add_subdirectory(libs)
add_subdirectory(metalibs)

add_subdirectory(core_service)
