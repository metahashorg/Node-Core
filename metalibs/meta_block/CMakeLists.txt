project(meta_block LANGUAGES CXX)

add_library(${PROJECT_NAME}
        src/block.cpp
        src/approve_block_get_txs.cpp
        src/approve_block_make.cpp
        src/approve_block_parse.cpp
        src/common_block_get.cpp
        src/common_block_parse.cpp
        src/rejected_block_get.cpp
        src/rejected_block_make.cpp
        src/rejected_block_parse.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

target_link_libraries(${PROJECT_NAME} meta_constants)
target_link_libraries(${PROJECT_NAME} meta_crypto)
target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} meta_pool)
target_link_libraries(${PROJECT_NAME} meta_transaction)
