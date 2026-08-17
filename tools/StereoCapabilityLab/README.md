# StereoCapabilityLab

StereoCapabilityLab is a standalone, offscreen Direct3D 11 experiment for one
question: on the target adapter and driver, can standard D3D11 mechanisms replace
two eye submissions with a correct, cheaper geometry path?

It does not load Skyrim, SKSE, Community Shaders, OpenComposite, Pimax software,
DLSS, NVAPI, or the Governor. A result qualifies or rejects a later, narrowly
allowlisted in-game experiment; it is not itself evidence of an MGO FPS gain.

## Implemented backends

| ID | Mechanism | GPU calls per synthetic object set | Purpose |
|---|---|---:|---|
| B0 | Eye-major native reference | `2 * objectCount` | Correctness and timing oracle |
| B1 | Shared invariant CPU packet list, native eye draws | `2 * objectCount` | Isolate reusable CPU preparation |
| B2 | Eye-expanded instancing plus side-by-side clip packing | 1 | Core-D3D11 one-call path without optional viewport routing |
| B3 | Instanced world vertices plus geometry-shader eye replication | 1 | Core-D3D11 `SV_ViewportArrayIndex` path |

B2 maps `combinedInstance / 2` to the object and `combinedInstance % 2` to
the eye. B3 transforms a vertex to world space once, then emits each triangle to
both eye viewports in the geometry shader. Neither path uses vendor extensions.

## Build in GitHub Actions

The repository workflow `.github/workflows/stereo-capability-lab.yml` is the
authoritative compiler. It uses `windows-2022`, MSVC, the Windows SDK, and CMake;
then it runs:

1. a CPU-only evidence/schema self-test;
2. an offscreen standard-D3D11 WARP smoke test covering B0-B3;
3. artifact upload even when a later CI step fails.

Download `StereoCapabilityLab-windows-x64-<commit>` from the workflow run. The ZIP
contains the executable, runtime HLSL, launcher, README, and all CI test evidence.

Equivalent developer build:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

## Run on the RTX 5090 game PC

Extract the artifact to a normal writable directory. Run from PowerShell; the
launcher supplies absolute shader/output paths and logs process failures that
could occur before the executable initializes.

Fast hardware check:

```powershell
.\Run-StereoCapabilityLab.cmd -Mode Smoke
```

Correctness qualification with the D3D debug layer (Windows Graphics Tools must
be installed):

```powershell
.\Run-StereoCapabilityLab.cmd `
  -Mode Validation -Width 1280 -Height 1280 -Draws 128 -DebugLayer
```

Release performance example:

```powershell
.\Run-StereoCapabilityLab.cmd `
  -Mode Benchmark -Scene S1 -Width 3494 -Height 3558 -Draws 1000 `
  -Warmup 300 -Frames 2000
```

Do not compare a debug-layer run, WARP run, different resolution, different
driver, or different binary hash with release RTX 5090 samples as though they
were one population.

## Workloads

- `S0`: correctness-oriented default shading.
- `S1`: many cheap cube submissions; intended to expose CPU/draw overhead.
- `S2`: adds controllable vertex arithmetic.
- `S3`: adds controllable pixel arithmetic and demonstrates the stereo pixel-cost
  floor.

The first implementation deliberately keeps a single deterministic scene shape.
State/resource stress, alpha behavior, clipping torture cases, and deferred
contexts are later lab levels; the generated summary never claims those are
already proven.

## Evidence contract

Every process invocation creates its unique directory before D3D device creation.
It records:

- source/build/binary SHA-256 identity;
- exact command line and normalized configuration;
- adapter, LUID, driver, feature level, debug flags and threading capabilities;
- timestamped human and JSONL event logs plus append-only lifecycle state;
- separate correctness CSV and per-frame CPU/GPU CSV;
- reference/candidate/difference PPM images and depth PGM images;
- an explicit `complete` or `failed` status and conservative summary.

GPU timestamps are issued per measured frame and resolved only after the batch;
the render loop never blocks on its current query. Invalid and disjoint samples
remain explicit instead of becoming zero. Validation readback and file output are
outside the performance phase.

See [results/README.md](results/README.md) for the directory schema.

## Pass/fail interpretation

Image validation allows at most two 8-bit channel levels per pixel and at most
0.1% pixels/depth samples outside tolerance; no above-tolerance seam pixel is
allowed. A failed backend is not benchmarked.
The summary applies the current synthetic performance gate using medians:

- validation passes;
- B0 and B1 identical-submission CPU/GPU medians agree within 5%;
- at least 15% lower CPU submission time than B0;
- no more than 5% GPU regression against B0.

Promotion additionally requires a separate matching debug-layer validation run
with zero errors. Debug timings are never treated as release performance; the two
runs are paired by binary/configuration signatures. Passing both means “worth one
allowlisted in-game POC,” not “safe to deploy broadly.”
The real proof still needs Skyrim eye-boundary tracing, shader/material
allowlisting, headset testing, and compositor-aware frame-delivery evidence.

## Standard API basis

- Microsoft documents geometry-shader viewport selection and the per-primitive
  behavior of `SV_ViewportArrayIndex` in the
  [D3D11 geometry-shader stage](https://learn.microsoft.com/windows/win32/direct3d11/geometry-shader-stage).
- The HLSL [system-value semantics](https://learn.microsoft.com/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics)
  define `SV_ClipDistance`, `SV_InstanceID`, and viewport routing.
- GPU elapsed time follows the documented
  [timestamp/disjoint query](https://learn.microsoft.com/windows/win32/api/d3d11/ne-d3d11-d3d11_query)
  contract and rejects samples whose disjoint flag is true.
- Driver-package identity uses the documented
  [`IDXGIAdapter::CheckInterfaceSupport`](https://learn.microsoft.com/windows/win32/api/dxgi/nf-dxgi-idxgiadapter-checkinterfacesupport)
  `IDXGIDevice` query; Microsoft notes that WDDM 2.3+ driver components share a
  package version.
