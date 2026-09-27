# Vast Engine Roadmap

**Current scope: P0 through P5.** P6, P7 and the stretch items are listed for later and aren't part of the current plan.

Vast Engine is a GPU-driven renderer for very large scenes, built on a modern explicit API.

---

## 1. API: Vulkan 1.3+, used the modern way

**Vulkan 1.3** with **dynamic rendering, synchronization2, buffer device address (BDA), and descriptor indexing (bindless)**. Shaders are written in **Slang** and compiled to SPIR-V with `slangc` at build time.

- **Explicit control.** Memory, barriers, queues and pipeline state are managed directly, the same way DX12 and Metal work.
- **Access to modern GPU features:** mesh shaders, hardware ray tracing, bindless resources, indirect draws with a GPU-written draw count, async compute.
- **Slang** has HLSL-like syntax, compiles to SPIR-V, DXIL and Metal, and adds modules and generics.

**Things to avoid:**
- **No RHI or multi-backend abstraction.** The abstraction layer would become the project. Write Vulkan directly. Keep it tidy, but don't abstract it.
- **No Vulkan 1.0-style patterns** (render passes, framebuffer objects, per-draw descriptor sets). Dynamic rendering and bindless from the start.
- **vk-bootstrap to start, as vkguide does.** Replace it with hand-written instance/device/queue setup once P1 works. VMA is used for memory allocation.
- **No volk.** vcpkg's ImGui Vulkan backend links the Vulkan loader directly, and mixing that with volk causes symbol clashes. Extension functions (e.g. mesh shaders) are loaded with `vkGetDeviceProcAddr`.

---

## 2. Scope

A **renderer for very large scenes** (a "massive model viewer"). It loads big glTF scenes or CAD-like assemblies and renders them fast, with the GPU choosing what to draw.

**Renderer only:**
- No editor, ECS, physics, audio or scripting. ImGui debug panels are enough.
- No custom asset format until one is actually needed. Load glTF directly.

---

## 3. Development rules

- [ ] **Short milestones.** Each phase is 3–6 weeks. If a milestone is heading past 8 weeks, cut scope.
- [ ] **Each milestone ends with 4 things:** a git tag, a README screenshot or gif, a short blog post, and **GPU timings**.
- [ ] **Get it working, then generalize.** Hardcode the first version. Only refactor once something is needed a second time.
- [ ] **Measure everything.** Every feature gets a timestamp query and a number in ImGui.
- [ ] **Keep the build green.** CI builds on every push. The validation layers should report zero errors, always.
- [ ] **When stuck, write a smaller test first.** Get the technique working in isolation before integrating it.

---

## 4. Phased checklist

Hour estimates assume ~5 hrs/week. P0 through P4 is about 155h, or roughly **8 months** with blog writing included.

### P0: Setup (~10h, 2 weeks)
- [x] New repo. CMake presets + vcpkg manifest. C++23. Visual Studio 2026.
- [x] Windowing with SDL3
- [x] Packages: vk-bootstrap, VMA, glm, fmt, ImGui, stb, fastgltf, meshoptimizer, Tracy
- [ ] ImGui with the Vulkan backend (needs a device, so done alongside P1)
- [x] Shader compilation: Slang→SPIR-V via `slangc`, run as a build step
- [ ] Validation layers on in debug builds, plus a debug-utils messenger. Give every object a debug name.
- [ ] RenderDoc capture works. Tracy for CPU and GPU profiling zones.
- [ ] GitHub Actions: Windows build (Linux build optional)

