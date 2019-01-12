#
## GGSock Core Lib

add_library(${GGSOCK_CORE_LIB} SHARED
    Communicator.cpp
    )

target_include_directories(${GGSOCK_CORE_LIB} PUBLIC
    ${GGSOCK_INCLUDE_DIRS}
    )

target_include_directories(${GGSOCK_CORE_LIB} PRIVATE
    )

target_link_libraries(${GGSOCK_CORE_LIB} PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    )

if(WIN32)
    target_link_libraries(${GGSOCK_CORE_LIB} PRIVATE wsock32 ws2_32)
endif()

#
## Install libs

if ("x${LIBRARY_INSTALL_PATH}" STREQUAL "x")
    set(LIBRARY_INSTALL_PATH "${PROJECT_SOURCE_DIR}/lib")
endif()

install(TARGETS ${GGSOCK_LIBRARIES} DESTINATION ${LIBRARY_INSTALL_PATH})
