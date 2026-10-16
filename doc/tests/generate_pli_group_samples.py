#!/usr/bin/env python3
"""Generate byte-exact regression fixtures for opentoonz/opentoonz#7182.

Usage: python3 doc/tests/generate_pli_group_samples.py [output-directory]
Run tconverter on each malformed file: expect a clean conversion failure,
no crash, and no invalid cast/out-of-bounds access under ASan/UBSan.
The control must still convert successfully. On Linux use Xvfb/xcb for the
control if offscreen OpenGL is unsupported. Also check existing PLI levels
with nested groups, outline options, palette pages and animated palettes.
These files exercise GROUP child validation, not general PLI fuzz coverage.
Fixtures transcribed from kordood's issue report and valid-control generator.
"""

import sys
from pathlib import Path

SAMPLES = {
    'control_stroke_group_child_quadchain.pli':
        '504c494d0506000000000100280100024e0105560201004a10000000000a640000000a0064000000004e0301170050040100290000',
    'pli_stroke_group_child_autoclose.pli':
        '504c494d0506000000000100280100024e0105560201005a0264004e03011700500401001b0000',
    'pli_palette_child_palettetag.pli':
        '504c494d0506000000000100280100024e030515004506aabbcc1122335602010000',
    'pli_palette_child_autoclose.pli':
        '504c494d0506000000000100280100024e030515005a0264005602010000',
}


def main():
    destination = Path(sys.argv[1] if len(sys.argv) > 1 else "pli-group-samples")
    destination.mkdir(parents=True, exist_ok=True)
    for name, hex_bytes in SAMPLES.items():
        data = bytes.fromhex(hex_bytes)
        path = destination / name
        path.write_bytes(data)
        print(f"{path}: {len(data)} bytes")


if __name__ == "__main__":
    main()
