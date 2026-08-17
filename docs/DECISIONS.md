# Decisions Log

## D-001 — Separate the project from Governor and Community Shaders

Date: 2026-08-16

Decision: create an independent frame-pipeline research project.

Reason: the suspected bottlenecks cross process and repository boundaries. Housing
the work in the Governor would bias the project toward control-policy fixes;
housing it in Community Shaders would bias it toward shader fixes and omit runtime
delivery.

## D-002 — Do not name a solution before the end-to-end trace

Date: 2026-08-16

Decision: stereo reuse is the leading hypothesis, not the implementation plan.

Reason: under-budget late intervals could be caused by CPU/runtime timing, and the
large fitted floor can contain GPU idle gaps as well as real fixed render work.

## D-003 — Maintain separate throughput and delivery models

Date: 2026-08-16

Decision: application GPU P95 and frame delivery are separate primary signals.

Reason: application GPU work is actionable for quality and rendering cost, but it
does not prove which frame the Pimax runtime displayed.

## D-004 — Treat the old whole-frame stereo prototype as prior art only

Date: 2026-08-16

Decision: reuse its hooks and lessons, not its correctness assumptions.

Reason: it was disabled by default, has material/disocclusion risk, and lacks a
current RC3 performance and binocular-quality proof.

## D-005 — Instrument stereo submission before selecting a reuse backend

Date: 2026-08-16

Decision: make the first stereo-duplication deliverable a passive, bounded
observer of engine phases and actual D3D11 `Draw*`/`Dispatch` traffic.

Reason: CPU preparation, API submission, vertex work, pixel work, and
screen-space compute need different optimizations. Buffer layout and two-eye
matrices do not prove which cost is duplicated or limiting.

## D-006 — Keep deferred contexts and engine-wide multi-view conditional

Date: 2026-08-16

Decision: do not make deferred command lists or broad single-pass stereo the
foundation. Evaluate them only after traces prove an isolated CPU-recording or
geometry/submission bottleneck.

Reason: command-list replay does not remove GPU draws and has mutable-state and
ordering constraints; engine-wide multi-view has a much larger shader, culling,
material, and compatibility surface.

## D-007 — Use a CSX research fork, not a competing standalone D3D11 plugin

Date: 2026-08-17

Decision: develop tracing and the first shared-stereo proof in a user-owned fork
of ParticleTroned CSX. Pin reproducible MGO RC3 measurements to tag `CSX3.18`,
then forward-port proven commits to current `main-VR`.

Reason: CSX already owns the immediate context, engine render hooks, shader and
eye state, frame diagnostics, and an extensive D3D11 hook bank. A separate plugin
would duplicate detours and create avoidable hook-order and resource-ownership
risk.

## D-008 — Defer a companion SKSE plugin until an engine-only boundary is proven

Date: 2026-08-17

Decision: create a separate `StereoFusionVR` companion only if the selected
optimization requires Skyrim visibility/draw-packet hooks that ParticleTroned
does not want inside CSX.

Reason: one renderer owner is safer. If separation becomes necessary, the
companion should exchange only versioned metadata through a small CSX VR API and
must not independently detour the same D3D11 methods.

## D-009 — Ship as a reversible MO2 CSX override

Date: 2026-08-17

Decision: package StereoFusion as a separate MO2 mod that overrides only its
modified CSX DLL, shaders, and configuration. Disabling the mod and restarting
exposes the untouched underlying Community Shaders installation.

Reason: a modified CSX DLL cannot be safely unloaded from a running game, but MO2
file virtualization gives a simple, auditable, and recoverable activation model.
Runtime `Off` and startup-safe configuration add escape paths.

## D-010 — Build StereoCapabilityLab before the geometry POC

Date: 2026-08-17

Decision: build a standalone D3D11 capability, correctness, and performance
executable before modifying Skyrim geometry submission.

Reason: current public NVAPI does not expose an obvious supported SPS/MVR API,
and generic instanced or geometry-shader stereo can help or regress depending on
driver and workload. The lab answers platform questions; in-game StereoTrace
remains necessary for Skyrim-specific opportunity.

## D-011 — Make the first lab implementation standard-D3D11-only

Date: 2026-08-17

Decision: implement B0-B3 with native D3D11 draws, instance expansion,
clip-distance seam restoration, and geometry-shader viewport routing. Do not
link NVAPI or any game/runtime component. Compile in GitHub Actions and execute
the deployable artifact on the game PC.

Reason: this isolates the platform question, keeps redistribution and fallback
simple, and makes a negative result useful. Vendor-specific work remains optional
only after an official supported interface is identified.

Every invocation must create an evidence directory before device creation and
retain lifecycle status, exact configuration, adapter/driver/build identity,
debug messages, validation images, and raw timing rows. Failed runs are data, not
disposable console output.

## Pending decisions

- Which tracing tool and instrumentation combination yields the least overhead?
- Can a delivered-frame identifier be obtained from the Pimax runtime or ETW?
- Is H1 best attacked at shared CPU visibility, multi-view geometry, selective
  second-eye shading, or only selected CS effects?
- Which representative scenes become the permanent acceptance suite?
