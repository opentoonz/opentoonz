# Flash Tools

This directory contains a couple of helper scripts that assist with importing
Flash/Fla/SWF/XFL/SWC/AS assets.

- `decompile_flash.py`: Wrapper for external Flash decompilers (JPEXS / ffdec).
  It takes a `.swf` (or compatible) file and attempts to export vector (SVG)
  and raster assets using the decompiler's command-line interface. It writes a
  `manifest.json` listing exported files.

- `import_container.py`: A conservative helper to extract assets from various
  Flash container formats (`.fla`, `.xfl`, `.swf`, `.swc`, `.as`). Behavior:
  - `.swf`: invokes `decompile_flash.py` (and thereby the external decompiler)
  - `.swc`: treated as a zip; extracts and decompiles any embedded SWF(s)
  - `.fla`: attempts to unzip (many modern FLA files are zip-backed XFL archives);
    if not zipped, it will try running the decompiler; if that also fails, it
    asks that you export XFL from Adobe Animate or use the external decompiler
    directly.
  - `.xfl`: treated as a project folder or zipped XFL; copies asset files
  - `.as`: copied to the output directory

Both scripts are intentionally minimal and avoid bundling any third-party
Flash decompilers due to licensing concerns. To use these scripts you should
install JPEXS (https://github.com/jindrapetrik/jpexs-decompiler) or ensure the
`ffdec`/`jpexs` executable is on your PATH, or set the path with the
`--decompiler` option.

Example usage:

python import_container.py --input path/to/file.swf --output /tmp/flare_flash_import

or

python decompile_flash.py --input path/to/file.swf --output /tmp/flare_flash_decomp --decompiler /path/to/jpexs

Notes:
- The importer emits a `manifest.json` file describing exported files and the
  input container. Use that to inspect what was exported and to import into
  Flare.
- ActionScript (.as) files are copied but not executed or fully analyzed by
  Flare. For now AS content is treated as metadata and source that can be
  attached to frames or symbols by the user.
