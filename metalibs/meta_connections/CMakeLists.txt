project(meta_connections LANGUAGES CXX)

add_library(${PROJECT_NAME}
        src/cores_connections.cpp
        src/cores_send.cpp
        src/cores_sync.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

target_link_libraries(${PROJECT_NAME} meta_network)
target_link_libraries(${PROJECT_NAME} meta_constants)
target_link_libraries(${PROJECT_NAME} meta_crypto)
