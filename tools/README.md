# Tools

Tooling will be added only when a measurement need is defined. Prefer small,
auditable collectors and analysis scripts over one monolithic profiler.

## Expected tool classes

- Configuration signature collector.
- DLL/settings hash manifest generator.
- QPC event provider for frame-loop boundaries.
- ETW/GPUView capture wrapper and process/context annotator.
- Governor/ETW/Nsight frame-identity joiner.
- Community Shaders profiler export parser.
- Per-eye timing and safe-reprojection-mask analyzer.
- Experiment report generator.

## Requirements

- Collectors must declare their own overhead.
- No blocking GPU readback in normal measurement mode.
- Asynchronous samples must publish value and identity coherently.
- Raw data formats must be documented and versioned.
- Analysis must preserve missing samples rather than replacing them with zero.
- Scripts must reject incompatible configuration signatures by default.

## Existing tools to reuse

- Governor trace/replay tooling in
  `development/custom_plugins/CSQualityGovernorVR`.
- Custom OpenXR Toolkit probes in
  `development/custom_OpenXR_Toolkit/tools` and `experiments`.
- Community Shaders profiler, Tracy zones, RenderDoc integration, and DevBench
  bridges in the checked CSX source.

## Implemented

The standalone lab is now implemented at
[`StereoCapabilityLab/`](StereoCapabilityLab/README.md), including B0-B3,
correctness images, delayed GPU queries, immutable run evidence, and Windows CI.

## Specification

- `StereoCapabilityLab/` — standalone D3D11 device probe, correctness renderer,
  and stereo submission benchmark specified in
  [STEREO_CAPABILITY_LAB_DESIGN.md](../docs/STEREO_CAPABILITY_LAB_DESIGN.md).
