project(meta_network LANGUAGES CXX)

add_library(${PROJECT_NAME}
        src/meta_client.cpp
        src/meta_client_connection.cpp
        src/meta_client_connection_response.cpp
        src/meta_server.cpp
        src/meta_server_connection.cpp
        src/meta_server_connection_request.cpp
        src/meta_server_connection_write.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(${PROJECT_NAME} moodycamel)
target_link_libraries(${PROJECT_NAME} meta_pool)
#target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} meta_crypto)
target_link_libraries(${PROJECT_NAME} version)
