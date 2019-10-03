project(net_io VERSION 0.1 LANGUAGES C CXX)

add_library(${PROJECT_NAME}
    src/http_io_data.cpp
    src/io_service.cpp
    src/picohttpparser.c
    src/socket_client.cpp
    src/socket_io_data.cpp
    src/socket_server.cpp)


target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(${PROJECT_NAME} uriparser)
target_link_libraries(${PROJECT_NAME} moodycamel thread_pool)
target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} metacrypto)
