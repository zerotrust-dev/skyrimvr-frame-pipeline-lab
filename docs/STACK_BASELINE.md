# Verified Stack Baseline

Status: starting signature assembled from RC3 captures and source audits. Re-record
before every experiment series.

## Hardware and display

| Component | Baseline |
|---|---|
| GPU | NVIDIA GeForce RTX 5090, 32 GB GDDR7 |
| HMD | Pimax Crystal Super |
| Panel class | 3840 x 3840 per eye for QLED optical engines |
| Test refresh | 72.000 Hz |
| Frame budget | 13.8889 ms |
| Submitted texture | 3494 x 3558 per eye in the 2026-08-12 RC3 capture |

The submitted texture dimensions, not a marketing panel resolution or a Pimax UI
label, are the authoritative render-chain identity for a capture.

## Software path

| Layer | Baseline |
|---|---|
| Game | Skyrim VR |
| Mod list | Mad God Overhaul 4.0 beta RC3 |
| Renderer extension | Community Shaders CSX 3.18 VR family |
| API build in RC3 capture | 11 |
| App/runtime bridge | OpenComposite Unleashed for Skyrim VR 4.2.3 |
| OpenComposite source line | `Unstable` branch matching the 2026-08-07 archive |
| XR runtime | Pimax OpenXR runtime |
| Upscaler | Community Shaders NVIDIA DLSS |
| DLSS profile | K in the established baseline |
| Controller | CSQualityGovernorVR |

Public source and installed binaries have diverged in build numbering before.
Record archive name, DLL hash, API build, source commit if known, NVIDIA driver,
Pimax Play version, and OpenComposite archive for every serious capture.

## Known graphics ownership

The established stack aims for one owner per lever:

| Lever | Owner |
|---|---|
| Quality rung / DLSS input scale | Community Shaders, controlled by Governor |
| Final submitted eye size | Runtime configuration / observed OpenVR size |
| FOV crop | PrimaShock/OpenXR Toolkit path when enabled |
| CS foveated upscaling | Community Shaders |
| Pimax center rendering | Off in the established baseline |
| Pimax sharpening | Off in the established baseline |
| Final sharpening | PrimaShock was the single sharpener in the measured baseline |
| Skyrim automatic dynamic resolution | Disabled/owned by CS integration |

## Capture signature

Every experiment must record at minimum:

```text
capture_id
wall_clock_start
save_name and location/cell
camera position and orientation method
weather and game time
HMD product and optical engine
reported refresh period
true submitted width/height per eye
Pimax Play version and Image Quality
Pimax refresh/upscale/lab mode
Pimax foveation/center-rendering/smoothing settings
OpenComposite archive/version/hash
OpenXR API layers and order
Community Shaders DLL hash/API build/source commit
Community Shaders settings hash
Governor DLL hash/config hash
NVIDIA driver
GPU clocks/power/temperature
CPU model, clocks and active power policy
Windows build and HAGS state
active overlays/capture tools
```

## Measured RC3 ladder

From the 2026-08-12 17:10 session, per-preset mean application GPU time:

| Rung | Mean GPU ms |
|---|---:|
| UltraPerformance | 9.70 |
| Performance | 11.32 |
| Balanced | 12.59 |
| Quality | 13.50 |
| UltraQuality | 15.64 |
| Hoshipa | 16.75 |
| NativeAA | 20.81 |

The mean fit was approximately:

```text
t = 8.22 + 12.34 * f
```

where `f = scale^2`. The corrected deduplicated P95 fit was:

```text
t = 9.26 + 15.11 * f
```

The intercept is a fitted term, not a literal measurement of all fixed passes. It
is nevertheless evidence that reducing input pixels alone does not remove most of
the low-rung frame cost.

Primary data note:
[RC3 2026-08-12 17:10](https://github.com/zerotrust-dev/CSQualityGovernorVR/tree/main/tests/data/rc3-2026-08-12-1710)
