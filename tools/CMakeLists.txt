set(TOOL_TARGET test-server)
add_executable(${TOOL_TARGET} test-server.cpp)
target_link_libraries(${TOOL_TARGET} PRIVATE ggsock)

set(TOOL_TARGET test-client)
add_executable(${TOOL_TARGET} test-client.cpp)
target_link_libraries(${TOOL_TARGET} PRIVATE ggsock)

set(TOOL_TARGET test-file-server)
add_executable(${TOOL_TARGET} test-file-server.cpp)
target_link_libraries(${TOOL_TARGET} PRIVATE ggsock)

set(TOOL_TARGET test-file-client)
add_executable(${TOOL_TARGET} test-file-client.cpp)
target_link_libraries(${TOOL_TARGET} PRIVATE ggsock)
