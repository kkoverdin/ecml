function(ecml_add_sample SAMPLE_NAME SOURCE_FILE)
    add_executable(${SAMPLE_NAME} ${SOURCE_FILE})
    target_link_libraries(${SAMPLE_NAME} PRIVATE ecml)
    set_target_properties(${SAMPLE_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/samples"
    )
endfunction()

ecml_add_sample(ecml_frame_demo frame_demo.cpp)

add_custom_target(samples DEPENDS
    ecml_frame_demo
)
