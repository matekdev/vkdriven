# vkdriven

A Vulkan 1.3 renderer, built up one feature at a time.

More details, screenshots and write-ups are on the [project page](https://matek.dev/projects/vkdriven/).

## Status

🚧 Early development. See the [roadmap](docs/roadmap.md).

## Building

**Requirements (Windows):**
- Visual Studio 2026 with the **Desktop development with C++** workload. It includes MSVC, CMake, Ninja and vcpkg.
- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home). Needed at runtime for the validation layers.

All libraries come from vcpkg and are installed on the first configure. The Vulkan core follows [How to Vulkan](https://www.howtovulkan.com/): SDL3, volk, VMA, glm and Slang, plus fastgltf and stb for loading glTF scenes, and ImGui for the debug UI.

**Visual Studio:** File → Open → Folder, pick the repo, then choose the `Debug` or `Release` preset.

**Command line** (Developer PowerShell for VS):

```
cmake --preset debug
cmake --build --preset debug
build\debug\bin\vkdriven.exe
```
