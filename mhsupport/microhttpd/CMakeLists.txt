project(microhttpd VERSION 0.1 LANGUAGES C)

add_library(${PROJECT_NAME}
	src/base64.c
	src/basicauth.c
	src/connection.c
	src/connection_https.c
	src/daemon.c
	src/digestauth.c
	src/internal.c
	src/md5.c
	src/memorypool.c
	src/mhd_mono_clock.c
	src/mhd_str.c
	src/postprocessor.c
	src/reason_phrase.c
	src/response.c
	src/sysfdsetsize.c
	src/tsearch.c
	)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)


target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} gnutls)
target_link_libraries(${PROJECT_NAME} gcrypt)
