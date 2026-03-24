aux_source_directory(./ SRCS)

add_library(net STATIC ${SRCS})

target_include_directories(net
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# 依赖
target_link_libraries(net
    PRIVATE
    utils
    threadpool
)