project(mh_libevent VERSION 0.1 LANGUAGES CXX)

add_library(${PROJECT_NAME}
    src/LibEvent.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(${PROJECT_NAME} event.a)
