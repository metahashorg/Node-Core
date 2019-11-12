project(core_service VERSION 0.3.1 LANGUAGES CXX)

################################################################################
# Get the current working branch
execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Git current branch: ${GIT_BRANCH}")
message(STATUS "Git commit hash: ${GIT_COMMIT_HASH}")

message(STATUS "Generating version.h")

message(STATUS "core_service PROJECT_BINARY_DIR = ${PROJECT_BINARY_DIR}")

add_definitions("-DGIT_COMMIT_HASH=${GIT_COMMIT_HASH}")
add_definitions("-DGIT_BRANCH=${GIT_BRANCH}")

configure_file(
        ${PROJECT_SOURCE_DIR}/src/version.h.in
        ${PROJECT_BINARY_DIR}/generated/version.h
)

include_directories(${PROJECT_BINARY_DIR}/generated)

################################################################################

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/blockserver.cpp
        )

target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(${PROJECT_NAME} tcmalloc)
target_link_libraries(${PROJECT_NAME} stdc++fs)
target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} metachain)
target_link_libraries(${PROJECT_NAME} metacrypto)
#target_link_libraries(${PROJECT_NAME} mh_libevent)
target_link_libraries(${PROJECT_NAME} mh_mhd)
target_link_libraries(${PROJECT_NAME} moodycamel)
target_link_libraries(${PROJECT_NAME} net_io)
target_link_libraries(${PROJECT_NAME} mhcurl)
target_link_libraries(${PROJECT_NAME} rapidjson)
