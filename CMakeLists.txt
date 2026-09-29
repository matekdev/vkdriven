cmake_minimum_required(VERSION 3.28)

project(vkdriven LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The executable, runtime DLLs and Slang runtime modules all land in build/<preset>/bin.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanUtilityLibraries CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)
find_package(slang CONFIG REQUIRED)

add_executable(vkdriven
    src/main.cpp
)

target_link_libraries(vkdriven PRIVATE
    Vulkan::Headers
    Vulkan::UtilityHeaders
    volk::volk
    SDL3::SDL3
    GPUOpen::VulkanMemoryAllocator
    glm::glm
    tinyobjloader::tinyobjloader
    KTX::ktx
    slang::slang
)

target_compile_definitions(vkdriven PRIVATE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(vkdriven PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()

# Slang loads its standard modules from beside slang.dll at runtime, and vcpkg's applocal step doesn't copy them.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(VKDRIVEN_VCPKG_BIN "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")
else()
    set(VKDRIVEN_VCPKG_BIN "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
endif()
file(GLOB VKDRIVEN_SLANG_MODULES "${VKDRIVEN_VCPKG_BIN}/slang-standard-module-*" "${VKDRIVEN_VCPKG_BIN}/slang.slang")
if(VKDRIVEN_SLANG_MODULES)
    add_custom_command(TARGET vkdriven POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VKDRIVEN_SLANG_MODULES} "$<TARGET_FILE_DIR:vkdriven>"
        VERBATIM
    )
endif()
