# GStreamer × Jetson NVMM — Hardware Contribution Plan

**Modern C++ Plugin Strategy**
March 2026 | Target: GStreamer monorepo + standalone reference plugin | Jetson Xavier & Orin (JetPack 5/6)

---

## 1. The Gap Being Addressed

The problem is structural and well-documented across the NVIDIA developer forums and GStreamer issue tracker.

| The gap | Why it persists |
|---------|----------------|
| The existing `nvcodec` plugin (in `gst-plugins-bad` / monorepo) uses CUDA contexts and OpenGL interop. It targets discrete desktop GPUs. | Jetson's memory model is `NvBufSurface` / NVMM — a physically contiguous, DMA-coherent buffer pool that lives in the Tegra SoC. It is not the same as a CUDA device allocation. The two paths do not compose. |
| Every time someone tries to upgrade GStreamer beyond L4T's pinned version, hardware acceleration breaks completely. | NVIDIA ships `nvvidconv`, `nvv4l2decoder` etc. as proprietary source-drops tied to each L4T BSP release. They patch their Makefiles per JetPack. There is no maintained open-source bridge. |
| Zero-copy NVMM pipelines require manually mapping `NvBufSurface` into `GstMemory` with correct `GstAllocator` semantics. No upstream plugin does this correctly. | `NvBufSurfaceCreate`, `NvBufSurfTransform`, and `NvSciSyncFence` interop are underdocumented and not abstracted in any open plugin. Each team reinvents the wheel. |

### Why we are positioned to solve this

We have already built the hard parts in ROCX: `GstBaseSink` with NVMM, `NvBufSurfTransform` for crop/scale, custom `GstMeta` propagation across plugin boundaries, and shared-memory output — all tested on Xavier and Orin.

---

## 2. Two Parallel Contribution Tracks

| Track | Target repo | Language | Visibility | Timeline |
|-------|------------|----------|-----------|----------|
| A — Upstream | `gitlab.freedesktop.org/gstreamer/gstreamer` | C (ABI) + C++17 internals | High — merged into monorepo | 6–18 months |
| B — Standalone | `github.com/PavelGuzenfeld/gst-nvmm-cpp` | C++17 throughout | Community / robotics niche | 1–3 months to v0.1 |

---

## 3. Track A — Upstream GStreamer Contributions

### 3.1 Infrastructure & Orientation

All GStreamer development since September 2021 happens in a single monorepo. Submit issues and merge requests at `gitlab.freedesktop.org/gstreamer/gstreamer`. New plugins submitted via MR land in `subprojects/gst-plugins-bad/` and follow the Meson build system.

**Account requirement:** GitLab accounts on freedesktop.org require User Verification template.

### 3.2 Study Material — Existing nvcodec Code

| File | What to learn |
|------|--------------|
| `sys/nvcodec/gstnvdec.c` | How NVDEC output surfaces map to `GstBuffer` via `GstCudaMemory`. The allocator pattern translates directly to `NvBufSurface`. |
| `sys/nvcodec/gstnvbaseenc.c` | Encoder base class — caps negotiation, buffer import, rate control properties. |
| `tests/examples/nvcodec/nvcodec.c` | Integration test patterns used by maintainers. |
| `sys/nvcodec/gstcudacontext.c` | `GstCudaContext` lifecycle — understand what you are NOT doing with `NvBufSurface`. |

### 3.3 Contribution Targets (in order)

**Step A1 — File issues for known gaps**

1. `nvcodec` has no Tegra/NVMM allocator path
2. No `GstAllocator` wrapper for `NvBufSurface`
3. `NvBufSurfTransform` has no GStreamer element

**Step A2 — GstNvmmAllocator** (Medium effort, high upstream value)

A `GstAllocator` subclass wrapping `NvBufSurfaceCreate/Destroy` with `GstMemory` semantics, correct map/unmap for CPU access, and DMA-buf export for interop.

**Step A3 — gst-nvmmconvert element** (High effort, high impact)

A `GstBaseTransform` subclass wrapping `NvBufSurfTransform` — the Tegra VIC for crop, scale, and format conversion entirely within NVMM memory.

**Step A4 — NvSciBuf / NvSciSync integration** (JP6 only, advanced)

---

## 4. Track B — Standalone Reference Plugin: gst-nvmm-cpp

