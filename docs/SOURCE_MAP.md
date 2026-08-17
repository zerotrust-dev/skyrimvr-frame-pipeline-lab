# Source Map

## Local primary evidence

### Governor

- [Governor repository](https://github.com/zerotrust-dev/CSQualityGovernorVR)
- [Governor design](https://github.com/zerotrust-dev/CSQualityGovernorVR/blob/main/docs/GOVERNOR_DESIGN.md)
- [Proposed safe ladder](https://github.com/zerotrust-dev/CSQualityGovernorVR/blob/main/docs/PROPOSED_SAFE_LADDER_GOVERNOR.md)
- [RC3 capture index](https://github.com/zerotrust-dev/CSQualityGovernorVR/tree/main/tests/data)
- [RC3 2026-08-12 17:10](https://github.com/zerotrust-dev/CSQualityGovernorVR/tree/main/tests/data/rc3-2026-08-12-1710)
- [OpenComposite frame timing audit](https://github.com/zerotrust-dev/CSQualityGovernorVR/blob/main/docs/OPENCOMPOSITE_FRAME_TIMING.md)
- [Render Scale Mode request](https://github.com/zerotrust-dev/CSQualityGovernorVR/blob/main/docs/CSX_RENDER_SCALE_REQUEST.md)

### Community Shaders research

- Research overview and architecture map: local evidence archive, not included in
  this public repository.
- [ParticleTroned CSX source](https://github.com/ParticleTroned/skyrim-community-shaders)
- [Earlier stereo optimization header](https://github.com/zerotrust-dev/skyrim-community-shaders/blob/169191563e227c68ce0ba79fb28d08de98c62905/src/Features/VRStereoOptimizations.h)
- [Earlier stereo optimization implementation](https://github.com/zerotrust-dev/skyrim-community-shaders/blob/169191563e227c68ce0ba79fb28d08de98c62905/src/Features/VRStereoOptimizations.cpp)

Exact current source authority used by the stereo submission design:

```text
ParticleTroned tag CSX3.18
commit 2051e2aead1b2bb2b03faa421201376e8bc84fe0
CSBuildNumber 11
```

The repository's active worktree is on an older revision. Exact CSX 3.18 claims
must use the tagged Git object (`git show CSX3.18:...`) or an immutable commit URL,
not the currently checked-out file. Match the installed DLL to a release artifact
before treating source equivalence as binary provenance.

Fork/base status verified 2026-08-17:

```text
ParticleTroned default branch: main-VR
main-VR commit checked:        a0a6218b0606a3e6f5f19c599880703eedf2dd9a
main-VR vs CSX3.18:            11 commits ahead at time checked
license:                       GPL-3.0-or-later plus repository exceptions
```

- [ParticleTroned CSX repository](https://github.com/ParticleTroned/skyrim-community-shaders)
- [Immutable CSX3.18 commit](https://github.com/ParticleTroned/skyrim-community-shaders/tree/2051e2aead1b2bb2b03faa421201376e8bc84fe0)

### Runtime/crop/foveation research

- [Custom OpenXR Toolkit](https://github.com/zerotrust-dev/custom_OpenXR_Toolkit)
- [Experiment index](https://github.com/zerotrust-dev/custom_OpenXR_Toolkit/tree/main/experiments)
- [Frame motion logger](https://github.com/zerotrust-dev/custom_OpenXR_Toolkit/tree/main/experiments/frame-motion-logger)
- [Pimax geometry probe](https://github.com/zerotrust-dev/custom_OpenXR_Toolkit/tree/main/experiments/pimax-geometry-probe)
- [Visibility-mask logger](https://github.com/zerotrust-dev/custom_OpenXR_Toolkit/tree/main/experiments/visibility-mask-logger)
- [Foveation live control](https://github.com/zerotrust-dev/custom_OpenXR_Toolkit/tree/main/experiments/foveation-live-control)

### Historical tuning

- Community Shaders ROI notes and configuration tuning: local evidence archive,
  not included in this public repository.

## External authoritative sources

- [Khronos OpenXR 1.1 specification](https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html)
- [Microsoft D3D11 device/context model](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-intro)
- [Microsoft D3D11 multithreading](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-intro)
- [Microsoft D3D11 command-list recording](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-render)
- [NVIDIA Single-Pass Stereo](https://developer.nvidia.com/vrworks/graphics/singlepassstereo)
- [NVIDIA Multi-View Rendering](https://developer.nvidia.com/vrworks/graphics/multiview)
- [NVIDIA Lens-Matched Shading](https://developer.nvidia.com/vrworks/graphics/lensmatchedshading)
- [NVIDIA Multi-Resolution Shading](https://developer.nvidia.com/vrworks/graphics/multiresshading)
- [NVIDIA RTX 5090](https://www.nvidia.com/en-us/geforce/graphics-cards/50-series/rtx-5090/)
- [Pimax Crystal Super](https://pimax.com/products/pimax-crystal-super)
- [ParticleTroned CSX VR branch](https://github.com/ParticleTroned/skyrim-community-shaders/tree/csx-3-VR)
- [NVIDIA Streamline DLSS guide](https://github.com/NVIDIAGameWorks/Streamline/blob/main/docs/ProgrammingGuideDLSS.md)

## Source handling rules

1. Prefer installed source/build matches over repository default branches.
2. Record branch and commit, not just repository URL.
3. Do not assume `master` matches a distributed OpenComposite archive.
4. Treat reverse-engineered Skyrim behavior as a hypothesis until traced on the
   installed executable.
5. Preserve exact source snippets or commit references for decisions that result
   in code changes.
