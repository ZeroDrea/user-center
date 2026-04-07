cmake_minimum_required(VERSION 3.12)
project(user_center)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 默认 Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# 编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Wall -Wextra -g -O0 -pthread)
    add_definitions(-DUSER_CNETER_DEBUG)
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

# ======================================================================
# 【关键】查找 MySQL 和 hiredis 库（自动找系统路径，不用手动写路径）
# ======================================================================


# find_package(MySQL REQUIRED)
# find_package(hiredis REQUIRED)

# 子模块
add_subdirectory(src/utils)
add_subdirectory(src/threadpool)
add_subdirectory(src/db)
add_subdirectory(src/net)
add_subdirectory(src/service)

# 主程序
add_executable(${PROJECT_NAME} main.cpp)

# ======================================================================
# 链接：使用官方变量，自动包含库路径 + 头文件路径
# ======================================================================
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    service
    net
    db
    threadpool
    utils
    # ${MYSQL_LIBRARIES}
    # ${HIREDIS_LIBRARIES}
)

# 主程序头文件
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    # ${MYSQL_INCLUDE_DIRS}
    # ${HIREDIS_INCLUDE_DIRS}
)