### 4.1 Repository Structure

```
gst/nvmmalloc/     — GstNvmmAllocator core memory abstraction
gst/nvmmconvert/   — GstNvmmConvert (GstBaseTransform + NvBufSurfTransform)
gst/nvmmsink/      — Shared-memory sink, DMA-buf export
gst/nvmmappsrc/    — Push NVMM buffers from C++ app into pipeline
tests/             — CTest-based unit tests per element
benchmarks/        — Latency and throughput benchmarks
docker/            — Dockerfiles for Xavier (L4T 35.x) and Orin (L4T 36.x)
docs/              — Design doc, JetPack version matrix
```

### 4.2 C++ Design Principles

The ABI boundary to GStreamer is C (`plugin_init`, element factory, GObject type system). Inside that boundary, everything is C++17.

| Pattern | Rationale |
|---------|-----------|
| RAII wrappers for `NvBufSurface` | `NvmmBuffer` class owns the `NvBufSurface*`, calls `NvBufSurfaceDestroy` in destructor. |
| `std::expected` for error paths | Return `std::expected<NvmmBuffer, NvmmError>` internally. Convert to `GError` at GStreamer boundary. |
| Template allocator policy | `NvmmAllocatorPolicy<MemType>` selects memory type at build time. No runtime branching in hot path. |
| `std::span`-based buffer views | `std::span<uint8_t>` for CPU-mapped plane access. No raw pointer arithmetic exposed. |
| Minimal GLib macro usage | `G_BEGIN_DECLS` / `G_END_DECLS` only at plugin_init boundary. All logic in `.cpp` files in namespaces. |

---

## 5. Execution Sequence

| Phase | Duration | Deliverable | Track |
|-------|----------|------------|-------|
| 1 | Week 1 | Account setup, clone monorepo, build nvcodec from source on Orin | A |
| 2 | Weeks 2–3 | Study nvcodec source, write design doc, file gap issues | A |
| 3 | Weeks 3–6 | Implement GstNvmmAllocator in standalone repo, unit tests, benchmarks | B |
| 4 | Weeks 6–8 | Add GstNvmmConvert, test with real pipelines, write benchmarks | B |
| 5 | Week 8 | Publish v0.1 on GitHub, post to NVIDIA forum and GStreamer Discourse | B |
| 6 | Months 3–4 | Propose GstNvmmAllocator as MR to monorepo | A |
| 7 | Months 4–6 | Iterate on feedback, propose GstNvmmConvert, start JP6/NvSciBuf work | A + B |
| 8 | Month 6+ | Propose moving into `gst-plugins-bad/sys/nvmm` | A |

---

## 6. Reference Links

| Resource | URL |
|----------|-----|
| GStreamer monorepo | `gitlab.freedesktop.org/gstreamer/gstreamer` |
| GStreamer contributing guide | `gstreamer.freedesktop.org/documentation/contribute` |
| nvcodec source (GitHub mirror) | `github.com/GStreamer/gst-plugins-bad/tree/master/sys/nvcodec` |
| Basler gst-plugin-pylon (C++ reference) | `github.com/basler/gst-plugin-pylon` |
| L4T Accelerated GStreamer docs | `docs.nvidia.com/jetson/.../AcceleratedGstreamer.html` |
| Jetson Linux archive | `developer.nvidia.com/embedded/jetson-linux-archive` |
| DeepStream custom plugin docs | `docs.nvidia.com/metropolis/deepstream/.../DS_sample_custom_gstream.html` |
| NvBufSurface zero-copy forum thread | `forums.developer.nvidia.com/t/...339246` |

---

## 7. Filed Issues (freedesktop.org GitLab)

| # | Issue | URL |
|---|-------|-----|
| 1 | nvcodec: No Tegra/NVMM allocator path | https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/4979 |
| 2 | Missing GstAllocator wrapper for NvBufSurface | https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/4980 |
| 3 | NvBufSurfTransform has no GStreamer element | https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/4981 |

## 8. Repos

| Repo | URL |
|------|-----|
| Standalone plugin (Track B) | https://github.com/PavelGuzenfeld/gst-nvmm-cpp |
| GStreamer fork (Track A) | https://github.com/PavelGuzenfeld/gstreamer (branch: `nvmm-jetson-plan`) |
