# Experiment X-001 — End-to-End Late-Frame Trace

Date planned: 2026-08-16
Status: **planned; instrumentation design not yet frozen**

## Question

When an RC3 application interval is late while the Governor's application GPU
sample is under 13.8889 ms, where was the time actually lost?

## Hypotheses

- **H1:** the GPU was starved by Skyrim/driver command production.
- **H2:** useful app work completed outside the Pimax runtime's admission window,
  or compositor work/preemption consumed the apparent margin.
- **H6:** a subset of late intervals coincides with paging, streaming, shader
  creation, or another transient system event.

## Falsifiers

- H1 weakens if the application GPU queue is continuously occupied until useful
  work completes and the render thread does not precede idle gaps.
- H2 weakens if under-budget samples are shown to be frame-misaligned/stale or if
  every late interval is fully explained inside Skyrim before final-eye submit.
- H6 weakens if late intervals have no correlated I/O, hard faults, WDDM eviction,
  resource creation, or shader compilation.

## Required event vocabulary

Every event needs a CPU QPC timestamp, process/thread ID, and the best available
shared frame/cycle identity.

```text
governor_sample_publish
governor_interval_complete
wait_get_poses_enter
wait_get_poses_exit
xr_wait_frame_enter
xr_wait_frame_exit
xr_begin_frame_enter
xr_begin_frame_exit
first_game_draw
eye0_submit_enter
eye0_submit_exit
eye1_submit_enter
eye1_submit_exit
xr_end_frame_enter
xr_end_frame_exit
present_enter
present_exit
```

Also record:

```text
predicted_display_time
predicted_display_period
should_render
quality_rung
application_gpu_sample_id/value
render-scale owner/mode
CS transition state
submitted eye dimensions
```

## ETW/GPU data

Capture enough providers to reconstruct:

- CPU scheduling and wait reasons for Skyrim main/render threads;
- D3D11/DXGI API and present activity;
- WDDM hardware queues, contexts, packets, preemption and residency;
- NVIDIA GPU engine activity;
- Pimax runtime/compositor processes and contexts;
- disk I/O and hard page faults;
- process/thread lifetime and module identity.

The exact provider set and capture command are deliberately pending. They must be
validated on a short run for overhead and file growth before the real session.

## Controlled scene

Use two passes:

1. **Static:** fixed camera in a scene where Performance normally stays close to
   the 72 Hz boundary.
2. **Motion:** repeatable slow and fast head rotation through the same view.

Hold the quality rung fixed for the primary classification run. A changing rung
introduces relatch/recovery behavior that belongs to a different experiment.

## Procedure draft

1. Record the complete stack signature and hashes.
2. Reboot or otherwise establish the same clean-start policy used by control runs.
3. Run a 60-second no-trace control and record Governor samples.
4. Run a short trace-overhead calibration.
5. Run at least 120 seconds static and 120 seconds controlled motion.
6. Mark scene/motion phase boundaries with explicit events.
7. Preserve the raw ETL and Governor logs before analysis.
8. Join data by frame/cycle identity; use wall clock only as a diagnostic fallback.

## Classification model

Assign every investigated late interval one primary class:

```text
A  app_gpu_busy_over_deadline
B  gpu_starved_waiting_for_app_or_driver
C  app_cpu_late_before_gpu_work
D  app_complete_runtime_or_compositor_late
E  gpu_preempted_or_contended_by_other_context
F  streaming_paging_or_resource_event
G  transition_or_known_exception
U  unknown_or_identity_ambiguous
```

Multiple contributing flags may accompany the primary class.

## Quality gates

- Trace overhead changes P95 application interval by no more than the declared
  tolerance established in the calibration.
- Governor GPU samples are deduplicated by sample identity.
- At least 200 late intervals are captured, or the run is explicitly reported as
  too clean to classify.
- No quality transitions occur in the primary fixed-rung windows.
- At least 95% of selected intervals have unambiguous event ordering before the
  experiment is used for an architecture decision.
- Pimax GPU contexts are identified by process/context evidence, not process-name
  guesswork alone.

## Planned outputs

- Timeline diagram for representative frames in each class.
- Counts and percentages by class.
- CPU ready/running/wait distributions.
- App and Pimax GPU queue overlap/idle distributions.
- Completion offset relative to predicted display time.
- List of ambiguous/missing boundaries.
- Evidence-ledger updates for C-001 and C-003.

## Artifacts

Not yet captured.

