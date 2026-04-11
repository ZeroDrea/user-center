aux_source_directory(./ SRCS)

add_library(service STATIC ${SRCS})

target_include_directories(service
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(service PRIVATE db ${OPENSSL_LIBRARIES})