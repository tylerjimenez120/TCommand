# Fetch GoogleTest for unit testing
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Single test binary aggregating all test files
add_executable(test_suite
    test_mock_hal.cpp
)

target_link_libraries(test_suite PRIVATE
    tcommand_hal
    GTest::gtest_main
    GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(test_suite)