### P1: Vulkan core (~40h, 8 weeks). *Milestone: textured Sponza + fly camera*
- [ ] Instance, physical device selection, logical device, queue families (graphics + dedicated compute + transfer), written by hand
- [ ] Swapchain creation, handling window resize/recreation, present modes
- [ ] Frames in flight (2): per-frame command pools/buffers, fences, semaphores
- [ ] **synchronization2**: image layout transitions and pipeline barriers. Every barrier should have a reason you can state.
- [ ] **Dynamic rendering** (no VkRenderPass)
- [ ] Depth buffer with **reverse-Z**
- [ ] Staging-buffer uploads on a transfer queue, with queue-family ownership transfer
- [ ] **Bindless**: one global descriptor set with a large texture array (descriptor indexing) and samplers
- [ ] **Buffer device address**: access vertex/instance/material buffers via pointers in push constants
- [ ] Graphics + compute pipeline creation, with a `VkPipelineCache` saved to disk
- [ ] Shader hot-reload (watch the file, recompile, rebuild the pipeline)
- [ ] Fly camera, ImGui stats (frame time, GPU time)
- [ ] 📝 Blog: "Getting started with Vulkan 1.3: what's actually different"

### P2: Assets & scene (~20h, 4 weeks)
- [ ] glTF 2.0 loading with **fastgltf**
- [ ] Textures: KTX2 with BC7/BC5 compression, full mip chains (use `toktx` or `basisu` offline). Fall back to generating mips at runtime.
- [ ] **One big vertex buffer + one big index buffer** for the whole scene (sets up GPU-driven rendering)
- [ ] Flattened GPU scene data in SSBOs: transforms, materials, and a per-draw array `{meshIndex, materialIndex, transformIndex}`
- [ ] Vertex quantization/compression with meshoptimizer (optional)
- [ ] Test scenes: Sponza, Intel Sponza, Amazon Lumberyard Bistro

### P3: Physically based lighting (~35h, 7 weeks). *Milestone: Bistro with PBR + cascaded shadows*
- [ ] HDR render target (`R16G16B16A16_SFLOAT`) and a separate tonemap pass (ACES or AgX), with exposure control
- [ ] Correct color handling: sRGB textures vs. linear data textures, sRGB swapchain
- [ ] Cook-Torrance BRDF: GGX NDF, Smith geometry term, Schlick Fresnel, metal/roughness workflow
- [ ] Normal mapping (tangents from glTF, or MikkTSpace)
- [ ] Image-based lighting: equirect→cubemap, **compute-shader** irradiance and specular prefiltering, BRDF LUT
- [ ] **Cascaded shadow maps**: cascade splits, texel snapping to stop shimmering, PCF, depth bias and normal offset
- [ ] Point/spot lights with a simple light buffer
- [ ] 📝 Blog: "PBR from Blinn-Phong: what changed and why", with before/after images

### P4: GPU-driven rendering (~50h, 10 weeks). ★ The main feature
- [ ] **Compute frustum culling** per instance → writes a compacted draw list
- [ ] `vkCmdDrawIndexedIndirectCount`: the whole scene in one draw call
- [ ] **Hi-Z depth pyramid** (compute downsample, min/max reduction)
- [ ] **Two-pass occlusion culling**: draw last frame's visible objects, build Hi-Z, cull the rest, draw the newly visible ones
- [ ] **Meshlets** built with meshoptimizer (`meshopt_buildMeshlets`)
- [ ] **Mesh shaders** (`VK_EXT_mesh_shader`): task shader does per-meshlet frustum, occlusion, and **cone backface** culling; mesh shader outputs triangles
- [ ] Fallback path without mesh shaders (meshlet index buffer + indirect draw), so it runs on all hardware
- [ ] Simple LOD (meshoptimizer `simplify`) chosen per instance in the culling shader
- [ ] Stress scene: 100k–1M instances (a procedurally scattered city or forest), or a large CAD-like assembly with heavy instancing
- [ ] ImGui: counts of instances, meshlets, and triangles submitted vs. culled, and time per pass
- [ ] 📝 Blog: "GPU-driven culling: from N draw calls to 1". Include CPU time, GPU time, and triangle counts before and after.

### P5: Profiling case studies (~20h, alongside P3 onward)
- [ ] GPU timestamp queries per pass, and pipeline statistics queries (VS/FS invocations, primitives)
- [ ] Profile with vendor tools on whatever hardware is available: **Nsight Graphics** (NVIDIA), **Radeon GPU Profiler** (AMD), **Intel GPA**
- [ ] Learn to read: occupancy, register/VGPR pressure, memory bandwidth, cache hit rates, warp/wave stalls, overdraw
- [ ] Write 2–3 case studies, each laid out as "this pass was slow → here's the capture → here's why → here's the fix → here's the new number". Candidates:
  - [ ] Culling shader occupancy (changing workgroup size, register usage)
  - [ ] Shadow pass cost vs. cascade count and resolution
  - [ ] Mesh shader path vs. indirect-draw path, on one or more GPUs
