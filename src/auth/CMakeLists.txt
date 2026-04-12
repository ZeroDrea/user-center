set(AUTH_SOURCES
    TokenManager.cpp
)

add_library(auth STATIC ${AUTH_SOURCES})
target_include_directories(auth PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(auth PRIVATE ${REDISPP_LIBRARY} ${HIREDIS_LIBRARY} ${OPENSSL_LIBRARIES})