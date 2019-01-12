set (TEST_TARGET test0)

add_executable(${TEST_TARGET}
    test0.cpp
    )

target_include_directories(${TEST_TARGET} PRIVATE
    ${GGSOCK_INCLUDE_DIRS}
    )

target_link_libraries(${TEST_TARGET} PRIVATE
    ${GGSOCK_LIBRARIES}
    )

add_test(NAME test0 COMMAND $<TARGET_FILE:${TEST_TARGET}>)
