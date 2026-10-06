function(ecml_configure_static_analyzers target_name)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-18 clang-tidy-17 clang-tidy-16)

  if(CLANG_TIDY_EXE)
    set(CLANG_TIDY_COMMAND
        "${CLANG_TIDY_EXE}"
        "--extra-arg-before=--driver-mode=g++"
        "--extra-arg=-Wno-unknown-warning-option"
    )
    set_target_properties(${target_name} PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}"
    )
    message(STATUS "clang-tidy enabled for ${target_name}")
  endif()
endfunction()
