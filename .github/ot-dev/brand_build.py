"""OT-Dev CI branding only; never include this overlay in upstream proposals."""

import argparse
from datetime import datetime, timezone
import html
import json
import os
from pathlib import Path
import re
import textwrap
import xml.etree.ElementTree as ET

TOKEN = re.compile(r"\{\{([A-Z_0-9]+)\}\}")
TITLE_ANCHOR = 'QString::fromStdString(TEnv::getApplicationFullName());'


def plain_text(value):
    text = re.sub(r"(?m)^\s*(?:#{1,6}|[-*+])\s+", "", value or "")
    text = re.sub(r"\[([^]]+)\]\([^)]*\)", r"\1", text).replace("`", "")
    return " ".join("".join(c if c.isprintable() else " " for c in text).split())


def two_lines(text, width):
    lines = textwrap.wrap(text, width=width, break_on_hyphens=False)
    if len(lines) > 2:
        lines[1] = lines[1][:width - 3].rstrip() + "..."
    return (lines + ["", ""])[:2]


def fields_for(event, event_name, sha, ref_name, built_at=None):
    if not re.fullmatch(r"[0-9a-fA-F]{7,40}", sha):
        raise ValueError("A real checkout commit SHA is required")
    short_sha = sha[:7]
    pr = event.get("pull_request") if event_name == "pull_request" else None
    if pr:
        number = pr["number"]
        title = plain_text(pr.get("title", ""))
        window_title = re.sub(r"[^A-Za-z0-9 ._#()+-]", " ", title)
        window_title = " ".join(window_title.split())
        window_title = textwrap.shorten(window_title, width=48, placeholder="...")
        label = f"OT-Dev PR #{number}"
        if window_title:
            label += " - " + window_title
        heading = f"PR #{number} — {textwrap.shorten(title, width=58, placeholder='...')}"
        description = plain_text(pr.get("body", ""))
        if not description:
            description = "Experimental OT-Dev pull-request build for review and testing."
    else:
        ref = re.sub(r"[^A-Za-z0-9._/-]", "-", ref_name) or "build"
        ref = textwrap.shorten(ref, width=48, placeholder="...")
        label = f"OT-Dev {ref} @ {short_sha}"
        heading = f"{ref} build @ {short_sha}"
        description = f"Experimental OT-Dev build from the {ref} ref for review and testing."
    heading_lines = two_lines(heading, 37)
    description_lines = two_lines(description, 47)
    built_at = built_at or datetime.now(timezone.utc)
    return {
        "BUILD_LABEL": label,
        "PR_HEADING": heading,
        "PR_HEADING_LINE_1": heading_lines[0],
        "PR_HEADING_LINE_2": heading_lines[1],
        "DESCRIPTION": description,
        "DESCRIPTION_LINE_1": description_lines[0],
        "DESCRIPTION_LINE_2": description_lines[1],
        "BUILD_DATE": built_at.astimezone(timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
        "SHORT_SHA": short_sha,
    }


def render(template, fields):
    # Substitute once so literal {{TOKENS}} in PR text stay literal data.
    svg = TOKEN.sub(lambda match: html.escape(fields[match[1]], quote=True), template)
    ET.fromstring(svg)
    return svg


def brand_window(source, label):
    if TITLE_ANCHOR not in source:
        raise ValueError("Unable to locate the OpenToonz window title")
    # JSON quoting is also a valid C++ string for our ASCII-only build label.
    suffix = json.dumps(" [" + label + "]", ensure_ascii=True)
    return source.replace(TITLE_ANCHOR, TITLE_ANCHOR[:-1] + " + " + suffix + ";")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--template", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--event-path", type=Path, default=os.getenv("GITHUB_EVENT_PATH"))
    parser.add_argument("--event-name", default=os.getenv("GITHUB_EVENT_NAME", "push"))
    parser.add_argument("--sha", default=os.getenv("GITHUB_SHA", ""))
    parser.add_argument("--ref-name", default=os.getenv("GITHUB_REF_NAME", "build"))
    args = parser.parse_args()
    event = json.loads(args.event_path.read_text(encoding="utf-8-sig")) if args.event_path else {}
    fields = fields_for(event, args.event_name, args.sha, args.ref_name)
    svg = render(args.template.read_text(encoding="utf-8"), fields)
    if args.source_root:
        source_path = args.source_root / "toonz/sources/toonz/mainwindow.cpp"
        source = brand_window(source_path.read_text(encoding="utf-8"), fields["BUILD_LABEL"])
        resource_path = args.source_root / "toonz/sources/toonz/Resources/splash.svg"
        if not resource_path.is_file():
            raise ValueError("Unable to locate the compiled OpenToonz splash resource")
        source_path.write_text(source, encoding="utf-8")
        resource_path.write_text(svg, encoding="utf-8")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(svg, encoding="utf-8")
    print("Window title branding:", fields["BUILD_LABEL"])
    print("Generated OT-Dev splash:", args.output)


if __name__ == "__main__":
    main()
