include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    SYSTEM # ignore GoogleTest warnings with -werro
)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(ecml_tests
    test_rawsocket.cpp
)

target_link_libraries(ecml_tests
    PRIVATE
        ecml
        ecml_warnings
        GTest::gtest_main
)

ecml_configure_static_analyzers(ecml_tests)

include(GoogleTest)
gtest_discover_tests(ecml_tests)
