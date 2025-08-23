cmake_minimum_required (VERSION 3.10)
project (ggsock)

set(CMAKE_EXPORT_COMPILE_COMMANDS "on")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(GGSOCK_STANDALONE ON)
    include(cmake/GitVars.cmake)
    include(cmake/BuildTypes.cmake)
else()
    set(GGSOCK_STANDALONE OFF)
endif()

if (EMSCRIPTEN)
    set(BUILD_SHARED_LIBS_DEFAULT OFF)
else()
    set(BUILD_SHARED_LIBS_DEFAULT ON)
endif()

# options

option(BUILD_SHARED_LIBS              "ggsock: build shared libs" ${BUILD_SHARED_LIBS_DEFAULT})

option(GGSOCK_ALL_WARNINGS            "ggsock: enable all compiler warnings" ON)
option(GGSOCK_ALL_WARNINGS_3RD_PARTY  "ggsock: enable all compiler warnings in 3rd party libs" ON)

option(GGSOCK_SANITIZE_THREAD         "ggsock: enable thread sanitizer" OFF)
option(GGSOCK_SANITIZE_ADDRESS        "ggsock: enable address sanitizer" OFF)
option(GGSOCK_SANITIZE_UNDEFINED      "ggsock: enable undefined sanitizer" OFF)

option(GGSOCK_BUILD_EXAMPLES          "ggsock: build examples" ${GGSOCK_STANDALONE})

# sanitizers

if (GGSOCK_SANITIZE_THREAD)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

if (GGSOCK_SANITIZE_ADDRESS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
endif()

if (GGSOCK_SANITIZE_UNDEFINED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
endif()

# dependencies

find_package(Threads REQUIRED)

# main

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (GGSOCK_ALL_WARNINGS)
    if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    else()
        # todo : windows
    endif()
endif()

add_subdirectory(src)

if (GGSOCK_DISABLE_PROGRAMS_BUILD)
    return()
endif ()

if (GGSOCK_STANDALONE AND GGSOCK_BUILD_EXAMPLES)
    add_subdirectory(tests)
    add_subdirectory(tools)
endif()
