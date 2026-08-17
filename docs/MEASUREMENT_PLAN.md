# Measurement Program

## Principle

Measure boundaries before components. A perfectly measured shader does not explain
a frame that missed because the CPU woke late or the runtime rejected its timing.

## Phase 0 — configuration identity

Goal: make captures comparable.

1. Implement or manually complete the signature in `STACK_BASELINE.md`.
2. Hash relevant DLLs and normalized settings.
3. Record true submitted dimensions and runtime-predicted period.
4. Pin a save, weather, time, position, and repeatable camera motion.
5. Record capture overhead using a no-capture control.

Exit condition: two nominally identical runs agree within a declared noise floor
for application GPU P50/P95, application interval P95, and draw count.

## Phase 1 — end-to-end timeline

Goal: classify late frames as GPU-busy, GPU-starved, runtime-late, or compositor
contention.

Capture:

- Windows ETW/GPUView providers for CPU scheduling, D3D11/DXGI, WDDM queues,
  context switches, page faults, disk I/O, and GPU preemption;
- Skyrim main/render thread IDs;
- Pimax runtime/compositor process and GPU context IDs;
- QPC events for `WaitGetPoses`, `xrWaitFrame` return, `xrBeginFrame`, first eye
  submit, second eye submit, `xrEndFrame` entry/return, and Present fallback;
- Governor frame, GPU sample, rung, and interval identities.

For every late frame, calculate:

```text
CPU ready-to-submit time
GPU queue idle time inside app bracket
GPU app execution time
eye0-to-eye1 submit gap
xrEndFrame CPU duration
Pimax GPU context overlap/preemption
completion relative to predicted display time
```

Exit condition: at least 95% of investigated late intervals have one primary
classification and no timestamp/frame-identity ambiguity.

## Phase 2 — resolution sensitivity and pass decomposition

Goal: explain the fitted fixed and pixel-dependent terms.

Capture the identical scene at:

- UltraPerformance;
- Quality;
- NativeAA.

For each state collect:

- CS whole-frame application GPU sample;
- CS pass Avg/P95/P99;
- draw calls and shader categories;
- GPU duration by event/pass;
- pixel-shader invocations;
- vertex counts;
- compute dispatch dimensions;
- SM/warp occupancy where available;
- memory throughput/cache hit rates;
- GPU queue gaps.

Classify each pass as:

```text
fixed
input-pixel-scaled
output-pixel-scaled
geometry/draw-scaled
scene/event-scaled
runtime-only
```

Exit condition: at least 90% of application GPU P95 is attributed or explicitly
labelled as an idle/synchronization gap.

## Phase 3 — stereo decomposition

Goal: estimate the upper bound for H1 before building it.

Required instrumentation:

- first-eye and second-eye geometry/shading ranges;
- per-eye DLSS and CS passes;
- safe reprojection classification from existing depth;
- disocclusion/transparent/foveal masks;
- screenshot/difference capture for both eyes.

Scenes:

1. distant landscape;
2. dense forest;
3. close NPC and hands;
4. interior with particle lights;
5. water/POM/reflection scene;
6. rapid lateral head translation.

Exit condition: a measured recoverable-ms range and an artifact-risk map, not just
a safe-pixel percentage.

## Phase 4 — runtime path comparison

Goal: isolate H2 and any Pimax 72 Hz Upscale/Lab behavior.

Compare where available:

- ordinary 72 Hz versus 72 Hz Upscale/Lab;
- identical submitted dimensions and app quality;
- OpenComposite versions/builds;
- optional runtime foveation/smoothing states, one lever at a time.

Do not compare UI labels alone. Verify the submitted extent, predicted period,
active runtime processes, and compositor queue behavior.

## Phase 5 — scene pathology hunt

Goal: find small, high-impact MGO content/configuration defects.

For bad scenes, rank:

- draws by GPU time;
- material/shader signatures by repetition;
- shadow casters and lights;
- alpha overdraw;
- triangle and material partitions by asset;
- page faults/residency changes;
- first-use shader/resource creation.

Correlate the top offender back to its mod and asset path before changing it.

## Metrics

Primary:

- delivered-frame identity, if obtained;
- application interval miss fraction;
- application GPU P50/P95/P99;
- runtime/compositor GPU duration;
- GPU queue idle fraction;
- time-weighted quality/pixel fraction.

Secondary:

- draw/state counts;
- per-eye duplicated milliseconds;
- shaded/visible/foveal pixel ratios;
- VRAM budget and WDDM residency;
- CPU main/render-thread ready/running/wait time.

## Statistical rules

- Deduplicate asynchronous GPU samples by sample frame/index.
- Treat dwell visits as units; do not pool serpentine turnaround endpoints blindly.
- Report scene motion and visit order.
- Use P95/P99 for safety and means only for throughput/energy questions.
- Establish capture-tool overhead for every tool and mode.
- Preserve raw data; derived reports must name their script and revision.

