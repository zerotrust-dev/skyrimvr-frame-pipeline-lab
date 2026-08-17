# StereoCapabilityLab Design

Status: **L0-L4 first implementation complete; CI/hardware qualification pending**  
Date: 2026-08-17  
Purpose: answer GPU/API questions before modifying Skyrim or Community Shaders

Implementation note (2026-08-17): the first executable is in
[`tools/StereoCapabilityLab`](../tools/StereoCapabilityLab/README.md). It uses
only standard D3D11. B2 includes an explicit `SV_ClipDistance` inner plane so
side-by-side clip packing cannot leak a primitive across the eye seam. Runtime
HLSL, the PowerShell launcher, and GitHub Actions packaging are part of the
evidence boundary. S4/S5 expansion and B4 remain subsequent levels; the summary
does not claim them as tested.

## Executive decision

Build a small standalone D3D11 executable before the first shared-geometry
prototype.

The lab is not a miniature Skyrim renderer and must not be treated as proof of an
MGO performance gain. It answers a narrower, high-value question:

> On the actual RTX 5090, driver, Windows build, and D3D11 runtime, which
> two-eye geometry submission techniques are supported, correct, and faster than
> two native draws?

Skyrim-specific duplication, engine hook reachability, material compatibility,
and real-play gains still require the in-game CSX StereoTrace phase.

## 1. Questions the lab must answer

### Capability

- Which D3D feature level and creation flags are active?
- Does the driver report concurrent resource creation and command-list support?
- Which shader stages can route primitives to viewport/render-target arrays?
- Can a geometry shader reliably emit both eye projections in D3D11?
- Can instancing add an eye dimension without corrupting object instances?
- Is a documented, redistributable NVIDIA SPS/MVR interface available?
- Does the D3D debug layer report any hazard for each approach?

### Correctness

- Do both eyes match a two-draw reference under asymmetric projection?
- Are depth, culling, clipping, winding, scissor, and viewport behavior correct?
- Are per-eye constants/resources consumed at the expected frequency?
- Are near-object parallax and disocclusion geometrically correct?
- Can each backend be disabled without recreating the device or targets?

### Performance

- What is CPU submission time at 100, 1,000, 5,000, and 10,000 packets?
- What is GPU time for vertex-light, balanced, and pixel-light scenes?
- At what draw/triangle count does geometry sharing break even?
- Does geometry-shader replication save submission but cost GPU time?
- Do deferred contexts improve or regress CPU time?
- How do results change at representative stereo resolutions?

## 2. Questions the lab cannot answer

The executable cannot determine:

- whether Skyrim builds or submits its scene twice;
- where the real Skyrim left/right phase boundary lies;
- whether Skyrim visibility, sorting, or material preparation is shared;
- which MGO shader/material families can be transformed safely;
- whether CSX, DLSS, overlays, or OpenComposite tolerate the changes;
- whether the Pimax compositor admits more application frames;
- actual in-headset comfort or end-to-end MGO savings.

Those remain StereoTrace and in-game proof obligations.

## 3. Implementation boundary

Create the lab under the Frame Pipeline Lab:

```text
tools/StereoCapabilityLab/
|-- CMakeLists.txt
|-- README.md
|-- src/
|   |-- Main.cpp
|   |-- DeviceProbe.cpp
|   |-- BenchmarkRunner.cpp
|   |-- ReferenceBackend.cpp
|   |-- InstancedStereoBackend.cpp
|   |-- GeometryShaderStereoBackend.cpp
|   |-- DeferredContextBackend.cpp
|   |-- Validation.cpp
|   `-- CsvWriter.cpp
|-- shaders/
|   |-- Reference.hlsl
|   |-- InstancedStereo.hlsl
|   |-- GeometryStereo.hlsl
|   `-- Validation.hlsl
|-- tests/
|   |-- ProjectionTests.cpp
|   |-- ImageComparisonTests.cpp
|   `-- ResultSchemaTests.cpp
`-- results/
    `-- README.md
```

Do not link Skyrim, SKSE, CSX, OpenVR, OpenXR, Streamline, or the Governor. This
keeps API conclusions independent and makes the executable safe without the game.

## 4. Backends

### B0 — two-draw native reference

```text
SetEyeConstants(left)
SetViewport(left)
DrawIndexed(...)

