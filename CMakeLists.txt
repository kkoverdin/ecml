cmake_minimum_required(VERSION 3.21)
project(ecml)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(ecml)
add_library(ecml::ecml ALIAS ecml)

target_include_directories(ecml
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DUR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    src
)

include(CTest)
if (BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
  add_subdirectory(tests)
endif()
