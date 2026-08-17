# Result directories

The executable creates one immutable directory per process invocation:

```text
YYYYMMDDTHHMMSS.mmmZ-pPID/
|-- run_manifest.json       binary/source/build identity
|-- config.json             exact workload/backend selection and runtime shader hash
|-- environment.json        machine/process facts
|-- capabilities.json       adapter, driver, feature and threading support
|-- command_line.txt        exact invocation
|-- status.json             latest explicit run state
|-- lifecycle.jsonl         append-only state history
|-- run.log                 readable timestamped log
|-- events.jsonl            structured append-only event stream
|-- validation.csv          correctness metrics
|-- benchmark.csv           one row per measured frame
|-- debug_layer.log         D3D11 validation output or disabled marker
|-- summary.md              statistics and conservative promotion gate
`-- images/                 lossless PPM/PGM reference, candidates and diffs
```

Failed initialization and partial runs are evidence. Do not delete them. The
launcher also appends process start/finish records to `launcher-events.jsonl` in
the result root, covering failures that occur before the executable can log.

Never combine performance samples across different run signatures. Debug-layer
and WARP results qualify correctness only and are not RTX 5090 performance data.
