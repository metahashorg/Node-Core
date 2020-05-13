project(metacrypto VERSION 0.1 LANGUAGES CXX)

add_library(${PROJECT_NAME}
        src/keccak.cpp
        src/open_ssl_decor.cpp)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} crypto++.a)
target_link_libraries(${PROJECT_NAME} mh_xxhash)
