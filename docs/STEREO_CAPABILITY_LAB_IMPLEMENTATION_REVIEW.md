# StereoCapabilityLab Implementation Review

Date: 2026-08-17  
Status: source-complete first pass; GitHub/MSVC and hardware evidence pending

## Review conclusion

The first L0-L4 implementation is suitable for CI compilation and standalone
qualification. It remains deliberately separate from Skyrim and Community
Shaders and uses only redistributable Windows/D3D11 interfaces.

It can reject an unsuitable one-call technique and can identify a synthetic
candidate. It cannot yet prove a real MGO benefit or authorize an in-game broad
hook.

## Implemented

- B0 eye-major two-draw correctness/timing oracle.
- B1 shared invariant CPU packet construction with the same native eye draws.
- B2 one-call eye-expanded instancing using side-by-side clip packing.
- B3 one-call instanced world vertices plus GS eye replication and
  `SV_ViewportArrayIndex`.
- Asymmetric eye transforms, edge-crossing objects, depth intersections, odd
  validation instance count, per-eye and seam error attribution.
- Batch-delayed timestamp/disjoint queries; invalid samples are blank and marked
  invalid, never converted to zero.
- Lossless reference/candidate/diff PPM plus depth PGM output.
- Unique per-process evidence directory created before D3D device creation.
- Exact binary, runtime shader, source, configuration, adapter, driver, process,
  power, affinity and timing identities.
- Human log, JSONL events, append-only lifecycle, explicit status, CSV samples,
  debug-layer dump, and generated decision summary.
- GitHub Actions MSVC build, CPU-only self-test, WARP B0-B3 smoke test, schema
  validator, deployable ZIP, and failure diagnostics retained with `always()`.

## Important correction found during review

A naive B2 side-by-side transform is not generally correct. Packing eye clip
space into one full-width viewport removes each eye's inner X clip plane, so a
boundary-crossing primitive can bleed across the stereo seam.

B2 therefore exports a signed `SV_ClipDistance0`:

```hlsl
leftEyeDistance  = clip.w - originalClipX;
rightEyeDistance = originalClipX + clip.w;
```

The ordinary outer clip planes remain equivalent after packing. Validation now
contains boundary-crossing geometry and rejects any above-tolerance seam pixel,
even if its fraction would fit inside the global raster tolerance.

## Measurement safeguards

- CPU preparation, submission and total time are separate.
- Query objects are created before measurement.
- The current frame is never polled from the render loop.
- One explicit context flush occurs after a backend batch; `GetData` then uses
  `D3D11_ASYNC_GETDATA_DONOTFLUSH` with a finite deadline.
- Readback, comparison, images, logs, and CSV serialization are outside measured
  submission intervals.
- Debug-layer and WARP numbers are labeled non-performance evidence.
- A release performance result cannot claim the debug correctness gate. It must
  be paired with a separate zero-error debug run by binary/shader/configuration
  signatures.

## CI and hardware sequence

1. Commit/push the source and workflow.
2. Require `windows-msvc` to configure and compile successfully.
3. Require the CPU evidence self-test and WARP B0-B3 smoke run to pass.
4. Download the CI artifact on the game PC.
5. Run `Smoke` on the RTX 5090.
6. Run `Validation` with `-DebugLayer` at 1280x1280 and then the MGO reference
   resolution.
7. Run release `Benchmark` for S1-S3 at 1280, 2448, 3494x3558, and 3840 per eye,
   with at least five fresh processes per retained matrix point.
8. Pair signatures, examine raw distributions and images, then apply the 15% CPU
   saving / <=5% GPU regression synthetic gate.
9. If a backend qualifies, proceed only to one allowlisted CSX shader-family POC
   with StereoTrace; otherwise retain B1 CPU preparation reuse as the safer line.

## Explicit pending work

- Native MSVC compile diagnostics (GitHub Actions).
- WARP and RTX 5090 images/timings.
- S4 existing-instancing stress beyond the odd-count mapping smoke case.
- S5 state/resource, alpha, culling, winding, scissor and indirect-draw matrix.
- B4 deferred-context experiment.
- Automated cross-process matrix aggregation and signature join.
- Skyrim/CSX hook reachability, material allowlist and compositor delivery proof.

None of these pending items is silently treated as passed by the generated
summary.

