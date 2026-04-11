cmake_minimum_required(VERSION 3.20)
project(user_center)

set(MYSQLCPPCONN_INCLUDE_DIR "/usr/include")
set(MYSQLCPPCONN_LIBRARY "mysqlcppconn")

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED) # （用于 SHA256）

# 默认 Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# 编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Wall -Wextra -g -O0 -pthread)
    add_definitions(-DUSER_CENTER_DEBUG)
    message("Build mode: Debug")
else()
    add_compile_options(-Wall -Wextra -O2 -pthread)
    add_definitions(-DUSER_CENTER_RELEASE)
    message("Build mode: Release")
endif()

# 统一输出路径
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/bin)

add_subdirectory(src/utils)
add_subdirectory(src/threadpool)
add_subdirectory(src/db)
add_subdirectory(src/net)
add_subdirectory(src/http)
add_subdirectory(src/router)
add_subdirectory(src/service)

# 主程序
add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    net utils db threadpool http router service
    ${MYSQLCPPCONN_LIBRARY}
    ${OPENSSL_LIBRARIES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${MYSQLCPPCONN_INCLUDE_DIR}
)