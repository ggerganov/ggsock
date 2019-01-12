cmake_minimum_required (VERSION 2.8)
project (GGSock)

set(CMAKE_EXPORT_COMPILE_COMMANDS "on")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RELEASE CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "DEBUG" "RELEASE" "RELWITHDEBINFO")
endif()

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

set(GGSOCK_CORE_LIB ggsock)

set(GGSOCK_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include CACHE STRING "GGSock Includes")
set(GGSOCK_LIBRARIES ${GGSOCK_CORE_LIB} CACHE STRING "GGSock Libraries")

add_subdirectory(src)

if (GGSOCK_DISABLE_PROGRAMS_BUILD)
    return()
endif ()

enable_testing()

add_subdirectory(tests)
add_subdirectory(tools)
