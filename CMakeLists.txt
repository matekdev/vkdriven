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
find_package(Ktx CONFIG REQUIRED)
find_package(slang CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(imgui CONFIG REQUIRED)

# vcpkg's prebuilt ImGui Vulkan backend links the Vulkan loader, which clashes with volk. Build the backend from
# the matching upstream release against volk instead. Keep the tag in sync with the imgui version (and docking feature) vcpkg installs.
include(FetchContent)
FetchContent_Declare(imgui_backends
    URL https://github.com/ocornut/imgui/archive/v1.92.9b-docking.tar.gz
    URL_HASH SHA512=7eddcdb475f1db1fc8242d918533b955c964d2267abe713bdf23f8e2444770946d3c79c7855e360bab6168e36231b95bd05a84106c08f876dcd53daac9caccac
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(imgui_backends)

add_library(imgui_impl_vulkan STATIC "${imgui_backends_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp")
target_include_directories(imgui_impl_vulkan PUBLIC "${imgui_backends_SOURCE_DIR}/backends")
target_compile_definitions(imgui_impl_vulkan PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)
target_link_libraries(imgui_impl_vulkan PUBLIC imgui::imgui Vulkan::Headers volk::volk)

# Icon codepoint macros (ICON_FA_*) for assets/fonts/fa-solid-900.ttf. Not in vcpkg; header-only.
FetchContent_Declare(icon_font_headers
    URL https://github.com/juliettef/IconFontCppHeaders/archive/210b5a399a64270674560d633638952d1e8d804d.tar.gz
    URL_HASH SHA256=aae5fd355521f62a0257064f40bab5676aee93c9fa9dd7fd53375d3594933b3f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(icon_font_headers)

add_executable(vkdriven
    src/app.cpp
    src/app.h
    src/main.cpp
    src/platform/file_watcher.cpp
    src/platform/file_watcher.h
    src/platform/window.cpp
    src/platform/window.h
    src/render/scene_pass.cpp
    src/render/scene_pass.h
    src/scene/camera.cpp
    src/scene/camera.h
    src/scene/gltf.cpp
    src/scene/gltf.h
    src/scene/scene.cpp
    src/scene/scene.h
    src/scene/vertex.h
    src/ui/imgui_layer.cpp
    src/ui/imgui_layer.h
    src/ui/imgui_style.cpp
    src/ui/imgui_style.h
    src/ui/panels.cpp
    src/ui/panels.h
    src/ui/viewport_target.cpp
    src/ui/viewport_target.h
    src/vk/allocator.cpp
    src/vk/allocator.h
    src/vk/bindless_textures.cpp
    src/vk/bindless_textures.h
    src/vk/buffer.cpp
    src/vk/buffer.h
    src/vk/check.h
    src/vk/command_pool.cpp
    src/vk/command_pool.h
    src/vk/device.cpp
    src/vk/device.h
    src/vk/frame_resources.cpp
    src/vk/frame_resources.h
    src/vk/graphics_pipeline.cpp
    src/vk/graphics_pipeline.h
    src/vk/handle.h
    src/vk/image.cpp
    src/vk/image.h
    src/vk/instance.cpp
    src/vk/instance.h
    src/vk/shader_compiler.cpp
    src/vk/shader_compiler.h
    src/vk/surface.cpp
    src/vk/surface.h
    src/vk/swapchain.cpp
    src/vk/swapchain.h
    src/vk/sync.h
    src/vk/texture.cpp
    src/vk/texture.h
    src/vk/upload.cpp
    src/vk/upload.h
    src/util/expected.h
)

target_include_directories(vkdriven PRIVATE src)

target_link_libraries(vkdriven PRIVATE
    Vulkan::Headers
    Vulkan::UtilityHeaders
    volk::volk
    SDL3::SDL3
    GPUOpen::VulkanMemoryAllocator
    glm::glm
    KTX::ktx
    slang::slang
    fastgltf::fastgltf
    imgui::imgui
    imgui_impl_vulkan
)

target_include_directories(vkdriven SYSTEM PRIVATE ${Stb_INCLUDE_DIR} ${icon_font_headers_SOURCE_DIR})

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

add_custom_command(TARGET vkdriven POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:vkdriven>/assets"
    VERBATIM
)

# Debug builds read shaders straight from the source tree so edits hot-reload without a rebuild.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(vkdriven PRIVATE VKDRIVEN_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders")
else()
    target_compile_definitions(vkdriven PRIVATE VKDRIVEN_SHADER_DIR="shaders")
endif()

# Shaders are compiled at runtime, so copy them on every build even when the executable doesn't relink.
add_custom_target(vkdriven_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:vkdriven>/shaders"
    VERBATIM
)
add_dependencies(vkdriven vkdriven_shaders)
