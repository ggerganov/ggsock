#
## ggsock

add_library(ggsock ${GGSOCK_LIBRARY_TYPE}
    communicator.cpp
    file-server.cpp
    serialization.cpp
    )

target_include_directories(ggsock PUBLIC
    ./
    ../include/
    )

target_include_directories(ggsock PRIVATE
    )

target_link_libraries(ggsock PUBLIC
    ${CMAKE_THREAD_LIBS_INIT}
    )

if(WIN32)
    target_link_libraries(ggsock PRIVATE wsock32 ws2_32)
endif()
