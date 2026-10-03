function(set_project_warnings target_name)
    option(WARNINGS_AS_ERRORS "Treat compiler warnings as error" OFF)
    
    # supposing that compiler either GCC or CLANG
    
    set(WARNINGS_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-deference
        -Wdouble-promotion
        -Wformat=2
    )
    
    if (WARNINGS_AS_ERRORS)
        list(append WARNINGS_FLAGS -Werror)
    endif()

    target_compile_options(${target_name} INTERFACE ${WARNINGS_FLAGS})

endfunction()
