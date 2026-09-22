include(FetchContent)

find_package(GTest CONFIG QUIET)

if (NOT TARGET GTest::gtest_main)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.12.0
  )
  # forcing static linking of gtest
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  # gtest will not be installed if ecml is being installed
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(ecml_tests test_rawsocket.cpp)

target_link_libraries(ecml_tests
  PRIVATE
    ecml::ecml
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(ecml_tests)
