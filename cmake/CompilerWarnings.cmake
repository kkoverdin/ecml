function(ecml_set_compiler_warnings target_name warnings_as_errors)
  set(CLANG_WARNINGS
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wnon-virtual-dtor
      -Wcast-align
      -Wunused
      -Woverloaded-virtual
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat=2
      -Wimplicit-fallthrough
  )

  set(GCC_WARNINGS
      ${CLANG_WARNINGS}
      -Wlogical-op
      -Wduplicated-cond
      -Wduplicated-branches
  )

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(PROJECT_WARNINGS ${CLANG_WARNINGS})
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(PROJECT_WARNINGS ${GCC_WARNINGS})
  else()
    message(WARNING "Compiler warnings not configured for ${CMAKE_CXX_COMPILER_ID}")
    return()
  endif()

  if(warnings_as_errors)
    list(APPEND PROJECT_WARNINGS -Werror)
  endif()

  target_compile_options(${target_name} INTERFACE ${PROJECT_WARNINGS})
endfunction()
