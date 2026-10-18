import unittest
from pathlib import Path
import xml.etree.ElementTree as ET

from brand_build import TITLE_ANCHOR, brand_window, fields_for, render


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
        template = Path(__file__).with_name("splash-template.svg").read_text(encoding="utf-8")
        for event, kind, ref in (({}, "push", "edition"),
                                ({"pull_request": {"number": 260, "title": "Restore OT-Dev branding",
                                                   "body": "Show the current PR and its build metadata."}},
                                 "pull_request", "260/merge")):
            svg = render(template, fields_for(event, kind, "abcdef123456", ref))
            self.assertNotIn("{{", svg)
            self.assertEqual(ET.fromstring(svg).tag, "{http://www.w3.org/2000/svg}svg")


if __name__ == "__main__":
    unittest.main()
