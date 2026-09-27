# Vast Engine

A GPU-driven Vulkan 1.3 renderer for very large scenes.

The goal is to render huge scenes efficiently by moving culling and draw submission onto the GPU, with profiling and performance numbers for every feature.

## Status

🚧 Early development. See the [roadmap](docs/roadmap.md).

## Planned features

- Vulkan 1.3 with dynamic rendering, synchronization2, buffer device address and bindless descriptors
- HLSL shaders compiled to SPIR-V
- glTF 2.0 scenes with KTX2 compressed textures
- PBR lighting, image-based lighting and cascaded shadow maps
- GPU-driven rendering: compute culling, one indirect draw call for the whole scene, two-pass Hi-Z occlusion culling
- Meshlets with task/mesh shaders, plus a fallback path without mesh shaders
- Per-pass GPU timings, with profiling write-ups
