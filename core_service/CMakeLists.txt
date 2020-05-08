project(core_service VERSION 0.3 LANGUAGES CXX)

add_executable(${PROJECT_NAME}
        src/main.cpp
        )

target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(${PROJECT_NAME} tcmalloc)
target_link_libraries(${PROJECT_NAME} meta_log)
target_link_libraries(${PROJECT_NAME} metachain)
target_link_libraries(${PROJECT_NAME} metacrypto)
target_link_libraries(${PROJECT_NAME} moodycamel)
target_link_libraries(${PROJECT_NAME} mhcurl)
target_link_libraries(${PROJECT_NAME} rapidjson)
