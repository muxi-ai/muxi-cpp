include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

add_executable(muxi_tests
    auth_test.cpp
    webhook_test.cpp
    errors_test.cpp
)

target_link_libraries(muxi_tests
    PRIVATE
        muxi
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(muxi_tests)
