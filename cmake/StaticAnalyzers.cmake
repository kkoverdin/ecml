macro(setup_static_analyzers)
    option(ENABLE_CLANG_TIDY "Run clang-tidy with compiler" OFF)

    if (ENABLE_CLANG_TIDY)
        find_program(CLANG_TIDY_EXE NAMES clang-tidy)
        if(CLANG_TIDY_EXE)
            set(CMAKE_CXX_CLANG_TIDY
                ${CLANG_TIDY_EXE};
                -extra-arg=-Wno-unknown-warning-option
            )
            message(STATUS " clang-tidy enbled: ${CLANG_TIDY_EXE}")
        else()
            message(WARNING " ENABLE_CLANG_TIDY is ON, but clang-tidy binary was not found")
        endif()
    endif()
endmacro()
