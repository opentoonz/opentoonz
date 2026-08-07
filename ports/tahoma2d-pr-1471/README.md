# Port workspace: Tahoma2D PR #1471 — Onion Skin Opacity control

Upstream pull request: https://github.com/tahoma2d/tahoma2d/pull/1471

Original author: @manongjohn

Upstream commits:

- `e519635f45e36ea5ed76ebda36b56063c08ef5b0` — Onion Skin Opacity control
- `8bf7d3766e9f79a6c84c8d8e100657fa10e6bbf2` — Prioritize higher opacity
  with overlapping onion-skin markers

Target base used to start this port:

- `OpenAnimationLibrary/opentoonz-dev@93591170f157640cb7b1f17f2678394d1c2af4f6`

## Current status

The feature is adapted to current OpenToonz source. The exact upstream
mail-formatted patch remains in `upstream.patch`, including the original author
and commit metadata.

## OpenToonz adaptations

1. Marker state stores true opacity in the `[0, 1]` range; `-1` selects the
   existing automatic fade calculation.
2. Fixed and relative markers at the same frame are de-duplicated and use the
   larger custom opacity.
3. Removing a marker verifies the requested position, so removing a missing
   marker cannot erase its neighbor.
4. `isMos()`, `isFos()`, and opacity getters remain const-correct.
5. The popup uses OpenToonz's `contextMenuEvent()` path and supports both Xsheet
   and Timeline orientation.
6. Popup updates use the onion-skin handle notification and do not mark the
   scene as modified.
7. The unrelated Column Transparency behavior change from the upstream patch
   is not included.
8. Custom opacity is propagated through vector, raster, Toonz Raster, guided
   drawing, OpenGL, and plastic-deformation rendering paths.

## Validation checklist

- Fixed and relative markers can each use Auto or a custom value.
- Overlapping fixed/relative markers choose the more visible result as intended.
- Removing a nonexistent marker does not remove a neighboring marker.
- Xsheet and Timeline popup placement/orientation are correct.
- Vector, Toonz Raster, Raster, mesh/plastic deformation, and OpenGL drawing paths agree.
- Shift and Trace, Guided Drawing, whole-scene onion skin, and light-table behavior do not regress.
- Existing scenes load without altered onion-skin marker behavior.
- Windows, macOS, and Linux builds compile cleanly.

## Reproducing the source commits locally

```bash
git remote add tahoma2d https://github.com/tahoma2d/tahoma2d.git
git fetch tahoma2d e519635f45e36ea5ed76ebda36b56063c08ef5b0 8bf7d3766e9f79a6c84c8d8e100657fa10e6bbf2
git cherry-pick -x e519635f45e36ea5ed76ebda36b56063c08ef5b0
git cherry-pick -x 8bf7d3766e9f79a6c84c8d8e100657fa10e6bbf2
```

Resolve conflicts by porting the individual feature hunks according to the
notes above; do not accept complete Tahoma2D versions of the affected files.
