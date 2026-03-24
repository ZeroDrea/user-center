
# 收集当前目录所有源文件
aux_source_directory(./ SRCS)

# 生成静态库
add_library(utils STATIC ${SRCS})

# 公共头文件目录（会自动传递给所有依赖它的模块）
target_include_directories(utils
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)