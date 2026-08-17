# Ranked Bottleneck Hypotheses

Ranks reflect present evidence and potential, not implementation order.

## Summary

| Rank | ID | Hypothesis | Present likelihood | Potential ceiling |
|---:|---|---|---:|---:|
| 1 | H1 | Stereo duplication plus legacy D3D11 submission | High | Very high |
| 2 | H2 | OpenComposite/OpenXR/Pimax scheduling and admission | High for tail misses | Very high if avoidable wait exists |
| 3 | H3 | Lens-space and gaze-space overshading | High at quality rungs | High |
| 4 | H4 | Cross-eye and temporal recomputation of lighting/effects | Medium-high | High |
| 5 | H5 | Scene-specific grass/shadow/material/LOD pathology | Scene-dependent | Occasionally enormous |
| 6 | H6 | Streaming, shader creation, residency, or compilation | Mostly transient | Medium |
| 7 | H7 | DLSS inference, app submit copy, raw VRAM bandwidth | Lower on current evidence | Low-medium |

## H1 — Stereo duplication and legacy D3D11 submission

### Claim

Skyrim VR repeats substantial scene and shading work for two highly overlapping
views, while thousands of small D3D11 operations and state changes serialize work
through the immediate-context/driver path. The RTX 5090 may be intermittently
starved or may repeat avoidable second-eye work.

### Supporting evidence

- A large fitted low-resolution floor remains at UltraPerformance.
- Historical measurements on this stack reported roughly 5,200–5,900 draw calls
  per frame and a large `Utility` category.
- NVIDIA hardware supports single-pass/multi-view projection, but Skyrim VR was
  not designed around that modern stereo model.
- Current CSX has selective one-eye/reprojection paths for SSGI and screen-space
  shadows.
- An older Community Shaders experiment implemented depth-classified right-eye
  stencil skipping and left-to-right reconstruction.

### Falsification

H1 weakens if a representative GPU trace shows:

- near-continuous GPU occupancy with negligible queue starvation;
- low vertex/geometry and draw-submission contribution;
- little measurable difference between first-eye and second-eye work;
- most cost concentrated in unavoidable full-screen compute or Pimax compositor
  work.

### Required measurement

- Per-eye GPU timestamps and hardware counters.
- GPU queue idle/busy timeline aligned to render-thread activity.
- Draw count, state-change count, vertex count, pixel-shader invocation count,
  and overdraw by eye/pass.

## H2 — Frame scheduling and runtime admission

### Claim

Some late application intervals are not caused by excessive measured application
GPU work. They arise from CPU scheduling, GPU starvation, cross-process fences,
OpenXR pacing, compositor reservation, or arriving outside the useful delivery
window.

### Supporting evidence

- At Performance, 73% of steady missed application intervals in one RC3 capture
  occurred while the application GPU sample was under the nominal 13.89 ms budget.
- The measured app-side post-submit GPU work was about 0.033 ms and flat.
- Pimax/OpenXR delivery counters in the current OpenComposite timing interface are
  unavailable or fallback values.
- OpenXR explicitly throttles and shifts application frame timing through
  `xrWaitFrame`.

### Falsification

H2 weakens if end-to-end traces show that every relevant late interval coincides
with application GPU completion after the runtime deadline and there are no
material CPU, fence, preemption, or compositor scheduling anomalies.

### Required measurement

- QPC events around `WaitGetPoses`, `xrWaitFrame`, `xrBeginFrame`, both eye
  submits, `xrEndFrame`, and return.
- ETW/GPUView across Skyrim and Pimax runtime GPU contexts.
- Runtime predicted display time/period and, if obtainable, delivered-frame IDs.

## H3 — Lens-space and gaze-space overshading

### Claim

Uniform rectangular rendering spends work on pixels later hidden, compressed by
lens distortion, cropped, or outside the gaze-critical region. Existing crop and
foveated upscaling recover only part of the possible saving because not every pass
obeys one common perceptual density map.

### Supporting evidence

- The capture submits 3494 x 3558 per eye.
- The Crystal Super has high-resolution panels, lens distortion, eye tracking,
  and dynamic-foveation capability.
- Earlier foveated-upscaling measurements recovered roughly 5–7 headroom points.
- The DLSS ladder remains strongly monotonic in pixel fraction.

### Falsification

H3 weakens if hardware counters show little fragment/full-screen cost, or if an
aggressive visibility/gaze mask changes shaded pixels without moving GPU time.

### Required measurement

- Pixel-shader invocations and compute work inside/outside the final visible mask.
- Per-pass sensitivity to crop, input scale, and foveal radius.
- Exact Pimax visibility/distortion geometry rather than assumed circles.

## H4 — Temporal and cross-eye recomputation

### Claim

Lighting, GI, distant shadows, probes, volumetrics, and other slowly changing
signals are recomputed at 72 Hz and sometimes per eye even when reprojection or
lower-frequency updates could preserve perceptual correctness.

### Supporting evidence

- CSX already has temporal histories and selective stereo reprojection.
- Head pose changes faster than much of the lighting state.
- Distant binocular disparity is small relative to near-field disparity.

### Falsification

H4 weakens if these passes account for little P95 time or if their outputs contain
frequent view-dependent disocclusion/specular changes that defeat reuse.

## H5 — Scene-specific render pathology

### Claim

Particular MGO assets or settings multiply draw, shadow, overdraw, lighting, or
material work. One bad flag, mesh partition pattern, alpha material, LOD, or
shadow caster can dominate a scene and create the classic small-fix/large-win
modding opportunity.

### Required measurement

- Top draws by GPU duration and repeated state/material signature.
- Alpha overdraw heatmap.
- Shadow-caster and light counts.
- Scene comparison: controlled interior, forest, city, heavy combat, rain.

## H6 — Transient streaming and compilation

This is more likely to explain turns, cell boundaries, newly visible effects, and
first-use hitches than the steady frame floor. Track texture residency, page
faults, disk I/O, shader creation, pipeline creation, and WDDM eviction around
tail events.

## H7 — Lower-priority suspects

Current evidence argues against starting with:

- the OpenComposite app-side eye copy;
- raw 5090 VRAM capacity or bandwidth;
- DLSS inference alone;
- the Governor render-target relatch as a steady-state cost;
- physical DisplayPort throughput.

Each remains measurable and may matter in a narrower failure mode.

