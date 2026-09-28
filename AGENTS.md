# AGENTS.md

vkdriven is a GPU-driven Vulkan 1.3 renderer for very large scenes. The plan and current scope are in `docs/roadmap.md`.

The Vulkan core follows [How to Vulkan](https://www.howtovulkan.com/) (source: https://github.com/SaschaWillems/HowToVulkan). When explaining or reviewing P1 work, point to the matching chapter and stay consistent with its approach, apart from the project's own choices (C++23, vcpkg, reverse-Z).

## Code style

- No comments in the code you generate unless absolutely necessary, leave any existing comments intact. Write clean, self-documenting code.
- Use early returns where possible.
- Keep functions small and focused.
- Keep functions small and focused, but don't split code up early. Keeping things in one function (even `main`) is fine until there's a real reason to extract. Don't over-engineer.
- Use descriptive variable and function names.
- C++23 on MSVC (`CMAKE_CXX_STANDARD 23`). Only drop to C++20 if a dependency genuinely fails to build with C++23.
- Always use modern C++ where possible:
  - RAII for every resource. No owning raw pointers, no manual `new`/`delete`.
  - `std::span` for views over contiguous data, `std::optional` for values that may be absent, `std::expected` for fallible operations.
  - Designated initializers for Vulkan create-info structs.
  - `enum class`, `constexpr`, structured bindings, ranges and range-based `for` where they make the code clearer.
- Warnings at `/W4` should stay clean in project code.

## Git

- **Never commit or push.** Leave all changes uncommitted so the owner can review them first. Only commit when the owner explicitly asks for that specific change.

## What AI may write

This is a learning project. The owner writes the core graphics code themselves.

- **Write freely:** build files (CMake, vcpkg, CI), utilities (logging, file watching, asset-loading plumbing), ImGui panels.
- **Explain and review only, don't write:** Vulkan setup (instance, device, swapchain), synchronization and barriers, descriptors / bindless / buffer device address, all shaders, culling and GPU-driven rendering code.
- If a request touches the second group, explain the concept or review the owner's code instead of producing the implementation, unless the owner explicitly asks for code.

## Build

Windows only. Requires Visual Studio 2026 with the C++ workload (MSVC, CMake, Ninja, vcpkg) and the Vulkan SDK for validation layers.

From a Developer PowerShell for VS:

```
cmake --preset debug
cmake --build --preset debug
build\debug\bin\vkdriven.exe
```

Presets: `debug` and `release` (RelWithDebInfo). vcpkg installs all dependencies on the first configure. CI (`.github/workflows/build.yml`) builds both presets on every push to `main`.

## Layout

- `src/`: engine source. Currently a single `main.cpp` (SDL window + event loop), like How to Vulkan. New `.cpp` files must be added to `add_executable` in `CMakeLists.txt`.
- `shaders/` (not created yet): Slang shaders, compiled to SPIR-V at runtime through the Slang API. There's no build-time shader step. When the first shader is added, CMake should copy `shaders/` next to the executable.
- `docs/roadmap.md`: phased checklist. Check items off when they're done.

## Dependencies

All from vcpkg (`vcpkg.json`, pinned in `vcpkg-configuration.json`). It's the same set How to Vulkan uses: Vulkan headers, volk, SDL3, VulkanMemoryAllocator, glm, tinyobjloader, KTX-Software (`ktx`), Slang (`shader-slang`). Later phases add fastgltf (P2), meshoptimizer (P4), and ImGui + Tracy (end of P1).

- **volk loads all Vulkan functions.** Link `Vulkan::Headers`, never `Vulkan::Vulkan` (the loader), or the loader's exported symbols clash with volk's function pointers.
- **ImGui with volk:** vcpkg's prebuilt ImGui Vulkan backend links the loader. When ImGui is added, compile `imgui_impl_vulkan.cpp` into the project with `IMGUI_IMPL_VULKAN_USE_VOLK` instead of using the `vulkan-binding` feature.
- Slang's runtime needs its standard modules next to `slang.dll`. `CMakeLists.txt` copies them into `build/<preset>/bin` after every build.
- glm is built with `GLM_FORCE_DEPTH_ZERO_TO_ONE`. The engine uses reverse-Z (How to Vulkan uses standard Z, so depth compare op and clear value differ).
- Slang matrices use column-major layout to match glm.
