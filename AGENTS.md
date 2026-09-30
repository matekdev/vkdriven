# AGENTS.md

vkdriven is a Vulkan 1.3 renderer. The plan and current scope are in `docs/roadmap.md`.

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

- `src/main.cpp`: creates `App` and runs it. Load errors come out of `App`'s constructor as exceptions and are printed here.
- `src/app.h/.cpp`: `App` owns the whole renderer and the frame loop. Its members are declared in dependency order, so destruction happens in the right order with no manual cleanup.
- `src/vk/`: thin RAII wrappers over Vulkan objects (`Instance`, `Device`, `Swapchain`, `Buffer`, `Image`, `DeviceHandle<T>` for simple handles, etc.). They expose the raw handles; render code still calls `vkCmd*` directly.
- `src/platform/`: SDL window and the file watcher used for shader hot-reload.
- `src/ui/`: `ImGuiLayer` (ImGui context and SDL3/Vulkan backends, docking enabled) and `ViewportTarget` (the offscreen color/depth images the scene renders into, shown in the ImGui "Viewport" window), and `panels` (free functions that draw each ImGui window).
- `src/util/`: small header-only helpers not tied to Vulkan (e.g. `orThrow` for `std::expected`).
- `src/scene/`: glTF loading (`parseGltf`) and the `Scene`: one shared vertex/index buffer, the primitive and draw lists, and the world-transforms buffer. `Vertex` (with its vertex-input description) lives here too.
- New `.cpp`/`.h` files must be added to `add_executable` in `CMakeLists.txt`.
- `shaders/`: Slang shaders, compiled to SPIR-V at runtime through the Slang API. There's no build-time shader step. CMake copies `shaders/` next to the executable. Debug builds read them from the source tree instead (`VKDRIVEN_SHADER_DIR`) and hot-reload on save or F5.
- `docs/roadmap.md`: phased checklist. Check items off when they're done.

## Dependencies

All from vcpkg (`vcpkg.json`, pinned in `vcpkg-configuration.json`). Vulkan headers, volk, SDL3, VulkanMemoryAllocator, glm, KTX-Software (`ktx`), Slang (`shader-slang`), fastgltf (glTF loading) and stb (`stb_image` for PNG/JPEG). Later phases add meshoptimizer (P4), and ImGui + Tracy (end of P1).

- **volk loads all Vulkan functions.** Link `Vulkan::Headers`, never `Vulkan::Vulkan` (the loader), or the loader's exported symbols clash with volk's function pointers.
- **ImGui with volk:** vcpkg's prebuilt ImGui Vulkan backend links the loader. When ImGui is added, compile `imgui_impl_vulkan.cpp` into the project with `IMGUI_IMPL_VULKAN_USE_VOLK` instead of using the `vulkan-binding` feature.
- Slang's runtime needs its standard modules next to `slang.dll`. `CMakeLists.txt` copies them into `build/<preset>/bin` after every build.
- glm is built with `GLM_FORCE_DEPTH_ZERO_TO_ONE`. The engine uses reverse-Z (How to Vulkan uses standard Z, so depth compare op and clear value differ).
- Slang matrices use column-major layout to match glm.
