#!/usr/bin/env python3
"""Import various Flash container formats (FLA, XFL, SWF, SWC, AS).

This helper extracts assets into an output directory and tries to run
an external decompiler for embedded SWF content when available.

It is intentionally conservative: for FLA (binary) files it will attempt
to unzip (many modern FLA files are zip-backed XFL archives). If that fails
it will exit with a helpful message asking the user to export XFL from
Adobe Animate or to use the external decompiler toolchain.

The script writes a simple manifest.json listing the exported files.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile


def run_decompiler_on_swf(swf_path: str, outdir: str, decompiler: str | None = None) -> list:
    """Invoke the existing decompiler wrapper script (decompile_flash.py).

    Returns a list of exported file paths relative to outdir.
    """
    script_dir = os.path.dirname(os.path.abspath(__file__))
    decomp_script = os.path.join(script_dir, "decompile_flash.py")
    if not os.path.exists(decomp_script):
        raise RuntimeError("decompile_flash.py not found alongside import_container.py")

    cmd = [sys.executable, decomp_script, "--input", swf_path, "--output", outdir]
    if decompiler:
        cmd += ["--decompiler", decompiler]

    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if proc.returncode != 0:
        raise RuntimeError(f"Decompiler failed: {proc.stderr.decode('utf-8', errors='ignore')}\n{proc.stdout.decode('utf-8', errors='ignore')}")

    # Read manifest emitted by decompile_flash.py (if present)
    mf = os.path.join(outdir, "manifest.json")
    files = []
    if os.path.exists(mf):
        try:
            with open(mf, "r", encoding="utf-8") as f:
                data = json.load(f)
                for p in data.get("files", []):
                    files.append(p)
        except Exception:
            pass
    return files


def copy_tree(src: str, dst: str, patterns=None) -> list:
    patterns = patterns or ("*",)
    copied = []
    for root, dirs, files in os.walk(src):
        rel_root = os.path.relpath(root, src)
        for fn in files:
            if any(fn.lower().endswith(p.lower().lstrip("*")) for p in patterns):
                srcpath = os.path.join(root, fn)
                tgt_dir = os.path.join(dst, rel_root) if rel_root != "." else dst
                os.makedirs(tgt_dir, exist_ok=True)
                tgt_path = os.path.join(tgt_dir, fn)
                shutil.copy2(srcpath, tgt_path)
                copied.append(os.path.relpath(tgt_path, dst))
    return copied


def handle_swc(path: str, outdir: str, decompiler: str | None) -> list:
    exported = []
    with zipfile.ZipFile(path, "r") as z:
        members = z.namelist()
        # Extract everything to a subdir
        extract_dir = os.path.join(outdir, "swc_extracted")
        os.makedirs(extract_dir, exist_ok=True)
        z.extractall(extract_dir)

    # Copy script files and resources
    exported += copy_tree(extract_dir, outdir, patterns=(".as", ".png", ".jpg", ".jpeg", ".svg", ".xml", ".txt", ".mp3"))

    # Find embedded swf(s)
    for root, _, files in os.walk(extract_dir):
        for fn in files:
            if fn.lower().endswith(".swf"):
                swf = os.path.join(root, fn)
                swf_outdir = os.path.join(outdir, "swf_%s" % os.path.splitext(fn)[0])
                os.makedirs(swf_outdir, exist_ok=True)
                try:
                    files_from_sw = run_decompiler_on_swf(swf, swf_outdir, decompiler)
                    exported += [os.path.join(os.path.basename(swf_outdir), x) for x in files_from_sw]
                except Exception as e:
                    print(f"Warning: decompilation of {swf} failed: {e}", file=sys.stderr)
    return exported


def handle_xfl_dir(path: str, outdir: str) -> list:
    # XFL is a directory-based project; copy a safe set of asset types
    exported = []
    exported += copy_tree(path, outdir, patterns=(".svg", ".xml", ".png", ".jpg", ".jpeg", ".as", ".mp3", ".wav"))
    return exported


def handle_fla(path: str, outdir: str, decompiler: str | None) -> list:
    # Try to unzip the FLA (some FLA files are zip archives containing XFL
    # structure). If unzipping works, treat as XFL. Otherwise give a helpful
    # message asking the user to export XFL or use the external decompiler.
    exported = []
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path, "r") as z:
            extract_dir = os.path.join(outdir, "fla_extracted")
            os.makedirs(extract_dir, exist_ok=True)
            z.extractall(extract_dir)
        exported += handle_xfl_dir(extract_dir, outdir)
    else:
        # As a fallback, try to run the decompiler on the FLA; some tools accept
        # SWF-like extraction from FLA. If that fails, provide a friendly error.
        tmp = os.path.join(outdir, "fla_attempt_decompile")
        os.makedirs(tmp, exist_ok=True)
        try:
            exported += run_decompiler_on_swf(path, tmp, decompiler)
        except Exception:
            raise RuntimeError(
                "Unable to parse binary FLA file. Export XFL from Adobe Animate or provide an XFL folder, or use an external decompiler to extract assets.")
    return exported


def handle_swf(path: str, outdir: str, decompiler: str | None) -> list:
    exported = []
    tmp = os.path.join(outdir, "swf_decomp")
    os.makedirs(tmp, exist_ok=True)
    exported += run_decompiler_on_swf(path, tmp, decompiler)
    exported += copy_tree(tmp, outdir, patterns=(".svg", ".png", ".jpg", ".jpeg", ".xml"))
    return exported


def handle_as(path: str, outdir: str) -> list:
    os.makedirs(outdir, exist_ok=True)
    tgt = os.path.join(outdir, os.path.basename(path))
    shutil.copy2(path, tgt)
    return [os.path.basename(tgt)]


def main():
    parser = argparse.ArgumentParser(description="Import Flash container and extract assets to an output directory")
    parser.add_argument("--input", "-i", required=True, help="Input file or folder (FLA/XFL/SWF/SWC/AS)")
    parser.add_argument("--output", "-o", required=True, help="Output directory (will be created)")
    parser.add_argument("--decompiler", "-d", help="Optional path to an external Flash decompiler (JPEXS/ffdec)")

    args = parser.parse_args()

    inp = args.input
    outdir = os.path.abspath(args.output)
    decompiler = args.decompiler

    if not os.path.exists(inp):
        print(f"Input {inp} not found", file=sys.stderr)
        sys.exit(2)

    os.makedirs(outdir, exist_ok=True)

    files = []
    try:
        if os.path.isdir(inp):
            # Treat as XFL folder
            files += handle_xfl_dir(inp, outdir)
            container_type = "xfl"
        else:
            _, ext = os.path.splitext(inp)
            ext = ext.lower()
            if ext == ".swf":
                files += handle_swf(inp, outdir, decompiler)
                container_type = "swf"
            elif ext == ".swc":
                files += handle_swc(inp, outdir, decompiler)
                container_type = "swc"
            elif ext == ".fla":
                files += handle_fla(inp, outdir, decompiler)
                container_type = "fla"
            elif ext == ".xfl":
                # Occasionally XFL projects are distributed as .xfl zip files
                if zipfile.is_zipfile(inp):
                    with zipfile.ZipFile(inp, "r") as z:
                        extract_dir = os.path.join(outdir, "xfl_extracted")
                        os.makedirs(extract_dir, exist_ok=True)
                        z.extractall(extract_dir)
                    files += handle_xfl_dir(extract_dir, outdir)
                else:
                    files += handle_fla(inp, outdir, decompiler)
                container_type = "xfl"
            elif ext == ".as":
                files += handle_as(inp, outdir)
                container_type = "as"
            else:
                print(f"Unsupported file type: {ext}", file=sys.stderr)
                sys.exit(3)
    except RuntimeError as e:
        print(str(e), file=sys.stderr)
        sys.exit(4)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(5)

    # Write manifest
    manifest = {
        "input": os.path.abspath(inp),
        "output_dir": os.path.abspath(outdir),
        "files": files,
        "type": container_type,
    }
    mf = os.path.join(outdir, "manifest.json")
    with open(mf, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    print(f"Import complete. Exported {len(files)} files.")
    sys.exit(0)


if __name__ == "__main__":
    main()
