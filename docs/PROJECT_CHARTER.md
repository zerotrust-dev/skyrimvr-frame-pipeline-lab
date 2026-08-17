# Project Charter

## Objective

Find the highest-leverage bottleneck in the complete MGO4 RC3 VR frame pipeline
and produce enough evidence to choose a serious implementation project.

The target is not a higher benchmark average. The target is a larger fraction of
correct, high-quality frames delivered inside the Pimax Crystal Super's display
deadline, with stable binocular output and no new comfort regressions.

## Primary question

Where is the most valuable avoidable work or wait between:

```text
game simulation
  -> scene traversal and culling
  -> D3D11 command generation
  -> stereo geometry and shading
  -> Community Shaders
  -> DLSS/final eye textures
  -> OpenComposite
  -> OpenXR/Pimax compositor
  -> distortion/reprojection
  -> headset scanout
```

## Success criteria

The research phase succeeds when it can answer all of these:

1. What fraction of a representative P95 frame is GPU-busy, CPU-feed idle,
   synchronization idle, and compositor/runtime work?
2. How much work is repeated between the two eyes?
3. Which work scales with input pixels and which does not?
4. Which late application intervals correspond to actual runtime/headset delivery
   failures?
5. Which candidate has the best combination of recoverable milliseconds,
   perceptual safety, implementation reach, and compatibility with the current
   mod stack?

## Evaluation dimensions

Every candidate is rated on:

- **Measured cost:** observed milliseconds or missed-deadline contribution.
- **Recoverable fraction:** how much of that cost can theoretically disappear.
- **Tail effect:** whether it improves P95/P99 and deadline delivery.
- **Binocular correctness:** disocclusion, transparency, specular, POM, particles,
  menus, hands, and near-field behavior.
- **Temporal correctness:** motion vectors, history, head motion, and late pose.
- **Implementation reach:** SKSE hook, Community Shaders change, OpenComposite
  change, API layer, or closed Pimax runtime.
- **Configuration independence:** behavior across HMD resolution, Pimax Image
  Quality, refresh rate, FOV/crop, and DLSS rung.
- **Failure containment:** safe fallback when an optimization is uncertain.

## Project boundaries

### In scope

- Skyrim main/render-thread timing.
- D3D11 draw submission and GPU queue occupancy.
- Per-eye and stereo-redundant work.
- Community Shaders feature/pass timing.
- DLSS input/output and foveated reconstruction.
- OpenComposite/OpenXR frame-loop boundaries.
- Pimax compositor timing observable through ETW/GPU scheduling.
- Scene-specific draw, shadow, grass, material, and streaming pathologies.

### Out of scope until supported by evidence

- Rewriting Skyrim VR as a new engine.
- Assuming frame generation is acceptable for VR.
- Treating synthetic frames as equivalent to delivered native frames.
- Depending on undocumented Pimax behavior without a runtime trace.
- Large implementation work based only on averages or a single static scene.

## Relationship to the Governor

The Governor solves a control problem: choose a quality rung from measured
headroom and delivery evidence. This project solves an architecture problem:
understand and reduce the work or waits that create that headroom and those misses.

The Governor can supply:

- deduplicated application GPU samples;
- quality-rung identity;
- application frame intervals;
- transitions and stable dwell windows;
- reproducible ladder captures.

It cannot currently prove which image the Pimax compositor scanned out.

