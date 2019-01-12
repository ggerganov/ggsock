set(TOOL_TARGET test_server)
add_executable(${TOOL_TARGET} test_server.cpp)
target_include_directories(${TOOL_TARGET} PRIVATE ${GGSOCK_INCLUDE_DIRS})
target_link_libraries(${TOOL_TARGET} PRIVATE ${GGSOCK_LIBRARIES})

set(TOOL_TARGET test_client)
add_executable(${TOOL_TARGET} test_client.cpp)
target_include_directories(${TOOL_TARGET} PRIVATE ${GGSOCK_INCLUDE_DIRS})
target_link_libraries(${TOOL_TARGET} PRIVATE ${GGSOCK_LIBRARIES})

