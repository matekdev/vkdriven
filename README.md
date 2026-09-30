# vkdriven

A GPU-driven Vulkan 1.3 renderer for very large scenes.

The goal is to render huge scenes efficiently by moving culling and draw submission onto the GPU, with profiling and performance numbers for every feature.

## Status

🚧 Early development. See the [roadmap](docs/roadmap.md).

## Building

**Requirements (Windows):**
- Visual Studio 2026 with the **Desktop development with C++** workload. It includes MSVC, CMake, Ninja and vcpkg.
- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home). Needed at runtime for the validation layers.

All libraries come from vcpkg and are installed on the first configure. The Vulkan core follows [How to Vulkan](https://www.howtovulkan.com/): SDL3, volk, VMA, glm, KTX-Software and Slang, plus fastgltf and stb for loading glTF scenes.

**Visual Studio:** File → Open → Folder, pick the repo, then choose the `Debug` or `Release` preset.

**Command line** (Developer PowerShell for VS):

```
cmake --preset debug
cmake --build --preset debug
build\debug\bin\vkdriven.exe
```

## Planned features

- Vulkan 1.3 with dynamic rendering, synchronization2, buffer device address and bindless descriptors
- Slang shaders compiled to SPIR-V at runtime
- glTF 2.0 scenes with KTX2 compressed textures
- PBR lighting, image-based lighting and cascaded shadow maps
- GPU-driven rendering: compute culling, one indirect draw call for the whole scene, two-pass Hi-Z occlusion culling
- Meshlets with task/mesh shaders, plus a fallback path without mesh shaders
- Per-pass GPU timings, with profiling write-ups
