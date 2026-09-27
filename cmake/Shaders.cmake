# Compiles .slang files to SPIR-V with slangc from the vcpkg shader-slang package.
# Output: <runtime output dir>/shaders/<name>.spv, one module per file with all of its entry points.

find_program(SLANGC_EXECUTABLE slangc
    HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/shader-slang"
    REQUIRED
)

# Debug info is always on so RenderDoc and Nsight can show shader source.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(VKDRIVEN_SLANG_FLAGS -g2 -O0)
else()
    set(VKDRIVEN_SLANG_FLAGS -g2 -O2)
endif()

function(vkdriven_add_shaders target)
    set(outputs)
    foreach(source IN LISTS ARGN)
        get_filename_component(name "${source}" NAME_WE)
        set(input "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
        set(output "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${name}.spv")

        add_custom_command(
            OUTPUT "${output}"
            COMMAND "${SLANGC_EXECUTABLE}" "${input}"
                -target spirv
                -profile spirv_1_6
                -fvk-use-entrypoint-name
                -matrix-layout-column-major
                ${VKDRIVEN_SLANG_FLAGS}
                -o "${output}"
                -depfile "${output}.d"
            DEPENDS "${input}"
            DEPFILE "${output}.d"
            COMMENT "Compiling shader ${source}"
            VERBATIM
        )
        list(APPEND outputs "${output}")
    endforeach()

    add_custom_target(${target}_shaders DEPENDS ${outputs} SOURCES ${ARGN})
    add_dependencies(${target} ${target}_shaders)
endfunction()
