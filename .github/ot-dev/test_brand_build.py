import unittest
import base64
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zlib

from brand_build import (ARTWORK_FILENAME, ARTWORK_LINK, ARTWORK_RELATIVE_PATH, SPLASH_ANCHOR,
                         SPLASH_FILENAME, TEMPLATE_NAME, TITLE_ANCHOR,
                         brand_startup, brand_window, embed_artwork, fields_for, render)

try:
    from PyQt5.QtGui import QGuiApplication, QImage, QPainter
    from PyQt5.QtSvg import QSvgRenderer
except ImportError:
    QSvgRenderer = None

STARTUP = SPLASH_ANCHOR + ''';
if (QFileInfo(localSplashPath).exists() && QFileInfo(localSplashPath).isFile()) {
  splashPixmap = QIcon(localSplashPath).pixmap(QSize(610, 344));
  if (splashPixmap.isNull()) {
    splashPixmap = QIcon(":Resources/splash.svg").pixmap(QSize(610, 344));
  }
} else {
  splashPixmap = QIcon(":Resources/splash.svg").pixmap(QSize(610, 344));
}
'''


def solid_png(red, green, blue):
    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff))

    rows = (b"\0" + bytes((red, green, blue)) * 4) * 3
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 3, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


class BrandingTest(unittest.TestCase):
    def test_pr_metadata_is_xml_data_and_substituted_once(self):
        event = {"pull_request": {"number": 260, "title": 'A & B <brush> "test"',
                                  "body": "# Fix\n- Keep [fills](https://example.org) & {{SHORT_SHA}}"}}
        fields = fields_for(event, "pull_request", "abcdef123456", "260/merge")
        template = '<svg><title>{{BUILD_LABEL}}</title><desc>{{DESCRIPTION}}</desc></svg>'
        root = ET.fromstring(render(template, fields))
        self.assertEqual(root.findtext("desc"), "Fix Keep fills & {{SHORT_SHA}}")
        self.assertIn("OT-Dev PR #260", root.findtext("title"))
        self.assertIn("<brush>", fields["PR_HEADING"])

    def test_long_text_fits_the_template_fields(self):
        event = {"pull_request": {"number": 260, "title": "W" * 200, "body": "Wide " * 100}}
        fields = fields_for(event, "pull_request", "abcdef123456", "260/merge")
        for prefix, width in (("PR_HEADING", 37), ("DESCRIPTION", 47)):
            for n in (1, 2):
                self.assertLessEqual(len(fields[f"{prefix}_LINE_{n}"]), width)
        self.assertTrue(fields["DESCRIPTION_LINE_2"].endswith("..."))

    def test_empty_pr_description_and_push_have_useful_fallbacks(self):
        fields = fields_for({"pull_request": {"number": 260, "title": "Fix", "body": None}},
                            "pull_request", "abcdef123456", "260/merge")
        self.assertIn("pull-request build", fields["DESCRIPTION"])
        push = fields_for({}, "push", "abcdef123456", "edition")
        self.assertEqual(push["BUILD_LABEL"], "OT-Dev edition @ abcdef1")
        self.assertIn("edition build", push["PR_HEADING"])

    def test_window_overlay_fails_when_the_source_anchor_changes(self):
        self.assertIn('[OT-Dev PR #260]', brand_window(TITLE_ANCHOR, "OT-Dev PR #260"))
        patched = brand_window(TITLE_ANCHOR * 2, "OT-Dev PR #260")
        self.assertEqual(patched.count("[OT-Dev PR #260]"), 2)
        with self.assertRaises(ValueError):
            brand_window("changed source", "OT-Dev PR #260")

    def test_real_svg_template_renders_without_missing_fields(self):
        template = Path(__file__).with_name(TEMPLATE_NAME).read_text(encoding="utf-8")
        for event, kind, ref in (({}, "push", "edition"),
                                ({"pull_request": {"number": 260, "title": "Restore OT-Dev branding",
                                                   "body": "Show the current PR and its build metadata."}},
                                 "pull_request", "260/merge")):
            svg = render(template, fields_for(event, kind, "abcdef123456", ref))
            self.assertNotIn("{{", svg)
            self.assertEqual(ET.fromstring(svg).tag, "{http://www.w3.org/2000/svg}svg")

    def test_legacy_template_remains_usable(self):
        for name in ("splash-template-legacy.svg", "splash-template-assembly-line.svg"):
            template = Path(__file__).with_name(name).read_text(encoding="utf-8")
            svg = render(template, fields_for({}, "push", "abcdef123456", "main"))
            self.assertNotIn("{{", svg)
            ET.fromstring(svg)
            self.assertEqual(embed_artwork(template, Path("unused.png")), template)
        with self.assertRaises(ValueError):
            brand_startup("changed startup source")

    def test_png_embedding_fails_for_missing_or_invalid_artwork(self):
        with tempfile.TemporaryDirectory() as directory:
            png = Path(directory) / ARTWORK_FILENAME
            with self.assertRaises(FileNotFoundError):
                embed_artwork(ARTWORK_LINK, png)
            png.write_bytes(b"not a PNG")
            with self.assertRaises(ValueError):
                embed_artwork(ARTWORK_LINK, png)

    def test_png_startup_requires_image_or_uses_embedded_fallback(self):
        startup = brand_startup(STARTUP, external_png=True)
        self.assertIn(f'QDir(exeDir).filePath("{SPLASH_FILENAME}")', startup)
        self.assertIn(f'QDir(exeDir).filePath("{ARTWORK_RELATIVE_PATH}")).isFile()', startup)
        self.assertEqual(startup.count("QSize(610, 480)"), 3)
        for changed_source in (SPLASH_ANCHOR,
                               STARTUP.replace(SPLASH_ANCHOR, '"changed path"'),
                               STARTUP.replace("QFileInfo(localSplashPath).isFile()", "changedCheck()"),
                               STARTUP.replace("QSize(610, 344)", "QSize(610, 300)", 1)):
            with self.subTest(source=changed_source), self.assertRaises(ValueError):
                brand_startup(changed_source, external_png=True)

    def test_cli_packages_replaceable_png_with_fixed_svg_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "toonz/sources/toonz"
            (source / "Resources").mkdir(parents=True)
            (source / "mainwindow.cpp").write_text(TITLE_ANCHOR * 2, encoding="utf-8")
            (source / "main.cpp").write_text(STARTUP, encoding="utf-8")
            compiled = source / "Resources/splash.svg"
            compiled.write_text("old compiled splash", encoding="utf-8")
            event = root / "event.json"
            event.write_text(json.dumps({"pull_request": {
                "number": 260, "title": "Current PR & new artwork", "body": "Live metadata"
            }}), encoding="utf-8")
            output = root / "package" / SPLASH_FILENAME
            subprocess.run([sys.executable, str(Path(__file__).with_name("brand_build.py")),
                            "--output", str(output), "--source-root", str(root),
                            "--event-path", str(event), "--event-name", "pull_request",
                            "--sha", "abcdef123456"], check=True, capture_output=True)
            svg = output.read_text(encoding="utf-8")
            artwork = Path(__file__).with_name(ARTWORK_FILENAME).read_bytes()
            packaged_artwork = output.parent / ARTWORK_RELATIVE_PATH
            self.assertEqual(output.name, "splash.svg")
            self.assertEqual(packaged_artwork.relative_to(output.parent).as_posix(),
                             "portablestuff/splash.png")
            self.assertEqual(packaged_artwork.read_bytes(), artwork)
            self.assertFalse(output.with_name(ARTWORK_FILENAME).exists())
            runtime_root = ET.fromstring(svg)
            compiled_root = ET.fromstring(compiled.read_text(encoding="utf-8"))
            image_tag = "{http://www.w3.org/2000/svg}image"
            href = "{http://www.w3.org/1999/xlink}href"
            self.assertEqual(runtime_root.find(image_tag).get(href), "portablestuff/splash.png")
            data = compiled_root.find(image_tag).get(href)
            self.assertTrue(data.startswith("data:image/png;base64,"))
            self.assertEqual(base64.b64decode(data.split(",", 1)[1]), artwork)
            for tag in ("title", "desc"):
                name = "{http://www.w3.org/2000/svg}" + tag
                self.assertEqual(runtime_root.findtext(name), compiled_root.findtext(name))
            self.assertIn(f'filePath("{SPLASH_FILENAME}")',
                          (source / "main.cpp").read_text(encoding="utf-8"))
            self.assertEqual(runtime_root.get("data-otdev-artwork"), "area10")
            self.assertIn("PR #260", svg)
            self.assertIn("abcdef1", svg)
            self.assertNotIn("{{", svg)

            # Artwork replacement never invokes the generator or edits build metadata.
            original_svg = output.read_bytes()
            original_compiled = compiled.read_bytes()
            replacement = solid_png(15, 90, 240)
            packaged_artwork.write_bytes(replacement)
            self.assertEqual((output.parent / runtime_root.find(image_tag).get(href)).read_bytes(),
                             replacement)
            self.assertEqual(output.read_bytes(), original_svg)
            self.assertEqual(compiled.read_bytes(), original_compiled)

    @unittest.skipUnless(QSvgRenderer is not None, "Qt5 SVG bindings are not installed")
    def test_qt5_renders_replaced_png_without_svg_changes(self):
        os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
        app = QGuiApplication.instance() or QGuiApplication([])
        self.assertIsNotNone(app)

        def rendered(path):
            renderer = QSvgRenderer(str(path))
            self.assertTrue(renderer.isValid())
            image = QImage(1440, 1320, QImage.Format_ARGB32_Premultiplied)
            image.fill(0)
            painter = QPainter(image)
            renderer.render(painter)
            painter.end()
            return image

        def pixels(image):
            return image.constBits().asstring(image.byteCount())

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            artwork = root / ARTWORK_RELATIVE_PATH
            artwork.parent.mkdir()
            artwork.write_bytes(solid_png(230, 40, 20))
            template = Path(__file__).with_name(TEMPLATE_NAME).read_text(encoding="utf-8")
            fields = fields_for({"pull_request": {"number": 260, "title": "Replace artwork",
                                                  "body": "Keep PR metadata intact."}},
                                "pull_request", "abcdef123456", "260/merge")
            svg = root / SPLASH_FILENAME
            svg.write_text(render(template, fields), encoding="utf-8")
            fallback = root / "embedded-fallback.svg"
            fallback.write_text(render(embed_artwork(template, artwork), fields), encoding="utf-8")
            original_svg = svg.read_bytes()
            original_fallback = fallback.read_bytes()
            unrelated = root / "unrelated-working-directory"
            unrelated.mkdir()
            previous_directory = Path.cwd()
            try:
                os.chdir(unrelated)
                first = rendered(svg)
                self.assertEqual(first.pixelColor(720, 540).getRgb(), (230, 40, 20, 255))
                self.assertEqual(pixels(first), pixels(rendered(fallback)))
                artwork.write_bytes(solid_png(15, 90, 240))
                replacement = rendered(svg)
                self.assertEqual(replacement.pixelColor(720, 540).getRgb(), (15, 90, 240, 255))
                self.assertEqual(pixels(first.copy(0, 1080, 1440, 240)),
                                 pixels(replacement.copy(0, 1080, 1440, 240)))
                self.assertEqual(svg.read_bytes(), original_svg)
                self.assertEqual(fallback.read_bytes(), original_fallback)
                artwork.unlink()
                self.assertEqual(pixels(first), pixels(rendered(fallback)))
            finally:
                os.chdir(previous_directory)


if __name__ == "__main__":
    unittest.main()
