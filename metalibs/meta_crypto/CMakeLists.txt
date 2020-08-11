project(meta_crypto LANGUAGES CXX)

add_library(${PROJECT_NAME}
        src/open_ssl_decor.cpp)

find_package(OpenSSL 1.1.0 REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} xxhash)
