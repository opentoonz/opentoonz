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
