project(metachain VERSION 0.5 LANGUAGES CXX)

add_library(${PROJECT_NAME}
        src/approve_block.cpp
        src/approve_record.cpp
        src/block_chain_controller.cpp
        src/block.cpp
        src/blockchain.cpp
        src/common_block.cpp
        src/common_wallet.cpp
        src/controller.cpp
        src/core_controller.cpp
        src/decentralized_application_wallet.cpp
        src/rejected_block.cpp
        src/rejected_tx.cpp
        src/transaction.cpp
        src/wallet.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} curl)
target_link_libraries(${PROJECT_NAME} stdc++fs)
target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} metacrypto)
target_link_libraries(${PROJECT_NAME} moodycamel)
target_link_libraries(${PROJECT_NAME} thread_pool)
target_link_libraries(${PROJECT_NAME} rapidjson)
