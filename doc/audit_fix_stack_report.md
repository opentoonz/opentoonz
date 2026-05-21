# Audit Fix Stack Report

This stack contains the high-confidence fixes from the code audit that were not
already covered by current upstream work. Each behavioral or security change was
kept in a separate commit so maintainers can review, reorder, or drop items
independently.

## Upstream overlap check

- Auto Close performance was already addressed upstream by merged PR
  https://github.com/opentoonz/opentoonz/pull/6340. The merged code is already
  present in this branch's base, so no duplicate performance commit was added.
- Scene asset importing is covered by open draft PR
  https://github.com/opentoonz/opentoonz/pull/6159. The audit confirmed the
  same affected path, but this stack does not duplicate that draft.
- TZL icon-specific crash handling is covered by open draft PR
  https://github.com/opentoonz/opentoonz/pull/6703. This stack's TZL commit is
  broader header/table validation for malformed TLV files; maintainers should
  reconcile it with that draft if both proceed.

## Open pull request mapping

This section calls out every open pull request that this audit stack either
touches or intentionally avoids. Status was checked on 2026-05-21.

| Open PR | Status | Stack commit | Relationship |
| --- | --- | --- | --- |
| [#6159: Fix scene assets importing](https://github.com/opentoonz/opentoonz/pull/6159) | Open draft | None | The audit found the same scene-import asset behavior, but no stack commit addresses it. This remains intentionally deferred to the existing draft PR. |
| [#6703: TZL icon buffer overflow and savebox exception](https://github.com/opentoonz/opentoonz/pull/6703) | Open draft | `92066d5e9` | Partial overlap in `toonz/sources/image/tzl/tiio_tzl.cpp`. This stack commit validates TLV/TZL header and offset tables more broadly; it does not replace PR #6703's icon buffer/savebox-specific fixes. |

## Stack summary

| Commit | Area | Open PR relationship | Summary | Primary risk addressed | Validation |
| --- | --- | --- | --- | --- | --- |
| `a620de8bb` | SVG image parser | None identified | Bounds fixed-buffer parsing for SVG path/point data. | Malformed SVG could overrun fixed local buffers. | `svg_parser_regression`; `image` target build. |
| `00d08dbe3` | PLT/TZP metadata | None identified | Validates color-name metadata and effect parameter counts. | Malformed PLT metadata could dereference null data or overflow parameter arrays. | `tzp_color_names_regression`; `image` target build. |
| `92066d5e9` | TLV/TZL reader | Partial overlap with open draft PR #6703 | Validates TLV header offsets, frame counts, table sizes, and suffix lengths before use. | Malformed TLV files could drive unchecked reads/seeks and oversized allocations. | `tzl_header_regression`; `image` target build. |
| `d5f0531c1` | External FX | None identified | Runs external FX commands with `QProcess` argument vectors instead of a shell. | User-controlled paths/options could be interpreted by a shell. | `externfx_process_regression`; `tnzbase` target build. |
| `4e8122e2b` | Crash handler | None identified | Runs `atos`/`addr2line` via `QProcess` instead of `popen`. | Crash-symbolizer command text no longer goes through a shell. | `OpenToonz` target build. |
| `18068c0b5` | Third-party tools | None identified | Handles empty FFmpeg/FFprobe/Rhubarb preference paths before indexing. | Empty preferences could crash path resolution. | `thirdparty_empty_path_regression`; `toonzlib` target build. |
| `8bfbdfee8` | File browser delete | None identified | Deletes simple-level sidecars through `TXshSimpleLevel::removeFiles`. | Deleting a level from the browser could leave palettes, hooks, or `_files` sidecars behind. | `OpenToonz` target build. |
| `17c8d7ac7` | PLT/TZP metadata | None identified | Replaces Semgrep-flagged string-copy helpers with exact-length `memcpy` copies and allocation checks. | Removes remaining unsafe C string-copy findings in the touched PLT parser code. | `tzp_color_names_regression`; focused Semgrep re-scan. |

## Fix details

### SVG parser fixed buffers

SVG files are untrusted project input. The parser previously used fixed local
character buffers while reading XML attributes and path-like fields. The fix
keeps the existing parser shape but bounds local copies and scans so oversized
tokens are rejected or truncated before they can overflow stack storage.

The regression test feeds deliberately oversized SVG data through the parser and
expects controlled failure instead of memory corruption.

### PLT/TZP color metadata validation

The PLT/TZP metadata parser trusted optional `TOONZCOLORNAMES` data and effect
parameter counts. The fix handles missing metadata and caps parsed effect
parameters before copying them into fixed local storage.

The regression test exercises missing metadata and excessive parameter counts.
The follow-up Semgrep hardening commit also removes the remaining `strcpy` and
`strncpy` warnings from the same parser by copying from known source lengths.

### TLV/TZL header table validation

TLV/TZL files are another untrusted file boundary. The reader now validates
frame counts, offset-table positions, table byte sizes, suffix lengths, and
checked read/seek behavior before using decoded values.

The regression test creates malformed TLV headers that previously reached
unchecked table logic and now fail cleanly.

### External FX process execution

The active POSIX external-FX path built a shell command and passed it to
`system()`. The fix preserves the existing external tool contract but invokes
the selected executable through `QProcess` with explicit arguments. Shell
metacharacters in paths and options are treated as data, not syntax.

The regression test uses a payload that would create a marker file when executed
by a shell. The marker is not created after the fix.

### Crash symbolizer process execution

The crash handler previously composed a shell command for `atos` or `addr2line`
and read it via `popen`. The fix uses `QProcess`, explicit argument lists,
merged output, and a bounded wait. This keeps symbolization behavior while
removing shell parsing from a path that includes runtime-derived strings.

This path is hard to unit-test without a crash harness, so validation is by
building the `OpenToonz` target.

### Third-party tool path handling

The FFmpeg/FFprobe/Rhubarb helpers indexed the first character of the configured
tool path without checking for an empty string. The fix resolves blank
preferences through a small shared helper and keeps the previous relative-path
behavior for non-empty values.

The regression test clears the relevant preferences in memory and verifies that
the helper calls return without crashing.

### File browser level deletion

The file browser delete path moved selected files directly to the trash. For
simple levels, that bypassed the established `TXshSimpleLevel::removeFiles`
cleanup path, leaving related palette, hook, or sidecar resources behind.

The fix detects simple-level file types and routes deletion through
`TXshSimpleLevel::removeFiles`; other files keep the existing recycle-bin
behavior. This was build-validated through `OpenToonz`; a non-destructive UI
unit test was not added because this code path intentionally moves user-visible
files to the platform trash.

## Final validation

Commands run successfully on this stack:

```sh
mise run doctor
mise run configure
nix develop path:. --command ctest --test-dir toonz/build/nix-relwithdebinfo -R 'svg_parser_regression|tzp_color_names_regression|tzl_header_regression|externfx_process_regression|thirdparty_empty_path_regression' --output-on-failure
HOME=/private/tmp/opentoonz-semgrep-home semgrep scan --metrics=off --config p/security-audit --config p/c --severity WARNING --severity ERROR --json --output /private/tmp/opentoonz-semgrep-post-changed.json toonz/sources/image/svg/tiio_svg.cpp toonz/sources/image/tzp/avl.c toonz/sources/image/tzp/tiio_plt.cpp toonz/sources/image/tzl/tiio_tzl.cpp toonz/sources/tnzbase/texternfx.cpp toonz/sources/toonz/crashhandler.cpp toonz/sources/toonzlib/thirdparty.cpp toonz/sources/toonz/fileselection.cpp
nix develop path:. --command cmake --build toonz/build/nix-relwithdebinfo --target OpenToonz --parallel
git diff --check
```

Results:

- All five audit regression tests passed.
- The focused post-fix Semgrep scan completed with zero findings on the
  Semgrep-supported target set.
- The `OpenToonz` target linked successfully.
- `git diff --check` was clean.

## Remaining audit items not fixed here

- Scene asset importing should be reviewed through open draft PR #6159 rather
  than duplicated in this stack.
- TZL icon buffer/savebox handling should be reviewed through open draft PR
  #6703. If maintainers prefer this stack's TLV header validation, it can be
  merged independently and then reconciled with the icon-specific draft.
- Broad Semgrep findings in old TWAIN utilities, command-line sample programs,
  and disabled/debug-only code were reviewed in local generated output. They
  were not changed here because the audit did not establish reachable
  exploitability comparable to the fixed parser and process-execution paths.
