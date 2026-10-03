function(enable_sanitizers target_name)
    if (NOT ENABLE_SANITIZERS)
        return()
    endif()

    target_compile_options(${target_name} INTERFACE 
        -fsanitize=addres, undefined
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
    )
    target_link_options(${target_name} INTERFACE
        -fsanitize=address, undefined
    )

endfunction()
