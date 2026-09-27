cmake_minimum_required(VERSION 3.28)

project(vkdriven LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The executable, runtime DLLs and compiled shaders all land in build/<preset>/bin.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Vulkan REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)

include(cmake/Shaders.cmake)

add_executable(vkdriven
    src/main.cpp
)

target_include_directories(vkdriven PRIVATE src ${Stb_INCLUDE_DIR})

target_link_libraries(vkdriven PRIVATE
    Vulkan::Vulkan
    SDL3::SDL3
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    glm::glm
    fmt::fmt
    imgui::imgui
    fastgltf::fastgltf
    meshoptimizer::meshoptimizer
    Tracy::TracyClient
)

target_compile_definitions(vkdriven PRIVATE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(vkdriven PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()

vkdriven_add_shaders(vkdriven
    shaders/triangle.slang
)
