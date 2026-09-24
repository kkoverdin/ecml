cmake_minimum_required(VERSION 3.21)
project(ecml)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)
include(StaticAnalyzers)

setup_static_analyzers()

add_library(ecml_warnings INTERFACE)
set_project_warnings(ecml_warnings)

add_library(ecml_sanitizers INTERFACE)
enable_sanitizers(ecml_sanitizers)

add_library(ecml)
add_library(ecml::ecml ALIAS ecml)

target_include_directories(ecml
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    src
)

target_link_libraries(ecml PRIVATE ecml_warnings)

include(CTest)
if (BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
  add_subdirectory(tests)
endif()
