aux_source_directory(./ SRCS)

add_library(db STATIC ${SRCS})

target_include_directories(db
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# 依赖
target_link_libraries(db
    PRIVATE
    utils
)