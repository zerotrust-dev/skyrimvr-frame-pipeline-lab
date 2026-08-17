# Status

Date: 2026-08-17

## Current phase

**Phase 0 — establish a trustworthy end-to-end measurement boundary.**

The project has a strong application-GPU ladder and useful application interval
data, but it cannot yet attribute all late frames. OpenComposite's exposed timing
fields are partly fallback values on the Pimax/OpenXR path, and the Pimax runtime
runs outside the D3D11 timestamp bracket used by the Governor.

## Leading hypotheses

1. **H1 — stereo duplication and legacy submission:** repeated eye work plus
   thousands of D3D11 draws create a large fixed/feed-bound floor.
2. **H2 — frame scheduling and compositor admission:** some otherwise affordable
   frames arrive outside the runtime's useful delivery window.
3. **H3 — lens-space and gaze-space overshading:** rectangular uniform rendering
   spends work on pixels later compressed, hidden, cropped, or perceptually
   unimportant.
4. **H4 — temporal recomputation:** slowly changing lighting and visibility are
   recalculated more frequently than necessary.
5. **H5 — scene-specific pathologies:** grass, alpha overdraw, shadows, lights,
   material partitions, or LODs dominate particular MGO scenes.

## Immediate next actions

1. Record one immutable baseline signature using the fields in
   [STACK_BASELINE.md](docs/STACK_BASELINE.md).
2. Capture an ETW/GPUView trace that includes SkyrimVR, NVIDIA GPU queues,
   OpenComposite/OpenXR calls, and Pimax runtime processes.
3. Capture one representative frame at UltraPerformance, Quality, and NativeAA
   with identical camera/scene state in Nsight Graphics or an equivalent GPU
   profiler.
4. Export Community Shaders per-pass Avg/P95/P99 data for the same three states.
5. Measure first-eye and second-eye costs separately before building any stereo
   reuse prototype.
6. Review and implement the passive observer specified in
   [STEREO_DUPLICATION_D3D11_SUBMISSION_DESIGN.md](docs/STEREO_DUPLICATION_D3D11_SUBMISSION_DESIGN.md).
7. Obtain Claude and ParticleTroned review of the
   [StereoFusion implementation roadmap](docs/STEREOFUSION_IMPLEMENTATION_ROADMAP.md),
   then execute WP-001 before creating optimization code.
8. Compile and smoke-test the implemented lab in GitHub Actions, then run the
   [StereoCapabilityLab](docs/STEREO_CAPABILITY_LAB_DESIGN.md) through levels L0–L4
   before selecting a one-call D3D11 geometry backend.

## Explicit non-goals for Phase 0

- Do not implement full single-pass stereo.
- Do not change the Governor policy.
- Do not infer headset scanout from application interval alone.
- Do not optimize the render-scale relatch; it is a separate transition problem.
- Do not select a solution before the GPU busy/starved/runtime-late split is known.
