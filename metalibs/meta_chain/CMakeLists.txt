project(meta_chain LANGUAGES CXX)

add_library(${PROJECT_NAME}
        src/blockchain.cpp
        src/can_apply_common_block.cpp
        src/can_apply_forging_block.cpp
        src/can_apply_state_block.cpp
        src/interface_functions.cpp
        src/make_common_block.cpp
        src/make_forging_block.cpp
        src/make_state_block.cpp
        src/make_statistics_block.cpp
        src/try_apply_block.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

target_link_libraries(${PROJECT_NAME} meta_block)
target_link_libraries(${PROJECT_NAME} meta_constants)
target_link_libraries(${PROJECT_NAME} meta_crypto)
target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} meta_pool)
target_link_libraries(${PROJECT_NAME} meta_transaction)
target_link_libraries(${PROJECT_NAME} meta_wallet)
target_link_libraries(${PROJECT_NAME} rapidjson)
