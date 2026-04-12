cmake_minimum_required(VERSION 3.20)
project(user_center)

set(MYSQLCPPCONN_INCLUDE_DIR "/usr/include")
set(MYSQLCPPCONN_LIBRARY "mysqlcppconn")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_library(BCRYPT_LIBRARY NAMES bcrypt REQUIRED)

# 手动查找 hiredis 和 redis++
find_path(HIREDIS_INCLUDE_DIR NAMES hiredis/hiredis.h REQUIRED)
find_library(HIREDIS_LIBRARY NAMES hiredis REQUIRED)
find_path(REDISPP_INCLUDE_DIR NAMES sw/redis++/redis++.h REQUIRED)
find_library(REDISPP_LIBRARY NAMES redis++ REQUIRED)

# 默认 Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Wall -Wextra -g -O0)
    add_definitions(-DUSER_CENTER_DEBUG)
    message("Build mode: Debug")
else()
    add_compile_options(-Wall -Wextra -O2)
    add_definitions(-DUSER_CENTER_RELEASE)
    message("Build mode: Release")
endif()

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
add_subdirectory(src/auth)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    net utils db threadpool http router service auth
    ${MYSQLCPPCONN_LIBRARY}
    ${OPENSSL_LIBRARIES}
    ${REDISPP_LIBRARY}
    ${HIREDIS_LIBRARY}
    Threads::Threads
    ${BCRYPT_LIBRARY}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${MYSQLCPPCONN_INCLUDE_DIR}
    ${HIREDIS_INCLUDE_DIR}
    ${REDISPP_INCLUDE_DIR}
)