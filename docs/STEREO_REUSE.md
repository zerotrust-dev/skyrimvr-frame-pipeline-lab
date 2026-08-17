# Stereo Reuse Research

## Why this is the leading structural hypothesis

The two eyes view the same world from nearby origins. Most distant and opaque
content is shared; differences concentrate around near geometry, silhouette
disocclusions, transparencies, particles, view-dependent materials, and the
foveal region where errors are most visible.

A conventional renderer repeats work because it treats each view as independent.
The research opportunity is to quantify and exploit the actual overlap.

## Three distinct reuse levels

Do not collapse these into one proposal.

### Level A — shared CPU visibility and draw preparation

Build a conservative union visibility set once, share object/material preparation,
and retain per-eye projection/culling only where required.

Potential saving:

- scene traversal;
- animation/material preparation;
- draw sorting and state preparation;
- some driver submission overhead.

Primary risk: union visibility can draw more geometry than either eye alone and
may move rather than remove cost.

### Level B — hardware multi-view geometry

Submit geometry once and project it into both eyes with single-pass stereo or
multi-view rendering.

Potential saving:

- draw calls;
- vertex, tessellation, and geometry processing;
- some state changes.

Primary risk: Skyrim and Community Shaders shader permutations, constant buffers,
screen-space layouts, culling, shadows, and custom materials assume the existing
side-by-side VR contract.

### Level C — depth-aware shading reuse

Shade one eye, classify which pixels are safe to transfer, reproject them into the
other eye, and shade only uncertain/disoccluded pixels in that eye.

Potential saving:

- material/pixel shading;
- portions of deferred composition;
- selected screen-space effects;
- distant/low-disparity lighting.

Primary risks:

- disocclusion holes;
- transparent and particle ordering;
- POM/parallax depth mismatch;
- view-dependent specular and reflections;
- thin geometry and alpha test;
- incorrect motion vectors/history;
- eye dominance and binocular rivalry;
- foveal artifacts during eye motion.

## Existing prior art in the checked source

### Current CSX selective reuse

The exact CSX 3.18 tag contains selective stereo synchronization/reprojection in:

- `src/Features/ScreenSpaceGI.cpp`;
- `src/Features/ScreenSpaceShadows.cpp`.

Screen-space shadows can skip the native right-eye ray-march when stereo
reprojection is enabled and ready, then run a stereo reproject/sync pass. SSGI has
explicit stereo-reproject, stereo-sync, and center-stereo-sync paths. This proves
current pass-local reuse is reachable; it does not prove that Skyrim's scene
submission or general material rendering is shared.

### Earlier whole-render prototype

The local research source contains an earlier `VRStereoOptimizations` feature:

- [Header](https://github.com/zerotrust-dev/skyrim-community-shaders/blob/169191563e227c68ce0ba79fb28d08de98c62905/src/Features/VRStereoOptimizations.h)
- [Implementation](https://github.com/zerotrust-dev/skyrim-community-shaders/blob/169191563e227c68ce0ba79fb28d08de98c62905/src/Features/VRStereoOptimizations.cpp)

Its pipeline was:

1. classify right-eye pixels from depth;
2. write safe regions into stencil;
3. stencil-skip right-eye shading;
4. reconstruct skipped right-eye pixels from the left eye;
5. preserve full shading or blending around disocclusions and sensitive regions.

It was disabled by default and later disappeared when that upstream line removed
VR support. It is evidence of reachability, not evidence of visual correctness or
net performance.

## Measurement before implementation

The first stereo experiment must produce these quantities:

```text
T_eye0_geometry
T_eye1_geometry
T_eye0_pixel
T_eye1_pixel
T_shared_shadow
T_per_eye_CS
safe_reproject_pixel_fraction
disoccluded_pixel_fraction
transparent_or_unsupported_fraction
foveal_safe_fraction
reprojection_classification_cost
```

The opportunity estimate is then bounded by:

```text
recoverable_ms <= duplicated_eye_work * safe_reproject_fraction
                 - classification_and_reconstruction_cost
```

This prevents the project from assuming that a large visual overlap automatically
means a large timing win.

## Correctness matrix

Any prototype must be tested against:

- hands, weapons, bodies, VRIK and near NPCs;
- foliage, fences, hair, alpha test and transparency;
- particles, magic, fire and smoke;
- POM/parallax and extended materials;
- water, refraction, wetness and reflections;
- screen-space shadows and GI;
- rapid translation, rotation and eye movement;
- menus, loading screens and overlays;
- both-eye screenshot difference and in-headset binocular comfort.
