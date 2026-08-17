# Evidence Ledger

This ledger separates measurements, source facts, and inferences. Promote an
inference only after a discriminating experiment.

| ID | Type | Statement | Source | Consequence |
|---|---|---|---|---|
| E-001 | Measurement | RC3 mean ladder is monotonic from 9.70 ms at UltraPerformance to 20.81 ms at NativeAA. | Governor RC3 17:10 capture | Pixel-dependent work is substantial. |
| E-002 | Analysis | Corrected deduplicated P95 fit is `9.26 + 15.11*f`. | Governor RC3 17:10 capture | A large low-scale floor remains; pixel reduction alone is insufficient. |
| E-003 | Measurement | At Performance, 73% of recorded late steady intervals occurred while app GPU was under nominal budget. | Governor RC3 17:10 capture | App GPU duration does not explain every late interval. |
| E-004 | Measurement | App-side post-submit GPU work was about 0.033–0.034 ms and flat across the two low rungs measured. | Governor RC3 17:10 capture | A large in-process OpenComposite submit copy is not the missing cost. |
| E-005 | Source fact | OpenComposite's Pimax/OpenXR timing fields include fallbacks/residuals; Oculus delivery counters are unavailable. | `OPENCOMPOSITE_FRAME_TIMING.md` and matched `Unstable` source | Current fields cannot prove headset scanout or compositor GPU time. |
| E-006 | Historical measurement | Earlier stack measurements reported 5,200–5,900 draw calls and Utility up to roughly 8 ms. | `knowledge/COMMUNITY_SHADERS_ROI_SKYRIMVR.md` | Draw/utility work is a candidate, but must be remeasured on RC3. |
| E-007 | Source fact | CSX SSGI and screen-space shadows contain stereo sync/reprojection paths. | CSX VR source | Cross-eye reuse is reachable for selected effects. |
| E-008 | Source fact | An older CS branch implemented depth classification, right-eye stencil skipping, and left-eye reprojection. | Local `VRStereoOptimizations` source | Broader stereo reuse has prior art but was not production-proven. |
| E-009 | Source fact | D3D11 immediate-context rendering calls are serialized; deferred contexts can record command lists on workers. | Microsoft D3D11 documentation | CPU/driver feeding is architecturally plausible. |
| E-010 | Source fact | OpenXR uses `xrWaitFrame` to throttle and may adjust application start relative to display/compositor needs. | Khronos OpenXR specification | Timing placement can matter independently of total work. |
| E-011 | Hardware fact | Crystal Super QLED engines use 3840 x 3840 panels, support eye tracking and 72/90 Hz modes. | Pimax product documentation | Lens/gaze-space optimization has hardware inputs and high pixel stakes. |
| E-012 | Hardware fact | RTX 5090 has 32 GB GDDR7 and 1792 GB/s rated bandwidth. | NVIDIA product documentation | Capacity/bandwidth are not first suspects without counter evidence. |
| E-013 | Measurement | Render Scale Mode on/off showed no measurable steady benefit on the current hardware within the measured noise floor. | `CSX_RENDER_SCALE_REQUEST.md` | Do not assume physical target scaling solves the steady bottleneck on this rig. |
| E-014 | Source/measurement | Render-scale quality changes rebuild physical engine/CS targets and create a transition hitch. | CSX source plus Governor captures | Important transition defect; separate from steady-frame research. |

## Open claims

| ID | Claim | Status | Decisive evidence needed |
|---|---|---|---|
| C-001 | The RTX 5090 is starved during a meaningful fraction of RC3 frames. | Open | GPUView/Nsight queue occupancy aligned with render thread. |
| C-002 | Second-eye work is the largest recoverable application cost. | Open | Per-eye timing/counters and safe-reprojection fraction. |
| C-003 | Pimax compositor timing explains under-budget late intervals. | Open | Cross-process GPU trace and runtime deadline alignment. |
| C-004 | Uniform rectangular overshading is still large after crop/foveated upscaling. | Open | Visible-mask/per-pass hardware counter experiment. |
| C-005 | One MGO asset/configuration causes a disproportionate tail cost. | Open | Top-draw/overdraw/asset attribution in representative bad scenes. |

## Corrections policy

Never silently rewrite a ledger row. If evidence changes:

1. preserve the old statement in the referenced capture/document;
2. add a corrected row or mark the claim refuted;
3. state the methodological error;
4. identify every decision that depended on it.

