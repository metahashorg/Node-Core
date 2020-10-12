project(meta_pool LANGUAGES CXX)

add_library(${PROJECT_NAME}
        src/thread_pool.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)