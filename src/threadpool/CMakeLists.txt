aux_source_directory(./ SRCS)

add_library(threadpool STATIC ${SRCS})

target_include_directories(threadpool
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# 依赖
target_link_libraries(threadpool
    PRIVATE
    utils
)