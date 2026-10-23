import copy
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import resolve_build
from resolve_build import REPOSITORY, resolve, run_artifacts

BASE, HEAD, MERGE, INFRA = (character * 40 for character in "abcd")
PR = {"number": 280, "state": "open", "mergeable": True, "merge_commit_sha": MERGE,
      "title": "Current PR title", "body": "Current description",
      "head": {"sha": HEAD, "ref": "feature/example"},
      "base": {"sha": BASE, "repo": {"full_name": REPOSITORY}}}


class ResolverTest(unittest.TestCase):
    def setUp(self):
        self.pr = copy.deepcopy(PR)
        self.artifacts = [{"name": "Opentoonz-Windows-" + MERGE}]
        self.commits = {MERGE: {"sha": MERGE, "parents": [{"sha": BASE}, {"sha": HEAD}]},
                        HEAD: {"sha": HEAD, "parents": [{"sha": BASE}]}}
        self.event = {"action": "completed", "repository": {"full_name": REPOSITORY},
                      "workflow_run": {"id": 99, "run_attempt": 2, "event": "pull_request",
                                       "name": "Windows Build", "conclusion": "success",
                                       "path": ".github/workflows/workflow_windows.yml",
                                       "head_sha": HEAD, "head_branch": "feature/example",
                                       "pull_requests": [{"number": 280,
                                                          "base": {"sha": BASE}}]}}
        self.calls = []

    def get(self, path):
        self.calls.append(path)
        if path == "actions/runs/99/artifacts?per_page=100&page=1":
            return {"artifacts": self.artifacts}
        if path == "pulls/280":
            return self.pr
        if path == f"commits/{HEAD}/pulls?per_page=100":
            return [self.pr]
        if path.startswith("commits/") and path[8:] in self.commits:
            return self.commits[path[8:]]
        self.fail("Unexpected API request: " + path)

    def run_resolver(self, kind="workflow_run", event=None, **kwargs):
        return resolve(kind, self.event if event is None else event, self.get,
                       build_run_id="123", infrastructure_sha=INFRA, **kwargs)

    def test_actual_merge_source_is_distinct_from_head_and_infrastructure(self):
        for run_head in (HEAD, MERGE):
            self.event["workflow_run"]["head_sha"] = run_head
            result = self.run_resolver()
            self.assertEqual(result["source_sha"], MERGE)
            self.assertEqual(result["head_sha"], HEAD)
            self.assertEqual(result["manifest"]["infrastructure_sha"], INFRA)
            self.assertEqual(result["manifest"]["source_run_attempt"], 2)
            self.assertEqual(result["source_event"]["pull_request"]["title"], PR["title"])
            self.assertEqual(result["artifact_name"], f"OT-Dev-PR-280-Windows-{MERGE}-123")

    def test_stale_pr_head_or_base_is_not_relabelled_as_current(self):
        for field in ("head", "base"):
            with self.subTest(field=field):
                self.pr = copy.deepcopy(PR)
                self.pr[field]["sha"] = "e" * 40
                self.assertEqual(self.run_resolver()["build"], "false")

    def test_misleading_merge_name_must_have_exact_current_parents(self):
        self.commits[MERGE]["parents"][1]["sha"] = "e" * 40
        result = self.run_resolver()
        self.assertEqual(result["build"], "false")
        self.assertIn("does not merge", result["skip_reason"])

    def test_old_dedicated_build_uses_artifact_source_not_infrastructure_head(self):
        self.event["workflow_run"].update(
            event="create", path=".github/workflows/otdev_branding.yml",
            name="OT-Dev Branded PR Build", head_sha=INFRA, pull_requests=[])
        self.artifacts = [{"name": f"OT-Dev-PR-280-Windows-{MERGE}"}]
        result = self.run_resolver()
        self.assertEqual(result["source_sha"], MERGE)
        self.assertEqual(result["build"], "true")

    def test_fork_association_can_be_recovered_from_verified_merge_head(self):
        self.event["workflow_run"]["pull_requests"] = []
        result = self.run_resolver()
        self.assertEqual(result["pr_number"], "280")
        self.assertIn(f"commits/{HEAD}/pulls?per_page=100", self.calls)

    def test_artifact_cannot_select_another_pr(self):
        self.artifacts = [{"name": f"OT-Dev-PR-281-Windows-{MERGE}"}]
        self.assertEqual(self.run_resolver()["build"], "false")
        self.assertNotIn("pulls/281", self.calls)

    def test_malformed_and_ambiguous_application_names_are_rejected(self):
        for names in (["Opentoonz-Windows-1234567"],
                      ["Opentoonz-Windows-" + MERGE, "Opentoonz-Windows-" + HEAD],
                      [f"OT-Dev-PR-280-Windows-{MERGE}-98"]):
            with self.subTest(names=names):
                self.artifacts = [{"name": name} for name in names]
                self.assertEqual(self.run_resolver()["build"], "false")

    def test_pr_without_source_artifact_does_not_build_head_instead(self):
        self.artifacts = [{"name": "OT-Dev-recorder-encoder"}]
        self.assertEqual(self.run_resolver()["build"], "false")

    def test_marker_requires_matching_new_package_and_does_not_skip_old_branding(self):
        self.artifacts = [{"name": f"OT-Dev-PR-280-Windows-{MERGE}"},
                          {"name": "OT-Dev-package-manifest-99"}]
        self.assertEqual(self.run_resolver()["build"], "true")
        self.artifacts[0]["name"] += "-99"
        self.assertEqual(self.run_resolver()["build"], "false")
        self.artifacts[0]["expired"] = True
        self.assertEqual(self.run_resolver()["build"], "true")

    def test_wrong_workflow_or_new_worker_cannot_start_a_loop(self):
        for change in ({"name": "OT-Dev Windows Package"},
                       {"path": ".github/workflows/arbitrary.yml"},
                       {"conclusion": "failure"}):
            with self.subTest(change=change):
                event = copy.deepcopy(self.event)
                event["workflow_run"].update(change)
                self.assertEqual(self.run_resolver(event=event)["build"], "false")
        self.assertEqual(self.calls, [])

    def test_channel_stack_keeps_branch_identity_and_exact_run_commit(self):
        self.event["workflow_run"].update(
            event="workflow_dispatch", path=".github/workflows/channel_stack_preview.yml",
            name="Channel Stack Preview (Windows)", head_branch="build/channel-stack-143-145",
            pull_requests=[])
        self.artifacts = [{"name": "OT-Dev-Windows-PR143-PR145"}]
        result = self.run_resolver()
        self.assertEqual(result["source_sha"], HEAD)
        self.assertEqual(result["source_ref"], "build/channel-stack-143-145")
        self.assertEqual(result["pr_number"], "")

    def test_branch_artifact_must_match_run_commit(self):
        self.event["workflow_run"]["event"] = "push"
        self.assertEqual(self.run_resolver()["build"], "false")

    def test_dispatch_inputs_are_explicit_and_mutually_exclusive(self):
        for inputs in ({}, {"pr_number": "280", "ref": "edition"}):
            with self.assertRaises(ValueError):
                self.run_resolver("workflow_dispatch", {"inputs": inputs})
        result = self.run_resolver("workflow_dispatch", {"inputs": {"pr_number": "280"}})
        self.assertEqual(result["source_sha"], MERGE)
        self.pr["state"] = "closed"
        self.assertEqual(self.run_resolver("workflow_dispatch", {
            "inputs": {"pr_number": "280"}})["build"], "false")

    def test_dispatch_ref_is_resolved_without_losing_its_identity(self):
        self.commits["upstream%2Fmaster"] = self.commits[HEAD]
        result = self.run_resolver("workflow_dispatch", {"inputs": {"ref": "upstream/master"}})
        self.assertEqual(result["source_sha"], HEAD)
        self.assertEqual(result["source_ref"], "upstream/master")

    def test_legacy_build_branch_events_and_reusable_inputs(self):
        for kind in ("create", "push"):
            self.assertEqual(self.run_resolver(kind, {}, ref_name="otdev-build/pr-280-check")[
                "pr_number"], "280")
            self.assertEqual(self.run_resolver(kind, {}, ref_name="unrelated")["build"], "false")
        self.assertEqual(self.run_resolver("pull_request", {}, pr_number="280")["source_sha"], MERGE)
        self.assertEqual(self.run_resolver("create", {"ref_type": "tag"},
                                          ref_name="otdev-build/pr-280")["build"], "false")

    def test_artifacts_are_read_beyond_the_first_page(self):
        pages = []
        def get(path):
            pages.append(path)
            return {"artifacts": ([{"name": "unrelated"}] * 100 if path.endswith("&page=1")
                                  else [{"name": "Opentoonz-Windows-" + MERGE}])}
        artifacts = run_artifacts(get, 99)
        self.assertEqual(len(artifacts), 101)
        self.assertEqual(len(pages), 2)

    def test_cli_writes_worker_handoff_files_and_output_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            event_path = root / "event.json"
            event_path.write_text(json.dumps({"inputs": {"pr_number": "280"}}))
            output_path = root / "outputs.txt"
            environment = {"GITHUB_REPOSITORY": REPOSITORY,
                           "GITHUB_EVENT_PATH": str(event_path),
                           "GITHUB_EVENT_NAME": "workflow_dispatch", "GITHUB_RUN_ID": "123",
                           "GITHUB_SHA": HEAD, "OTDEV_INFRASTRUCTURE_SHA": INFRA,
                           "RUNNER_TEMP": directory, "GITHUB_OUTPUT": str(output_path)}
            with patch.dict(os.environ, environment, clear=True), patch.object(
                    resolve_build, "api_reader", return_value=self.get):
                resolve_build.main()
            outputs = dict(line.split("=", 1) for line in output_path.read_text().splitlines())
            self.assertEqual(outputs["build"], "true")
            self.assertEqual(json.loads(Path(outputs["event_path"]).read_text())["pull_request"], PR)
            manifest = json.loads(Path(outputs["manifest_path"]).read_text())
            self.assertEqual(manifest["infrastructure_sha"], INFRA)
            self.assertEqual(manifest["source_sha"], MERGE)


if __name__ == "__main__":
    unittest.main()
