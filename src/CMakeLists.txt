#
## ggsock

add_library(ggsock
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

if (WIN32)
    target_link_libraries(ggsock PRIVATE wsock32 ws2_32)
endif()

if (MINGW)
    target_link_libraries(ggsock PRIVATE wsock32 ws2_32 stdc++)
endif()

install(TARGETS ggsock
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    )
