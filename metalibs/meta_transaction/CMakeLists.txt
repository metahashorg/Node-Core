project(meta_transaction LANGUAGES CXX)

add_library(${PROJECT_NAME}
        src/approve_record.cpp
        src/approve_record_parse.cpp
        src/rejected_tx.cpp
        src/transaction.cpp
        src/transaction_constructors.cpp
        src/transaction_parse.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

target_link_libraries(${PROJECT_NAME} meta_constants)
target_link_libraries(${PROJECT_NAME} meta_crypto)
target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} rapidjson)
