Flare SWF/FLA Import

Flare adds basic import support for SWF files by leveraging FFmpeg. This imports
SWF animations as raster frames (PNG frames) so they can be used in a Flare
scene.

Prerequisites:
- FFmpeg installed and in PATH (or configured in Flare Preferences).

How it works:
- If FFmpeg supports reading the SWF format on your system, Flare will use
  its FFmpeg-based readers to decode frames and import them as a raster level.
- FLA is a proprietary authoring format; Flare will recognize `.fla` extensions
  but extraction usually requires authoring tools or external conversion tools.

Notes:
- Using JPEXS or other Flash decompilers may allow improved import of vector
  assets. These tools are third-party and subject to their own licenses.
- This import path is intentionally implemented to avoid including GPL-licensed
  decompilers directly in the repository. Instead, users are encouraged to
  install and configure compatible third-party tools and use them to export
  material that Flare can consume.

If you need more advanced Flash vector support (symbol extraction, ActionScript
handling), consider using external decompilers to export SWF contents to SVG
or image sequences and import those into Flare.

Vector import (recommended workflow)

- Install a third-party Flash decompiler (we recommend JPEXS Free Flash
  Decompiler: https://github.com/jindrapetrik/jpexs-decompiler).
- From Flare: File → Import → Import Flash (Vector via External Decompiler)...
  The action will attempt to run a small helper script included in the
  repository (tools/flash/decompile_flash.py) which calls the external
decompiler to export SVG or image sequences into a temporary folder.
- After export completes, Flare opens the containing folder so you can import
  the generated SVG/PNG files using File → Load Level or by dragging them into
  the File Browser.

Notes:
- We do not bundle any third-party decompilers with Flare; you must install
  them separately. This avoids license conflicts and lets you pick a tool that
  suits your needs.
- The helper script attempts several invocation styles for JPEXS. If the
  script can’t find a decompiler on your PATH, specify the decompiler path
  manually (or run the decompiler yourself and import the exported files).

Native container import (FLA / XFL / SWC / AS)

- Flare also provides a "File → Import → Import Flash/XFL/SWC (Native)..."
  action which runs `tools/flash/import_container.py`. This helper attempts to
  extract assets from container formats and will call the decompiler helper for
  embedded SWF files when available.
- Supported behaviors:
  - `.swf`: decompiled using the external decompiler (same as vector import).
  - `.swc`: ZIP archive; the script extracts scripts and assets and decompiles
    any embedded SWF entries.
  - `.fla`: the script tries to unzip the file (many modern FLA files are
    zip-backed XFL archives). If unzipping fails it will attempt to run the
    decompiler on the package; if that also fails you will be asked to export
    XFL from Adobe Animate or use a third-party decompiler.
  - `.xfl`: treated as a project folder or zipped XFL; asset files are copied
    for manual import into Flare.
  - `.as`: ActionScript source files are copied into the export folder and
    recorded in the manifest.

- The importer writes a `manifest.json` into the output folder describing the
  exported files; use this to review and import files into Flare.
- ActionScript files are included as source/metadata and are not executed by
  Flare; consider them as reference code attached to layers or symbols for now.
