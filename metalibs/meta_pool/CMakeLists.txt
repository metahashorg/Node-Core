project(meta_pool LANGUAGES CXX)

set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.66.0 REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

add_library(${PROJECT_NAME}
        src/thread_pool.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)