project(mh_xxhash VERSION 0.1 LANGUAGES C)

add_library(${PROJECT_NAME}
    src/xxhash.c)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)