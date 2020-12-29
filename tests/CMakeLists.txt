enable_testing()

set (TEST_TARGET test0)

add_executable(${TEST_TARGET}
    test0.cpp
    )

target_link_libraries(${TEST_TARGET} PRIVATE
    ggsock
    )

add_test(NAME test0 COMMAND $<TARGET_FILE:${TEST_TARGET}>)
