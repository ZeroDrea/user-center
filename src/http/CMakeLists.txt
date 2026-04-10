aux_source_directory(./ SRCS)

add_library(http STATIC ${SRCS})


target_include_directories(http PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