SetEyeConstants(right)
SetViewport(right)
DrawIndexed(...)
```

All backends must match this correctness oracle within declared raster tolerance.

### B1 — shared CPU packet, two native draws

Build, sort, and bind invariant packet data once, then issue two eye draws with
only eye-variant state changed. This tests the safest target: shared preparation
without shared GPU geometry.

### B2 — eye-expanded instancing

Use one instanced call with an additional logical eye dimension:

```text
objectInstance = combinedInstance / eyeCount
eyeIndex       = combinedInstance % eyeCount
```

This is valid only for controlled shaders and layouts. It tests one-call stereo
while preserving existing object instancing.

### B3 — geometry-shader stereo replication

Run object vertex work once, replicate each primitive in a geometry shader, apply
eye projections, and write `SV_ViewportArrayIndex`.

This generic D3D11 route may move work into a geometry stage that costs more than
two modern vertex invocations. The benchmark decides.

### B4 — deferred-context recording

Record isolated immutable packets on deferred contexts and execute their command
lists on the immediate context. This tests CPU recording, not shared GPU geometry.

Record:

```text
device creation flags
D3D11_FEATURE_DATA_THREADING.DriverConcurrentCreates
D3D11_FEATURE_DATA_THREADING.DriverCommandLists
FinishCommandList CPU time
ExecuteCommandList CPU time
GPU time
```

### B5 — optional vendor stereo backend

Do not make the lab depend on this backend.

The current public [NVIDIA NVAPI repository](https://github.com/NVIDIA/nvapi)
does not expose an obvious supported Single-Pass Stereo/Multi-View interface in
the audited source. Historical VRWorks pages are prior art, not a current SDK
contract.

Implement B5 only with:

1. current official NVIDIA documentation;
2. a redistributable SDK/header/library with clear license terms;
3. a runtime support query that succeeds on the RTX 5090 driver;
4. a maintained fallback when unavailable.

Absence of B5 does not block StereoFusion.

## 5. Synthetic scenes

### S0 — correctness grid

- asymmetric left/right projection;
- colored near, mid, and far geometry;
- depth intersections and occlusion edges;
- front/back-face winding cases;
- primitives crossing seams and frustum boundaries.

### S1 — submission-bound

- many small draws;
- few triangles per draw;
- inexpensive pixel shader;
- configurable redundant/changing state;
- 100 through 10,000 packets.

### S2 — vertex-bound

- fewer draws and dense meshes;
- controllable vertex transform/bone-like arithmetic;
- cheap pixel shader.

### S3 — pixel-bound control

- large screen coverage;
- expensive pixel shader;
- low geometry cost.

This demonstrates the expected limit: shared geometry cannot remove two-eye pixel
shading.

### S4 — existing instancing

- many object instances;
- distinct transforms/material indices;
- eye-expanded instance mapping;
- odd/even counts and maximum IDs.

### S5 — state and resource stress

- dynamic constant-buffer updates;
- SRV/sampler changes;
- depth/blend/rasterizer changes;
- alpha-test control for correctness only;
- indirect arguments where supported.

## 6. Resolution and workload matrix

```text
1280 x 1280 per eye       quick functional
2448 x 2448 per eye       medium VR
3494 x 3558 per eye       current MGO RC3 submitted reference
3840 x 3840 per eye       Crystal Super panel-class stress
```

Dimensions are command-line inputs, never backend constants.

Per backend/scene/resolution:

```text
warmup:          >= 300 frames
measurement:     >= 2,000 frames
repetitions:     >= 5 process-level runs
CPU statistics:  mean, median, P95, P99
GPU statistics:  mean, median, P95, P99
validation:      reference image comparison + debug-layer count
```

## 7. Measurement implementation

### CPU

Measure separately:

```text
packet preparation
state binding
command recording
FinishCommandList
ExecuteCommandList
total render-loop CPU
```

Use a high-resolution monotonic counter, pin/report the benchmark thread, and
record power-plan/CPU state. Never time debug-layer and release runs as comparable.

### GPU

Use timestamp/disjoint queries with delayed readback. Never block each frame to
read the current query. Report invalid/disjoint samples explicitly.

Measure:

```text
whole backend
geometry phase
pixel control phase
copy/validation phase excluded from performance result
```

### Frame identity

Every record contains:

```text
run_id
process_run
frame_id
backend
scene
resolution
draw_count
triangle_count
instance_count
driver_version
adapter_luid
feature_level
debug_layer
build_hash
```

## 8. Image validation

Render reference and candidate into separate offscreen targets with identical
inputs.

Validation output:

- left/right color PNG or lossless equivalent;
- depth visualization;
- absolute-difference heatmap;
- mismatched-pixel count;
- maximum and percentile channel error;
- seam/frustum-edge error classification;
- exact backend/configuration signature.

Integer/depth-sensitive tests should be exact where possible. Floating-point
raster results need a documented small tolerance, never an unexplained blanket
threshold.

Candidate failure immediately disqualifies that run from performance promotion.

## 9. Output schema

### `capabilities.json`

```json
{
  "adapter": "NVIDIA GeForce RTX 5090",
  "driver": "recorded-at-runtime",
  "feature_level": "11_1",
  "creation_flags": 0,
  "driver_concurrent_creates": false,
  "driver_command_lists": false,
  "geometry_viewport_routing": "tested",
  "vendor_stereo_api": "not_available_or_unverified"
}
```

Values above illustrate the schema; the lab must populate real results.

### `benchmark.csv`

```text
run_id,process_run,frame_id,backend,scene,width_per_eye,height,
draws,triangles,instances,cpu_prepare_us,cpu_submit_us,cpu_total_us,
gpu_total_us,query_valid,validation_pass,max_error,mismatch_pixels
```

### `summary.md`

The generated summary must answer:

```text
fastest correct backend by workload
CPU and GPU break-even points
unsupported/failed backends and exact reason
debug-layer findings
recommended backend for the in-game POC
remaining questions that only Skyrim can answer
```

## 10. Decision gates

### Gate L1 — platform support

At least B1 and one one-call geometry backend must render correctly. If neither B2
nor B3 is correct, StereoFusion can still pursue CPU preparation reuse but not the
one-call geometry claim through those methods.

### Gate L2 — synthetic performance

Promote a geometry backend only if:

```text
correct across S0, S2, S4, and S5
no D3D debug-layer error
>= 15% CPU submission saving in S1
no >5% GPU regression in representative balanced workloads
positive net result at current MGO reference dimensions
```

These gates qualify an in-game experiment; they do not predict its gain.

### Gate L3 — reproducibility

Another run from a clean build must reproduce the selected backend within the
declared variance. Every result retains source commit, binary hash, driver, and
command line.

## 11. Risks and controls

| Risk | Control |
|---|---|
| Synthetic benchmark flatters the method | Multiple workload classes; no MGO claims |
| Geometry shader is a bottleneck | Measure B3 against B0/B2; reject on GPU regression |
| Existing instancing semantics break | Explicit S4 mapping and maximum-ID tests |
| CPU timer includes validation | Separate performance and validation phases |
| GPU query stalls distort timing | Delayed asynchronous readback |
| Debug layer changes performance | Separate correctness and release runs |
| Historical NVAPI is unavailable | B5 optional; standard D3D11 backends first |
| One driver result is treated as universal | Record driver; rerun after material update |
| Side-by-side hides eye errors | Separate per-eye validation and asymmetric cameras |
| Test app becomes premature engine | Strict boundary; no Skyrim/CSX dependencies |

## 12. Build order

```text
L0 project skeleton + device probe + CSV/JSON
L1 B0 reference + S0 correctness + image export
L2 B1 shared packet + CPU timing
L3 B2 eye-expanded instancing
L4 B3 geometry-shader replication
L5 S1-S5 workload generator + GPU queries
L6 B4 deferred contexts
L7 automated matrix + summary generator
L8 optional B5 only if current official SDK is found
```

Each level must pass before the next. L0–L4 provide the highest-value answers.

## 13. Exit deliverable

The lab is finished when it produces one auditable statement of this form:

```text
On adapter/driver X, backend Y is correct under tests A–E.
At the MGO reference dimensions and draw workload range, it changes
CPU submission by N% and GPU time by M% versus two native draws.
It is/is not recommended for one allowlisted in-game shader-family POC.
These Skyrim-specific questions remain unanswered: [...].
```

Anything less precise is not a foundation for renderer changes.
