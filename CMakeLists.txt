cmake_minimum_required(VERSION 3.20)
project(ecml LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "ECML supports only Linux")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ECML_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(ECML_ENABLE_SANITIZERS "Enable ASan and UBSan" ON)
option(ECML_BUILD_TESTS "Build unit/integration tests" ON)

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/StaticAnalyzers.cmake)

# warnings
add_library(ecml_warnings INTERFACE)
ecml_set_compiler_warnings(ecml_warnings ${ECML_WARNINGS_AS_ERRORS})

# sanitizers
add_library(ecml_sanitizers INTERFACE)
ecml_configure_sanitizers(ecml_sanitizers ${ECML_ENABLE_SANITIZERS})

# main lib
add_library(ecml STATIC
    src/ecml_rawsocket.cpp
)

target_include_directories(ecml
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# warnings links private 
target_link_libraries(ecml
    PUBLIC
        ecml_sanitizers
    PRIVATE
        ecml_warnings
)

# static analyzers so as not to impose strict flags on external projects
ecml_configure_static_analyzers(ecml)

if(ECML_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
