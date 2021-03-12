project(meta_core LANGUAGES CXX)

add_library(${PROJECT_NAME}
        src/block_chain_controller.cpp
        src/controller.cpp
        src/controller_write_block.cpp
        src/controller_process_requests.cpp
        src/controller_actualize_chain.cpp
        src/controller_main_loop.cpp
        src/controller_try_make_block.cpp
        src/controller_check_online_nodes.cpp
        src/controller_read_and_apply_local_chain.cpp
        src/controller_add_pack_to_queue.cpp
        src/controller_public_interface.cpp 
        src/controller_blocks.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

target_link_libraries(${PROJECT_NAME} meta_block)
target_link_libraries(${PROJECT_NAME} meta_chain)
target_link_libraries(${PROJECT_NAME} meta_connections)
target_link_libraries(${PROJECT_NAME} meta_constants)
target_link_libraries(${PROJECT_NAME} meta_crypto)
target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} meta_network)
target_link_libraries(${PROJECT_NAME} meta_pool)
target_link_libraries(${PROJECT_NAME} meta_transaction)
target_link_libraries(${PROJECT_NAME} moodycamel)
target_link_libraries(${PROJECT_NAME} rapidjson)
target_link_libraries(${PROJECT_NAME} stdc++fs)
