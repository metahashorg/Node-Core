project(core_service LANGUAGES CXX)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/core_service_parse_settings.cpp
        src/core_service_libevent.cpp
        src/core_service_print_config.cpp)

target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(${PROJECT_NAME} meta_core)
target_link_libraries(${PROJECT_NAME} meta_crypto)
target_link_libraries(${PROJECT_NAME} meta_constants)
target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} curl_pp)
target_link_libraries(${PROJECT_NAME} rapidjson)
target_link_libraries(${PROJECT_NAME} tcmalloc)
target_link_libraries(${PROJECT_NAME} version)
