"""Finish and verify a complete OT-Dev Windows package before publication."""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET

from brand_build import ARTWORK_RELATIVE_PATH, TEMPLATE_NAME, fields_for, render

ENCODER_FILES = (
    "ffmpeg.exe", "ffmpeg-source.tar.gz", "x264-source.tar.gz",
    "COPYING.GPLv2", "COPYING.LGPLv2.1", "COPYING.x264", "LICENSE.md",
    "build-configuration.txt", "build-ffmpeg.sh",
)
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
SVG_NS = "{http://www.w3.org/2000/svg}"
XLINK_HREF = "{http://www.w3.org/1999/xlink}href"


def require_file(path):
    if not path.is_file() or path.stat().st_size == 0:
        raise ValueError(f"Missing or empty package input: {path}")
    return path


def validate_branding(directory, fields):
    svg = require_file(directory / "splash.svg").read_text(encoding="utf-8")
    artwork = require_file(directory / ARTWORK_RELATIVE_PATH).read_bytes()
    if len(artwork) <= len(PNG_SIGNATURE) or not artwork.startswith(PNG_SIGNATURE):
        raise ValueError("The splash artwork is not a PNG")
    root = ET.fromstring(svg)
    images = list(root.iter(SVG_NS + "image"))
    if (len(images) != 1
            or images[0].get(XLINK_HREF, images[0].get("href")) != ARTWORK_RELATIVE_PATH):
        raise ValueError(f"The splash must reference exactly {ARTWORK_RELATIVE_PATH}")

    # Re-render with the original build time. This also distinguishes literal
    # {{TOKENS}} in PR text from template fields that were never populated.
    template = Path(__file__).with_name(TEMPLATE_NAME).read_text(encoding="utf-8")
    for element in root.iter(SVG_NS + "text"):
        value = element.text or ""
        if not re.fullmatch(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2} UTC", value):
            continue
        try:
            datetime.strptime(value, "%Y-%m-%d %H:%M UTC")
        except ValueError:
            continue
        expected = dict(fields, BUILD_DATE=value)
        if ET.tostring(root) == ET.tostring(ET.fromstring(render(template, expected))):
            return expected
    raise ValueError("The splash contains unpopulated fields or mismatched PR/build metadata")


def build_package(package, branding_dir, encoder_dir, source_sha, infrastructure_sha,
                  build_id, event=None, event_name="push", ref_name="build",
                  request_manifest=None, manifest_output=None):
    for label, sha in (("source", source_sha), ("infrastructure", infrastructure_sha)):
        if not re.fullmatch(r"[0-9a-fA-F]{40}", sha):
            raise ValueError(f"A complete {label} commit SHA is required")
    if not re.fullmatch(r"[A-Za-z0-9._-]+", build_id):
        raise ValueError("Invalid CI build identity")
    source_sha, infrastructure_sha = source_sha.lower(), infrastructure_sha.lower()
    event = event or {}
    fields = fields_for(event, event_name, source_sha, ref_name)
    fields = validate_branding(branding_dir, fields)
    if not (package / "portablestuff").is_dir():
        raise ValueError("Copy the application and portablestuff before finishing the package")
    executables = [path for path in package.iterdir()
                   if path.name.lower() == "opentoonz.exe" and path.is_file()]
    if len(executables) != 1:
        raise ValueError("The package must contain one OpenToonz.exe")
    executable = require_file(executables[0])
    binary = executable.read_bytes()
    if not binary.startswith(b"MZ"):
        raise ValueError("The packaged OpenToonz executable is not a Windows binary")
    for marker in ("OT-Dev Session Recording", build_id, "[" + fields["BUILD_LABEL"] + "]"):
        if marker.encode("ascii") not in binary and marker.encode("utf-16-le") not in binary:
            raise ValueError(f"The compiled OpenToonz executable is missing: {marker}")
    for name in ENCODER_FILES:
        require_file(encoder_dir / name)
    recorder_readme = require_file(Path(__file__).parent / "recorder/README.md")
    request = None
    if request_manifest:
        request = json.loads(request_manifest.read_text(encoding="utf-8-sig"))
        if not isinstance(request, dict):
            raise ValueError("The source request manifest must be an object")
        for key, expected in (("source_sha", source_sha), ("infrastructure_sha", infrastructure_sha)):
            if str(request.get(key, "")).lower() != expected:
                raise ValueError(f"The source request manifest has a mismatched {key}")

    # Run after copying stuff so its files cannot overwrite the selected artwork.
    shutil.copyfile(branding_dir / "splash.svg", package / "splash.svg")
    shutil.copyfile(branding_dir / ARTWORK_RELATIVE_PATH, package / ARTWORK_RELATIVE_PATH)
    destination = package / "otdev-recorder"
    shutil.copytree(encoder_dir, destination, dirs_exist_ok=True)
    shutil.copyfile(recorder_readme, destination / "README.md")
    result = subprocess.run([str((destination / "ffmpeg.exe").resolve()), "-version"],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, check=True, timeout=15)
    encoder_version = (result.stdout or "").splitlines()
    if not encoder_version or not encoder_version[0].startswith("ffmpeg version "):
        raise ValueError("The packaged recorder encoder did not identify itself as FFmpeg")

    pr = event.get("pull_request", {}) if event_name == "pull_request" else {}
    manifest = {
        "schema": 1,
        "full_otdev_build": True,
        "source_sha": source_sha,
        "infrastructure_sha": infrastructure_sha,
        "build_id": build_id,
        "event_name": event_name,
        "ref_name": ref_name,
        "pr_number": pr.get("number"),
        "pr_head_sha": pr.get("head", {}).get("sha"),
        "branding": fields,
        "features": {"splash": True, "title_bar": True, "session_recorder": True},
        "paths": {
            "executable": executable.name,
            "splash_svg": "splash.svg",
            "splash_png": ARTWORK_RELATIVE_PATH,
            "recorder_encoder": "otdev-recorder/ffmpeg.exe",
            "recordings": "portablestuff/recordings",
        },
        "encoder_version": encoder_version[0],
        "packaged_at": datetime.now(timezone.utc).isoformat(),
    }
    if request is not None:
        manifest["source_request"] = request
    serialized = json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"
    (package / "otdev-build.json").write_text(serialized, encoding="utf-8")
    if manifest_output:
        manifest_output.parent.mkdir(parents=True, exist_ok=True)
        manifest_output.write_text(serialized, encoding="utf-8")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--branding-dir", type=Path, required=True)
    parser.add_argument("--encoder-dir", type=Path, required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--infrastructure-sha", required=True)
    parser.add_argument("--build-id", required=True)
    parser.add_argument("--event-path", type=Path, default=os.getenv("GITHUB_EVENT_PATH"))
    parser.add_argument("--event-name", default=os.getenv("GITHUB_EVENT_NAME", "push"))
    parser.add_argument("--ref-name", default=os.getenv("GITHUB_REF_NAME", "build"))
    parser.add_argument("--request-manifest", type=Path)
    parser.add_argument("--manifest-output", type=Path)
    args = vars(parser.parse_args())
    event_path = args.pop("event_path")
    args["event"] = json.loads(event_path.read_text(encoding="utf-8-sig")) if event_path else {}
    manifest = build_package(**args)
    print("Verified complete OT-Dev package:", manifest["branding"]["BUILD_LABEL"])


if __name__ == "__main__":
    main()
