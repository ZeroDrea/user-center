aux_source_directory(./ SRCS)

add_library(router STATIC ${SRCS})


target_include_directories(router PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