- [ ] 📝 Blog: one post per case study

---

> **Current scope ends here.** Everything below is for after P5.

### P6: Ray tracing (~35h, 7 weeks)
- [ ] `VK_KHR_acceleration_structure`: build a BLAS per mesh and a TLAS per frame
- [ ] BLAS **compaction**. TLAS **update/refit** vs. full rebuild, with timings.
- [ ] `VK_KHR_ray_query` in compute/fragment shaders:
  - [ ] Ray-traced shadows, compared against CSM (quality and cost)
  - [ ] Ray-traced ambient occlusion
- [ ] (Optional) A small path-tracing reference mode (ray-tracing pipeline + SBT) to check PBR correctness
- [ ] 📝 Blog: "RT shadows vs. cascaded shadow maps, with numbers"

### P7: Frame structure (~25h, 5 weeks)
- [ ] Lightweight **render graph**: passes declare what they read and write, and barriers and transient resources are handled automatically
- [ ] **Async compute**: run culling or SSAO on the compute queue overlapped with graphics, with timeline semaphores. Measure the overlap.
- [ ] **Clustered forward lighting**: compute light-cluster assignment, supporting 1000+ lights

### Stretch: CAD flavor (pick 1–2)
- [ ] **Camera-relative rendering** for large coordinates (double precision on the CPU, float relative to the camera on the GPU). Show the jitter being fixed.
- [ ] **Edge / silhouette line rendering** (feature edges, hidden-line style)
- [ ] Selection highlight / outline with GPU picking (ID buffer)
- [ ] Section / clipping planes with capping
- [ ] Heavy instancing of repeated parts (bolts, fasteners) with per-instance data

### Stretch: general (pick 1–2)
- [ ] TAA (jitter, reprojection, history clamping) or SMAA
- [ ] Bloom (physically based downsample/upsample)
- [ ] Visibility buffer (a natural follow-on to meshlets)
- [ ] Virtual shadow maps

---

## 5. Timeline (~5 hrs/week)

| Months | Phase | Output |
|---|---|---|
| 0–0.5 | P0 Setup | CI green, triangle on screen |
| 0.5–2.5 | P1 Vulkan core | Textured Sponza, blog post |
| 2.5–3.5 | P2 Assets | Bistro loads, bindless materials |
| 3.5–5 | P3 PBR + CSM | Blog post, screenshots |
| 5–8 | P4 GPU-driven | Blog post |
| 5–9 | P5 Profiling | 2–3 case study posts |

---

## 6. Resources

**Books**
- *Real-Time Rendering, 4th ed.* (Akenine-Möller et al.)
- *Physically Based Rendering* (pbr-book.org, free)
- *GPU Gems / GPU Pro / GPU Zen*: for specific techniques

**Vulkan**
- vkguide.dev: modern Vulkan 1.3 guide (dynamic rendering, BDA)
- Khronos Vulkan-Samples, and Sascha Willems' Vulkan examples
- **niagara** by Arseny Kapoulkine (zeux): GitHub repo plus YouTube streams. Very close to P4.
- Vulkan Guide by Khronos (docs.vulkan.org), especially the synchronization chapters

**GPU architecture & perf**
- "A Trip Through the Graphics Pipeline 2011" by Fabian Giesen
- AMD GPUOpen (RDNA performance guides, RGP docs)
- NVIDIA developer blog: Vulkan do's & don'ts, Nsight tutorials, mesh shader introduction
- "Life of a triangle" (NVIDIA)

**Staying current**
- Jendrik Illner's *Graphics Programming Weekly*
- SIGGRAPH "Advances in Real-Time Rendering" course notes
- Graphics Programming Discord
