project(net_io VERSION 0.1 LANGUAGES C CXX)

add_library(${PROJECT_NAME}
        src/meta_server.cpp
        src/meta_client.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(${PROJECT_NAME} uriparser)
target_link_libraries(${PROJECT_NAME} moodycamel thread_pool)
target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} metacrypto)
target_link_libraries(${PROJECT_NAME} version)
