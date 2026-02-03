Flash vector import helper scripts

This folder contains a small helper script used by Flare to call third-party
Flash decompilers (for example the JPEXS Free Flash Decompiler) to export
vector assets (SVG) and image sequences from `.swf` or `.fla` files.

Usage

- Install JPEXS Free Flash Decompiler: https://github.com/jindrapetrik/jpexs-decompiler
- Ensure the `ffdec` command-line tool is available on your PATH (or pass the
  path to it using `--decompiler` when calling the script).
- From Flare: File → Import → Import Flash (Vector via External Decompiler). The
  import action will run the helper script which will attempt to run JPEXS and
  export SVG/PNG files into a temporary folder. Flare will then open the
  containing folder for you to import the exported files.

Notes

- We intentionally don’t bundle any decompiler to avoid license issues. Please
  install the decompiler yourself if you need vector import.
- The helper script attempts several common invocation styles for JPEXS. If it
  fails for your system, inspect the printed output to see the error and run the
  decompiler manually with the desired flags.
