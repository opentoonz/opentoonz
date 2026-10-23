import base64
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from brand_build import ARTWORK_RELATIVE_PATH, TEMPLATE_NAME, fields_for, render
from package_build import ENCODER_FILES, build_package

PNG = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAusB9Y9Zl1sAAAAASUVORK5CYII=")


class PackageTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.package = self.root / "application"
        self.branding = self.root / "branding"
        self.encoder = self.root / "encoder"
        (self.package / "portablestuff").mkdir(parents=True)
        (self.branding / "portablestuff").mkdir(parents=True)
        self.encoder.mkdir()
        self.source = "a" * 40
        self.infrastructure = "b" * 40
        self.build_id = self.source + "-12345-1"
        self.event = {"pull_request": {
            "number": 287, "title": "Branding & recorder", "head": {"sha": "c" * 40},
            "body": "Preserve literal {{SHORT_SHA}} and <artwork> text.",
        }}
        self.fields = fields_for(self.event, "pull_request", self.source, "287/merge",
                                 datetime(2026, 10, 23, 10, 30, tzinfo=timezone.utc))
        template = Path(__file__).with_name(TEMPLATE_NAME).read_text(encoding="utf-8")
        (self.branding / "splash.svg").write_text(render(template, self.fields), encoding="utf-8")
        (self.branding / ARTWORK_RELATIVE_PATH).write_bytes(PNG)
        (self.package / ARTWORK_RELATIVE_PATH).write_bytes(b"older stuff artwork")
        self.executable = self.package / "OpenToonz.exe"
        self.binary = (b"MZ\0OT-Dev Session Recording\0"
                       + self.build_id.encode("utf-16-le") + b"\0"
                       + ("[" + self.fields["BUILD_LABEL"] + "]").encode("ascii"))
        self.executable.write_bytes(self.binary)
        for name in ENCODER_FILES:
            (self.encoder / name).write_bytes(b"fixture for " + name.encode("ascii"))
        self.marker = self.root / "upload" / "otdev-build.json"
        patcher = mock.patch("package_build.subprocess.run")
        self.run = patcher.start()
        self.addCleanup(patcher.stop)
        self.run.return_value = subprocess.CompletedProcess([], 0, "ffmpeg version n8.1.2\n")

    def finish(self, **overrides):
        arguments = dict(package=self.package, branding_dir=self.branding,
                         encoder_dir=self.encoder, source_sha=self.source,
                         infrastructure_sha=self.infrastructure, build_id=self.build_id,
                         event=self.event, event_name="pull_request", ref_name="287/merge",
                         manifest_output=self.marker)
        arguments.update(overrides)
        return build_package(**arguments)

    def assert_unpublished(self):
        self.assertFalse((self.package / "otdev-build.json").exists())
        self.assertFalse(self.marker.exists())

    def test_complete_package_preserves_metadata_and_copies_artwork_after_stuff(self):
        result = self.finish()
        self.assertTrue(result["full_otdev_build"])
        self.assertTrue(result["features"]["session_recorder"])
        self.assertEqual(result["branding"], self.fields)
        self.assertEqual((self.package / ARTWORK_RELATIVE_PATH).read_bytes(), PNG)
        self.assertEqual((self.package / "splash.svg").read_bytes(),
                         (self.branding / "splash.svg").read_bytes())
        for name in (*ENCODER_FILES, "README.md"):
            self.assertTrue((self.package / "otdev-recorder" / name).is_file(), name)
        self.assertEqual(json.loads(self.marker.read_text(encoding="utf-8")), result)
        self.assertEqual(self.marker.read_bytes(), (self.package / "otdev-build.json").read_bytes())
        self.run.assert_called_once()
        self.assertEqual(self.run.call_args.args[0],
                         [str((self.package / "otdev-recorder/ffmpeg.exe").resolve()), "-version"])
        self.assertEqual(self.run.call_args.kwargs["timeout"], 15)
        self.assertTrue(self.run.call_args.kwargs["check"])

    def test_replacing_png_does_not_require_svg_changes(self):
        original = (self.branding / "splash.svg").read_bytes()
        replacement = PNG + b"replacement fixture"
        (self.branding / ARTWORK_RELATIVE_PATH).write_bytes(replacement)
        self.finish()
        self.assertEqual((self.package / ARTWORK_RELATIVE_PATH).read_bytes(), replacement)
        self.assertEqual((self.package / "splash.svg").read_bytes(), original)

    def test_date_shaped_pr_text_does_not_hide_the_real_build_timestamp(self):
        self.event["pull_request"]["body"] = "2026-99-99 99:99 UTC"
        self.fields = fields_for(self.event, "pull_request", self.source, "287/merge",
                                 datetime(2026, 10, 23, 10, 30, tzinfo=timezone.utc))
        template = Path(__file__).with_name(TEMPLATE_NAME).read_text(encoding="utf-8")
        (self.branding / "splash.svg").write_text(render(template, self.fields), encoding="utf-8")
        result = self.finish()
        self.assertEqual(result["branding"]["BUILD_DATE"], "2026-10-23 10:30 UTC")
        self.assertEqual(result["branding"]["DESCRIPTION"], "2026-99-99 99:99 UTC")

    def test_missing_encoder_or_matching_sources_prevents_publication(self):
        for name in ("ffmpeg.exe", "ffmpeg-source.tar.gz", "x264-source.tar.gz", "COPYING.x264"):
            with self.subTest(name=name):
                path = self.encoder / name
                content = path.read_bytes()
                path.unlink()
                with self.assertRaisesRegex(ValueError, "Missing or empty"):
                    self.finish()
                self.assert_unpublished()
                path.write_bytes(content)
        self.run.assert_not_called()

    def test_branding_only_or_wrong_build_executable_prevents_publication(self):
        for marker in (b"OT-Dev Session Recording", self.build_id.encode("utf-16-le"),
                       ("[" + self.fields["BUILD_LABEL"] + "]").encode("ascii")):
            with self.subTest(marker=marker):
                self.executable.write_bytes(self.binary.replace(marker, b"omitted"))
                with self.assertRaisesRegex(ValueError, "compiled OpenToonz executable is missing"):
                    self.finish()
                self.assert_unpublished()
        self.run.assert_not_called()

    def test_wrong_artwork_link_and_unpopulated_field_prevent_publication(self):
        path = self.branding / "splash.svg"
        original = path.read_text(encoding="utf-8")
        for svg in (original.replace(ARTWORK_RELATIVE_PATH, "splash.png"),
                    original.replace(self.fields["BUILD_LABEL"], "{{BUILD_LABEL}}")):
            with self.subTest(svg=svg):
                path.write_text(svg, encoding="utf-8")
                with self.assertRaises(ValueError):
                    self.finish()
                self.assert_unpublished()
        self.run.assert_not_called()

    def test_encoder_failure_does_not_publish_manifest(self):
        self.run.side_effect = subprocess.CalledProcessError(1, ["ffmpeg.exe", "-version"])
        with self.assertRaises(subprocess.CalledProcessError):
            self.finish()
        self.assert_unpublished()

    def test_invalid_png_prevents_publication(self):
        (self.branding / ARTWORK_RELATIVE_PATH).write_bytes(b"not a PNG")
        with self.assertRaisesRegex(ValueError, "artwork is not a PNG"):
            self.finish()
        self.assert_unpublished()
        self.run.assert_not_called()

    def test_source_request_identities_are_checked_before_publication(self):
        path = self.root / "request.json"
        request = {"source_sha": self.source, "infrastructure_sha": "d" * 40, "run_id": 12345}
        path.write_text(json.dumps(request), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "mismatched infrastructure_sha"):
            self.finish(request_manifest=path)
        self.assert_unpublished()
        self.run.assert_not_called()
        request["infrastructure_sha"] = self.infrastructure
        path.write_text(json.dumps(request), encoding="utf-8")
        self.assertEqual(self.finish(request_manifest=path)["source_request"], request)


if __name__ == "__main__":
    unittest.main()
