# X-007 — StereoCapabilityLab CI and RTX 5090 qualification

Date: 2026-08-17

Status: first hardware process passed; debug-layer prerequisite missing

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
