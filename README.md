# Skyrim VR Frame Pipeline Lab

[![StereoCapabilityLab](https://github.com/zerotrust-dev/skyrimvr-frame-pipeline-lab/actions/workflows/stereo-capability-lab.yml/badge.svg)](https://github.com/zerotrust-dev/skyrimvr-frame-pipeline-lab/actions/workflows/stereo-capability-lab.yml)

Status: **research project initialized 2026-08-16**

This project investigates where throughput, latency, and frame-delivery reliability
are lost across the complete Skyrim VR pipeline used by Mad God Overhaul 4.0 RC3.
It begins at game simulation and draw preparation and ends at the image delivered
to the Pimax Crystal Super.

The project is intentionally separate from both Community Shaders and
CSQualityGovernorVR:

- Community Shaders is one renderer component and a likely implementation surface.
- CSQualityGovernorVR is a measurement/control instrument and a likely consumer of
  any new capability.
- This project owns the cross-component evidence and decides which bottleneck is
  worth attacking.

## Working hypothesis

The leading structural hypothesis is that Skyrim VR repeats too much work between
two highly overlapping eye views and feeds that work through a legacy D3D11
submission path. The leading frame-delivery hypothesis is that some late frames
are caused by CPU/runtime scheduling or compositor admission rather than measured
application GPU work.

These are hypotheses, not conclusions. The first task is to measure which of the
following happens on a late frame:

1. the RTX 5090 is continuously busy;
2. the RTX 5090 is starved while Skyrim or the driver prepares work;
3. the application finishes useful work but misses the OpenXR/Pimax delivery
   window;
4. an out-of-process compositor workload consumes the unmeasured margin.

## Start here

1. [Project charter](docs/PROJECT_CHARTER.md)
2. [Verified stack baseline](docs/STACK_BASELINE.md)
3. [End-to-end pipeline map](docs/PIPELINE_MAP.md)
4. [Ranked bottleneck hypotheses](docs/BOTTLENECK_HYPOTHESES.md)
5. [Stereo reuse research](docs/STEREO_REUSE.md)
6. [Stereo duplication and D3D11 submission design](docs/STEREO_DUPLICATION_D3D11_SUBMISSION_DESIGN.md)
7. [StereoFusion implementation roadmap](docs/STEREOFUSION_IMPLEMENTATION_ROADMAP.md)
8. [StereoCapabilityLab design](docs/STEREO_CAPABILITY_LAB_DESIGN.md)
9. [StereoCapabilityLab implementation review](docs/STEREO_CAPABILITY_LAB_IMPLEMENTATION_REVIEW.md)
10. [Measurement program](docs/MEASUREMENT_PLAN.md)
11. [Evidence ledger](docs/EVIDENCE_LEDGER.md)
12. [Source map](docs/SOURCE_MAP.md)
13. [Project status and next actions](STATUS.md)

## Project layout

```text
skyrimvr-frame-pipeline-lab/
|-- README.md
|-- STATUS.md
|-- docs/
|   |-- PROJECT_CHARTER.md
|   |-- STACK_BASELINE.md
|   |-- PIPELINE_MAP.md
|   |-- BOTTLENECK_HYPOTHESES.md
|   |-- STEREO_REUSE.md
|   |-- STEREO_DUPLICATION_D3D11_SUBMISSION_DESIGN.md
|   |-- STEREOFUSION_IMPLEMENTATION_ROADMAP.md
|   |-- STEREO_CAPABILITY_LAB_DESIGN.md
|   |-- MEASUREMENT_PLAN.md
|   |-- EVIDENCE_LEDGER.md
|   |-- SOURCE_MAP.md
|   `-- DECISIONS.md
|-- experiments/
|   |-- README.md
|   `-- EXPERIMENT_TEMPLATE.md
|-- data/
|   `-- README.md
`-- tools/
    `-- README.md
```

## Rules of evidence

- A timestamp inside `SkyrimVR.exe` does not measure the Pimax compositor.
- A late application interval is not automatically a dropped headset frame.
- Means describe throughput; P95/P99 and delivery deadlines describe VR comfort.
- Repeated asynchronous GPU samples must be deduplicated by their frame identity.
- Every capture must carry a complete configuration signature.
- Public source must be matched to the installed build or archive before it is
  treated as exact runtime behavior.
- A promising rendering idea is not promoted until it states its artifact and
  correctness risks for both eyes.

## Adjacent projects

- [CSQualityGovernorVR](https://github.com/zerotrust-dev/CSQualityGovernorVR)
- [Governor RC3 data](https://github.com/zerotrust-dev/CSQualityGovernorVR/tree/main/tests/data)
- Foveated Community Shaders research (local evidence archive; not included)
- [Custom OpenXR Toolkit work](https://github.com/zerotrust-dev/custom_OpenXR_Toolkit)

## License

The standalone research project and StereoCapabilityLab are released under the
[MIT License](LICENSE). Referenced upstream projects retain their own licenses.
