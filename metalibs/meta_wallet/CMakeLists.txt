project(meta_wallet LANGUAGES CXX)

add_library(${PROJECT_NAME}
        src/common_wallet.cpp
        src/common_wallet_try_delegate.cpp
        src/common_wallet_try_undelegate.cpp
        src/common_wallet_register_node.cpp
        src/common_wallet_apply_delegates.cpp
        src/common_wallet_serialize.cpp
        src/common_wallet_initialize.cpp
        src/common_wallet_try_apply_method.cpp
        src/common_wallet_state_trust.cpp
        src/common_wallet_get.cpp
        src/wallet.cpp
        src/wallet_map.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

target_link_libraries(${PROJECT_NAME} meta_constants)
target_link_libraries(${PROJECT_NAME} meta_crypto)
target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} meta_transaction)
