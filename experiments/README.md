# Experiments

Each experiment receives its own directory:

```text
experiments/
  YYYY-MM-DD-short-name/
    README.md
    config/
    raw/
    derived/
    captures/
```

Only `README.md` and small normalized configuration files should be committed by
default. Large GPU captures, ETL traces, RenderDoc captures, videos, and raw image
sets belong in a locally referenced artifact location unless the repository's
storage policy is changed explicitly.

Copy [EXPERIMENT_TEMPLATE.md](EXPERIMENT_TEMPLATE.md) into the new experiment
directory and fill it before running the experiment. A result without a complete
configuration identity is exploratory, not decision evidence.

## Planned experiment series

| ID | Experiment | Hypotheses |
|---|---|---|
| [X-001](2026-08-16-x001-end-to-end-trace/README.md) | ETW/GPUView end-to-end late-frame classification | H1, H2, H6 |
| X-002 | Nsight ladder decomposition at UP/Quality/NativeAA | H1, H3, H4 |
| X-003 | Community Shaders per-pass P95 export | H1, H3, H4, H5 |
| X-004 | Per-eye work and safe-reprojection mask | H1, H4 |
| X-005 | Ordinary 72 Hz vs 72 Hz Upscale/Lab | H2, H3 |
| X-006 | Forest alpha/shadow/asset pathology trace | H5 |
| [X-007](2026-08-17-x007-stereo-capability-hardware/README.md) | StereoCapabilityLab CI and RTX 5090 qualification | H1 |
