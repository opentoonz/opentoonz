# Vector alignment and distribution

Adapted for OpenToonz from [Tahoma2D PR #1275](https://github.com/tahoma2d/tahoma2d/pull/1275),
**Vector Control Point/Stroke Alignment Commands**, by **John Dancel (manongjohn)**.
The original panel, icons and alignment behavior are the source of this port.
The commit preserves co-author credit; the applicable BSD copyright notice is
retained in `LICENSE.txt`.

Open **Windows > Align and Distribute Panel**. With the Selection Tool, select
vector strokes or groups. With the Control Point Editor Tool, select control
points on one stroke. The panel offers left, right, top, bottom, two center
alignments, and horizontal/vertical distribution. All eight commands can also
be assigned shortcuts or added to a command bar. Set Linear Control Point and
Set Nonlinear Control Point are registered commands as well.

Existing saved Windows room menus gain the panel entry when loaded. There is
no need to reset room layouts or remove customized menus. A panel entry already
present in a submenu keeps its existing placement and label.

## References and scope

- Stroke alignment: Selection Area, First Selected, Last Selected, Smallest
  Object, Largest Object, or Camera Area.
- Control points: Selection Area, First Selected, or Last Selected.
- Distribution: centers are evenly spaced between the two outer objects,
  which stay fixed. Camera Area distribution puts the outer object edges on
  the camera bounds and evenly spaces the centers.
- A group counts as one object. Entered groups are respected; nested groups
  move together without changing internal positions.
- First/Last Selected uses click order. Bulk selections have the existing
  deterministic stroke/control-point index order.
- The displayed vector drawing is edited. Multi-frame and whole-level
  selections are deliberately excluded; raster drawings are unsupported.
- Camera bounds use the current animated camera and column placement. A
  rotated camera is represented by its axis-aligned bounds in drawing space.
- Read-only levels/frames, locked columns, and active editing drags are
  guarded. Commands that would change nothing add no undo entry.

OpenToonz's ordered sets remain the selection API. A separate order list is
used only for alignment references, preserving existing copy/paste,
deformation, grouping and thickness code. Stroke edits notify the vector
image of changed geometry and preserve fill information in undo/redo.
Point edits use the existing editor's handle movement and undo implementation;
selection-preserving undo is enabled only for the new alignment operations.
Panel state never enables or disables another component's global actions.

## Validation

The standalone numerical regression checks cover all eight operations,
reference methods, camera distribution with two objects, coincident centers,
empty/single selections and zero-area control-point bounds. From the repo root:

```sh
g++ -std=c++17 -DLINUX -Itoonz/sources/include \
  toonz/sources/tnztools/tests/vectoralignment_test.cpp \
  -o /tmp/vectoralignment_test
/tmp/vectoralignment_test
```

Local validation also includes C++17/Qt 5.15 syntax compilation of all changed
implementation files, Qt moc/resource generation, XML/resource checks and
clang-format 14 checks. An offscreen Qt menu-loading check covers saved-menu
migration, preserved labels/order, nested panel entries, duplicate handling,
and all seven shipped menus containing a Windows menu. A complete application
build and interactive Windows testing are separate from these checks.

Interactive verification before merging:

1. Align individual strokes and nested groups with each reference method;
   confirm first/last selection follows click order and groups retain shape.
2. Distribute three strokes or points; confirm the two outer centers stay
   fixed. Distribute two objects within Camera Area; confirm finite results.
3. Undo/redo repeatedly, including after changing the current frame/tool;
   verify fills, selection bounds, handles and saved/reloaded geometry.
4. Try raster, locked/read-only and multi-frame selections. Switch tools and
   open/close multiple panels; verify availability and shortcut behavior.
5. Verify Camera Area with animated camera/column transforms and a zero-scale
   column. Existing scene renders change only after an explicit edit command.

AI assistance: Codex (GPT-6) adapted and reviewed the port in response to the
request to extract the pertinent code from PR #1275 for OT-Dev and credit
manongjohn. Interactive application behavior remains to be verified.
