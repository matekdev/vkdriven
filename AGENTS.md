# AGENTS.md

Vast Engine is a GPU-driven Vulkan 1.3 renderer for very large scenes. The plan and current scope are in `docs/roadmap.md`.

## Code style

- No comments in the code you generate unless absolutely necessary, leave any existing comments intact. Write clean, self-documenting code.
- Use early returns where possible.
- Keep functions small and focused.
- Use descriptive variable and function names.
- C++23, MSVC. Warnings at `/W4` should stay clean in project code.

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
build\debug\bin\vast.exe
```

Presets: `debug` and `release` (RelWithDebInfo). vcpkg installs all dependencies on the first configure. CI (`.github/workflows/build.yml`) builds both presets on every push to `main`.

## Layout

- `src/`: engine source. New `.cpp` files must be added to `add_executable` in `CMakeLists.txt`.
- `shaders/`: Slang shaders. New files must be added to `vast_add_shaders` in `CMakeLists.txt`.
- `cmake/Shaders.cmake`: compiles each `.slang` file with `slangc` into one SPIR-V module per file at `build/<preset>/bin/shaders/<name>.spv`, keeping entry point names (`-fvk-use-entrypoint-name`).
- `docs/roadmap.md`: phased checklist. Check items off when they're done.

## Dependencies

All from vcpkg (`vcpkg.json`, pinned in `vcpkg-configuration.json`): Vulkan loader + headers, SDL3, vk-bootstrap, VulkanMemoryAllocator, glm, fmt, ImGui (SDL3 + Vulkan backends), stb, fastgltf, meshoptimizer, Tracy, Slang.

- **Don't add volk.** vcpkg's ImGui Vulkan backend links the Vulkan loader directly, and volk's symbols clash with it. Load extension functions with `vkGetDeviceProcAddr`.
- glm is built with `GLM_FORCE_DEPTH_ZERO_TO_ONE`. The engine uses reverse-Z.
- Slang matrices use column-major layout to match glm.
- The Tracy profiler app must match the vcpkg Tracy version.
