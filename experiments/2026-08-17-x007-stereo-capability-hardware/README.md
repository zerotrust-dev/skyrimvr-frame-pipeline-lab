# X-007 — StereoCapabilityLab CI and RTX 5090 qualification

Date: 2026-08-17

Status: five-process balanced qualification passed; debug-layer prerequisite missing

## Identity

```text
source commit:       a5ba5cef034266c8285724069bfbf8fd6e495d8a
binary SHA-256:      fa2b11601ecdbe73cd8da6a3d24adc77adaeacddc82fccb066d098505a639500
adapter:             NVIDIA GeForce RTX 5090
DXGI driver:         32.0.16.1062
NVIDIA display:      610.62
feature level:       11_1
```

GitHub Actions run:
[32037552414](https://github.com/zerotrust-dev/skyrimvr-frame-pipeline-lab/actions/runs/32037552414)

## CI evidence

- MSVC Release compile: pass.
- CPU evidence/schema self-test: pass.
- D3D11 WARP smoke: pass.
- B0-B3 color/depth validation: pass.
- WARP performance is correctness/control evidence only.

## RTX 5090 runs

### Hardware smoke

Run `20260817T140452.795Z-p27124`, 320x320 per eye, S1, 100 objects,
5 warmup and 30 measured frames:

- B0-B3: zero color, depth, eye and seam mismatches.
- All backends: 30/30 valid GPU samples.
- Performance numbers retained but not promoted because this matrix is too small.

### Debug validation attempt

Run `20260817T140525.688Z-p19244` failed before rendering because the Windows
Graphics Tools optional capability/D3D11 debug layer is not installed. The
failed run and exact reason are preserved. This is a machine prerequisite, not a
backend correctness failure.

### 1280 correctness

Run `20260817T140547.347Z-p16272`, 1280x1280 per eye, 128 validation objects:

- B0, B1, B3: exact color and depth match.
- B2: zero pixels outside tolerance, maximum channel delta 1, zero depth and
  seam errors.

### First full S1 process

Run `20260817T140632.846Z-p9764`, 1280x1280 per eye, 1,000 objects,
300 warmup and 2,000 measured frames:

| Backend | CPU submit median | GPU median | Valid samples |
|---|---:|---:|---:|
| B0 | 40.50 us | 44.128 us | 2000/2000 |
| B1 | 40.50 us | 44.080 us | 2000/2000 |
| B2 | 1.80 us | 5.216 us | 2000/2000 |
| B3 | 1.90 us | 5.808 us | 2000/2000 |

B0 and B1 use the same draw-submission path. Their identical CPU medians and
0.11% GPU-median difference are a useful within-process control. A few B0
outliers inflated its CPU mean to 55.88 us and GPU mean to 52.50 us, while B1
means remained 40.86/38.47 us. Therefore the original mean-based promotion
calculation was not robust enough.

## Correction triggered by this experiment

The gate now:

1. uses CPU/GPU medians for B2/B3 comparisons;
2. requires B0/B1 identical-submission medians to agree within 5%;
3. blocks promotion when that control is absent or fails;
4. continues to report mean, P95 and P99 so tails remain visible.

## Interpretation

B2 and B3 are correct standard-D3D11 synthetic candidates on this RTX 5090.
B2 is the current leading one-call candidate because it avoids a geometry stage
and has the lower GPU median in this process. The large savings are an upper
bound created by collapsing 2,000 tiny synthetic draws into one instance domain;
they are not an estimate of Skyrim or MGO performance.

No in-game promotion occurs until:

- a matching debug-layer run has zero errors;
- process-level repetitions reproduce the result at the representative matrix;
- StereoTrace proves a reachable, material-safe duplicated region in Skyrim/CSX.

## Gate verification and order-effect discovery

CI run
[32038392042](https://github.com/zerotrust-dev/skyrimvr-frame-pipeline-lab/actions/runs/32038392042)
passed for commit `aa335c445f90fe80b4a827d88f6d61674cf44499`. The
CI-built binary was then run twice on the RTX 5090 with 2,000 measured frames
per backend.

Normal order (`B0,B1,B2,B3`), run `20260817T143818.816Z-p36392`:

- B0/B1 CPU median delta: +3.60%;
- B0/B1 GPU median delta: -22.39%;
- control: fail, so B2/B3 promotion was correctly blocked.

Reversed control order (`B1,B0`), run `20260817T143928.749Z-p35220`:

- B1, now first, became slower than B0;
- B0/B1 CPU median delta: +34.90%;
- B0/B1 GPU median delta: +10.74%;
- control: fail.

The slower result following the first measured backend demonstrates temporal
order contamination rather than a B0/B1 rendering difference. Raw 250-frame
windows also show substantial within-backend drift for the two high-submission
controls, while B2/B3 remain stable.

The benchmark scheduler therefore now uses balanced 25-frame blocks
with a rotating start backend and alternating traversal direction. Every CSV
sample records its schedule round, position, and intra-block frame.

## Balanced-schedule qualification

CI run
[32040238189](https://github.com/zerotrust-dev/skyrimvr-frame-pipeline-lab/actions/runs/32040238189)
passed for scheduler commit `6838721fb99dd4cb29d151c871e157163221808b`.
The packaged binary SHA-256 is
`8d0d94a9452e824920604536748189009eba088a72b18bf1b2b886a2d347a416`.

Five independent RTX 5090 processes used S1, 1280x1280 per eye, 1,000 objects,
300 balanced warmup frames and 2,000 balanced measured frames per backend:

| Run | CPU control | GPU control | B2 CPU saving | B2 GPU change | B3 CPU saving | B3 GPU change |
|---|---:|---:|---:|---:|---:|---:|
| `150304-p14220` | +1.23% | -0.34% | 99.01% | -80.15% | 99.01% | -74.61% |
| `150329-p42360` | 0.00% | +0.07% | 98.68% | -79.69% | 98.68% | -74.91% |
| `150331-p11120` | +0.76% | -0.67% | 98.99% | -80.11% | 98.99% | -75.13% |
| `150333-p28196` | -0.25% | -0.81% | 99.01% | -80.03% | 99.01% | -75.17% |
| `150336-p31076` | 0.00% | -0.14% | 98.69% | -79.84% | 98.69% | -74.93% |

All five evidence directories pass the strengthened schema/schedule validator;
all B0-B3 validations pass; all 40,000 GPU timing samples are valid; and B0/B1 passes
the identical-submission control in every process. The balanced scheduler has
therefore removed the measured order confound at this matrix. B2 is the leading
synthetic candidate because it consistently has the lower GPU median without a
geometry shader.

These percentages remain a synthetic upper bound, not a prediction of Skyrim
FPS. In-game promotion is still blocked by the missing debug-layer qualification
and by the need for StereoTrace to prove a reachable, material-safe duplicated
region in Skyrim/CSX.
