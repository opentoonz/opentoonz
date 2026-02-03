#!/usr/bin/env python3
"""Simple wrapper to run a Flash decompiler (JPEXS / ffdec) to export vector
content (SVG) or image sequences from a SWF/FLA file.

This script attempts a small set of likely invocations for JPEXS/ffdec.
It writes a simple manifest.json with the exported files on success.
"""

import argparse
import os
import shutil
import subprocess
import sys
import json


def find_decompiler(explicit=None):
    # If an explicit path was provided, prefer it
    if explicit:
        if os.path.exists(explicit):
            return explicit
        # Maybe user provided just a command name
        found = shutil.which(explicit)
        if found:
            return found
    # Try common CLI names
    for cmd in ("ffdec", "jpexs", "jpexs-decompiler", "ffdec.exe", "jpexs.exe"):
        found = shutil.which(cmd)
        if found:
            return found
    # Nothing found
    return None


def try_run(cmd):
    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
        return proc.stdout.decode('utf-8', errors='ignore'), proc.stderr.decode('utf-8', errors='ignore')
    except subprocess.CalledProcessError as e:
        return None, (e.stdout.decode('utf-8', errors='ignore') + e.stderr.decode('utf-8', errors='ignore'))
    except FileNotFoundError:
        return None, 'executable not found'


def export_with_jpexs(exe, swf, outdir):
    # Try a few possible invocation styles
    candidates = [
        [exe, '-export', 'svg', swf, outdir],
        [exe, '-export', 'svg', '-o', outdir, swf],
        [exe, 'export', 'svg', swf, outdir],
    ]
    for cmd in candidates:
        out, err = try_run(cmd)
        if out is not None:
            return True, out + err
    # As a last resort, try 'java -jar ffdec.jar'
    if exe.lower().endswith('.jar'):
        java_cmd = [sys.executable, '-jar', exe, '-export', 'svg', swf, outdir]
        out, err = try_run(java_cmd)
        if out is not None:
            return True, out + err
    return False, err


def main():
    parser = argparse.ArgumentParser(description='Decompile SWF/FLA using an external decompiler and export SVG/PNG outputs')
    parser.add_argument('--input', '-i', required=True, help='Input SWF/FLA file')
    parser.add_argument('--output', '-o', required=True, help='Output directory (will be created)')
    parser.add_argument('--decompiler', '-d', help='Path to decompiler executable or jar (optional)')

    args = parser.parse_args()
    swf = os.path.abspath(args.input)
    outdir = os.path.abspath(args.output)

    if not os.path.exists(swf):
        print('Input file not found: %s' % swf, file=sys.stderr)
        sys.exit(2)

    os.makedirs(outdir, exist_ok=True)

    decompiler = find_decompiler(args.decompiler)
    if not decompiler:
        print('No Flash decompiler found on PATH. Install JPEXS (https://github.com/jindrapetrik/jpexs-decompiler) and ensure its CLI (ffdec) is on your PATH, or specify the path with --decompiler.', file=sys.stderr)
        sys.exit(3)

    # Run exporter
    success, output = export_with_jpexs(decompiler, swf, outdir)
    if not success:
        print('Decompilation failed:\n' + output, file=sys.stderr)
        sys.exit(4)

    # Build manifest
    files = []
    for root, dirs, filenames in os.walk(outdir):
        for fn in filenames:
            if fn.lower().endswith(('.svg', '.png', '.jpg', '.jpeg')):
                files.append(os.path.relpath(os.path.join(root, fn), outdir))
    manifest = {
        'input': swf,
        'output_dir': outdir,
        'files': files,
    }
    mf = os.path.join(outdir, 'manifest.json')
    with open(mf, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)

    print('Decompilation complete. Exported %d files.' % len(files))
    sys.exit(0)


if __name__ == '__main__':
    main()
