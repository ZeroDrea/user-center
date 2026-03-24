aux_source_directory(./ SRCS)

add_library(service STATIC ${SRCS})

target_include_directories(service
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# 依赖
target_link_libraries(service
    PRIVATE
    db
    net
    utils
)