# vkdriven Roadmap

**Current scope: P0 through P4.** P5 and the stretch items are listed for later and aren't part of the current plan.

**Focus: breadth over depth.** Each phase covers standard real-time techniques that come up in graphics interviews and take a week or two each to build. Performance work (GPU-driven pipelines, culling, meshlets, profiling case studies) is out of scope.

vkdriven is a renderer built on a modern explicit API.

---

## 1. API: Vulkan 1.3+, used the modern way

**Vulkan 1.3** with **dynamic rendering, synchronization2, buffer device address (BDA), and descriptor indexing (bindless)**. Shaders are written in **Slang** and compiled to SPIR-V at runtime through the Slang API.

**Primary reference: [How to Vulkan](https://www.howtovulkan.com/)** by Sascha Willems ([source](https://github.com/SaschaWillems/HowToVulkan)). It builds a textured, lit mesh renderer on exactly this feature set in a single `main.cpp`, using the same libraries as this project: SDL3, volk, VMA, glm, tinyobjloader, KTX-Software and Slang. P1 follows it chapter by chapter, with the code written by hand rather than copied.

- **Explicit control.** Memory, barriers, queues and pipeline state are managed directly, the same way DX12 and Metal work.
- **Access to modern GPU features:** mesh shaders, hardware ray tracing, bindless resources, indirect draws with a GPU-written draw count, async compute.
- **Slang** has HLSL-like syntax, compiles to SPIR-V, DXIL and Metal, and adds modules and generics.

**Things to avoid:**
- **No RHI or multi-backend abstraction.** The abstraction layer would become the project. Write Vulkan directly. Keep it tidy, but don't abstract it.
- **No Vulkan 1.0-style patterns** (render passes, framebuffer objects, per-draw descriptor sets). Dynamic rendering and bindless from the start.
- **No vk-bootstrap.** Instance, device and queue setup are written by hand from the start, as How to Vulkan does. VMA is used for memory allocation.
- **volk loads all Vulkan functions**, including extensions (e.g. mesh shaders). The project doesn't link the Vulkan loader. When ImGui is added, its Vulkan backend has to be built with `IMGUI_IMPL_VULKAN_USE_VOLK` (or `IMGUI_IMPL_VULKAN_NO_PROTOTYPES`), because vcpkg's prebuilt backend links the loader and clashes with volk.

---

## 2. Scope

A **renderer**: it loads glTF models and scenes and draws them with modern Vulkan.

**Renderer only:**
- No editor, ECS, physics, audio or scripting. ImGui debug panels are enough.
- No custom asset format until one is actually needed. Load glTF directly.

---

## 3. Development rules

- [ ] **Short milestones.** Each phase is 3–6 weeks. If a milestone is heading past 8 weeks, cut scope.
- [ ] **Get it working, then generalize.** Hardcode the first version. Only refactor once something is needed a second time.
- [ ] **Keep the build green.** CI builds on every push. The validation layers should report zero errors, always.
- [ ] **When stuck, write a smaller test first.** Get the technique working in isolation before integrating it.

---

## 4. Phased checklist

Hour estimates assume ~5 hrs/week. P0 through P4 is about 145h, or roughly **7 months** with blog writing included.

### P0: Setup (~10h, 2 weeks)
- [x] New repo. CMake presets + vcpkg manifest. C++23. Visual Studio 2026.
- [x] Windowing with SDL3: a window that opens and runs an event loop
- [x] Packages (the How to Vulkan set): volk, VMA, glm, SDL3, tinyobjloader, KTX-Software, Slang
- [x] Validation layers on in debug builds, plus a debug-utils messenger
- [x] RenderDoc capture works
- [x] GitHub Actions: Windows build (Linux build optional)

### P1: Vulkan core, following How to Vulkan (~40h, 8 weeks). *Milestone: the tutorial's textured, lit mesh, then textured Sponza + fly camera*

Work through [How to Vulkan](https://www.howtovulkan.com/) in order. Each item names the chapter it comes from.

- [x] Instance setup with volk (*Instance setup*)
- [x] Physical device selection, queue families, logical device with the 1.3 features enabled (*Device selection*, *Queues*, *Device setup*)
- [x] VMA allocator (*Setting up VMA*)
- [x] SDL3 surface and swapchain (*Window and surface*, *Swapchain*)
- [x] Depth attachment, switched to **reverse-Z** (*Depth attachment*)
- [x] Load an OBJ mesh with tinyobjloader into a VMA buffer (*Loading meshes*)
- [x] Frames in flight (2): per-frame shader data buffers, fences, semaphores, command buffers (*CPU and GPU parallelism*, *Shader data buffers*, *Synchronization objects*, *Command buffers*)
- [x] **Buffer device address**: shader data reached through a pointer in push constants (*Shader data buffers*)
- [x] KTX textures via KTX-Software, uploaded through a staging buffer (*Loading textures*)
- [x] **Bindless**: one descriptor set with a variable-count texture array (descriptor indexing) (*Loading textures*)
- [x] Runtime Slang compilation to SPIR-V through the Slang API (*Loading shaders*, *The shader*)
- [x] Graphics pipeline with **dynamic rendering** (no VkRenderPass) (*Graphics pipeline*)
- [x] Render loop with **synchronization2** barriers, and swapchain recreation on resize. Every barrier should have a reason you can state. (*Render loop*)
- [x] Clean shutdown with zero validation errors (*Cleaning up*)
- [x] **Tutorial complete.** Tag it.
- [x] Beyond the tutorial: shader hot-reload (watch the file, recompile with the Slang API, rebuild the pipeline). Runtime compilation makes this easy.
- [x] Add ImGui (backend built against volk), with a stats panel
- [x] Fly camera
- [x] 📝 Blog: "Getting started with Vulkan 1.3: what's actually different"

### P2: Assets & scene (~20h, 4 weeks)
- [x] Add **fastgltf** and move from OBJ (tinyobjloader) to glTF 2.0 loading
- [x] Textures: glTF PNG/JPEG decoded with stb_image, full mip chains generated on the GPU
- [x] **One big vertex buffer + one big index buffer** for the whole scene (sets up GPU-driven rendering)
- [x] Flattened GPU scene data in SSBOs: transforms, materials, and a per-draw array `{meshIndex, materialIndex, transformIndex}`
- [x] Test scene: Sponza

### P3: Physically based lighting (~35h, 7 weeks). *Milestone: Sponza with PBR + cascaded shadows*

Follow [LearnOpenGL](https://learnopengl.com/)'s *Advanced Lighting* and *PBR* chapters by Joey de Vries, in the order below. Each item names the chapter it comes from. The tutorial is OpenGL, so translate as you go: framebuffers become dynamic rendering attachments, uniforms become the `FrameData`/material buffers reached through BDA, texture binds become bindless indices, and every pass boundary needs a barrier you can explain. Keep reverse-Z and Slang.

- [x] Before starting: tag P2 and take a "before" screenshot of the Lambert Sponza for the blog post
- [x] Correct color handling: sRGB textures vs. linear data textures (metallic-roughness and normal maps load as `UNORM`), sRGB output (*Gamma Correction*)
- [x] HDR render target (`R16G16B16A16_SFLOAT`) and a separate fullscreen tonemap pass with exposure control in ImGui (*HDR*). The chapter uses Reinhard/exposure; add ACES or AgX afterwards.
- [x] Read the theory: microfacets, energy conservation, the reflectance equation, metal/roughness workflow (*PBR → Theory*)
- [x] Cook-Torrance BRDF: GGX NDF, Smith geometry term, Schlick Fresnel. Material gains metallic/roughness factors and the glTF metallic-roughness texture (blue = metallic, green = roughness); `FrameData` gains camera position and light color × intensity (*PBR → Lighting*)
- [x] Normal mapping with the glTF `TANGENT` attribute (Sponza ships it) and a flat-normal default texture (*Normal Mapping*)
- [x] Compute pipelines and a `VkPipelineCache` saved to disk (not in the tutorial)
- [x] Shadow mapping basics: single directional shadow map, depth bias, PCF (*Shadows → Shadow Mapping*). Remember reverse-Z flips the comparison.
- [x] **Cascaded shadow maps**: cascade splits, a layered depth image, cascade selection (*Guest Articles → Cascaded Shadow Mapping*). Beyond the tutorial: texel snapping to stop shimmering, and normal-offset bias.
- [x] GPU timestamp queries per pass

### P4: Screen-space effects & post-processing (~40h, 8 weeks). *Milestone: Sponza with SSAO, bloom and TAA, each toggleable in ImGui*

Each effect is a compute or fullscreen pass over the HDR target, depth, and the prepass outputs. Give each one an ImGui toggle and a debug view, so you can show before and after.

- [ ] **Depth + normal prepass**: draws the opaque scene once, writing depth, view-space normals and **motion vectors** (current vs. previous clip-space position). The main pass then tests depth with `EQUAL` and no writes. This feeds SSAO and TAA without moving to deferred.
- [ ] **SSAO**: hemisphere kernel, rotation noise, blur, computed after the prepass and applied to the ambient term in the main pass (*SSAO*)
- [ ] **Bloom**: physically based downsample/upsample chain (*Guest Articles → Phys. Based Bloom*)
- [ ] **FXAA** as the simple post-process AA baseline
- [ ] **TAA**: sub-pixel jitter (Halton), reprojection with motion vectors, neighborhood clamping of the history, then sharpening. Compare against FXAA.
- [ ] Exponential distance/height fog, applied from depth
- [ ] (Optional) **SSR**: screen-space reflections by ray marching the depth buffer
- [ ] (Optional) Color grading with a 3D LUT in the tonemap pass
- [ ] **Transparency**: a second forward pass for alpha-blended materials (glTF `BLEND`), sorted back to front, depth-tested against the opaque depth without writing it (*Blending*)
- [ ] 📝 Blog: "TAA from scratch: jitter, reprojection and ghosting"

---

> **Current scope ends here.** Everything below is for after P4.

### P5: Ray tracing (~25h, 5 weeks)
- [ ] `VK_KHR_acceleration_structure`: build a BLAS per mesh and a TLAS
- [ ] `VK_KHR_ray_query` in compute/fragment shaders:
  - [ ] Ray-traced shadows, compared against CSM
  - [ ] Ray-traced ambient occlusion, compared against SSAO
- [ ] (Optional) A small path-tracing reference mode to check PBR correctness
- [ ] 📝 Blog: "RT shadows vs. cascaded shadow maps"

### Stretch (pick 1–2)
- [ ] Lightweight **render graph**: passes declare what they read and write, and barriers and transient resources are handled automatically
- [ ] Order-independent transparency (weighted blended OIT)
- [ ] Projected decals
- [ ] Volumetric light / god rays by ray marching the shadow map
- [ ] Compute particles
- [ ] Camera-relative rendering for large coordinates

---

## 5. Timeline (~5 hrs/week)

| Months | Phase | Output |
|---|---|---|
| 0–0.5 | P0 Setup | CI green, SDL window, validation layers |
| 0.5–2.5 | P1 Vulkan core | How to Vulkan finished, then textured Sponza, blog post |
| 2.5–3.5 | P2 Assets | Sponza loads, bindless materials |
| 3.5–5 | P3 PBR + CSM | Blog post, screenshots |
| 5–7 | P4 Screen-space + post | Blog post, before/after screenshots |

---

## 6. Resources

**Books**
- *Real-Time Rendering, 4th ed.* (Akenine-Möller et al.)
- *Physically Based Rendering* (pbr-book.org, free)
- *GPU Gems / GPU Pro / GPU Zen*: for specific techniques

**Vulkan**
- **[How to Vulkan](https://www.howtovulkan.com/)** by Sascha Willems: the primary reference for P1. Modern Vulkan 1.3 in one file, same libraries as this project ([source](https://github.com/SaschaWillems/HowToVulkan)).
- **[LearnOpenGL](https://learnopengl.com/)** by Joey de Vries: the primary reference for P3–P4 (Gamma Correction, HDR, Normal Mapping, Shadow Mapping, PBR, Cascaded Shadow Mapping, Blending, SSAO, Phys. Based Bloom). OpenGL, but the lighting and the math carry over directly.
- vkguide.dev: a second take on Vulkan 1.3 (dynamic rendering, BDA), useful for engine structure after P1
- Khronos Vulkan-Samples, and Sascha Willems' Vulkan examples
- Vulkan Guide by Khronos (docs.vulkan.org), especially the synchronization chapters

**Techniques**
- Brian Karis, "High Quality Temporal Supersampling" (SIGGRAPH 2014), and Yang et al., "A Survey of Temporal Antialiasing Techniques" (2020): TAA
- Jorge Jimenez, "Next Generation Post Processing in Call of Duty: Advanced Warfare" (SIGGRAPH 2014): bloom
- Timothy Lottes, FXAA 3.11 whitepaper

**GPU architecture** (common interview ground)
- "A Trip Through the Graphics Pipeline 2011" by Fabian Giesen
- "Life of a triangle" (NVIDIA)

**Staying current**
- Jendrik Illner's *Graphics Programming Weekly*
- SIGGRAPH "Advances in Real-Time Rendering" course notes
- Graphics Programming Discord
