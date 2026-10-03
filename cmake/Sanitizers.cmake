function(ecml_configure_sanitizers target_name enable_sanitizers)
  if(NOT enable_sanitizers)
    return()
  endif()

  # release without sanitizers
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Sanitizers disabled for Release build type")
    return()
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(SAN_FLAGS "-fsanitize=address,undefined")
    
    target_compile_options(${target_name} INTERFACE 
        "${SAN_FLAGS}" 
        "-fno-omit-frame-pointer"
        "-fno-optimize-sibling-calls"
    )
    
    target_link_options(${target_name} INTERFACE 
        "${SAN_FLAGS}"
    )
  else()
    message(WARNING "Sanitizers are not supported for compiler ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()
