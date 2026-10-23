"""Resolve an OT-Dev Windows source without changing branches or source runs."""

import json
import os
from pathlib import Path
import re
from urllib.parse import quote
from urllib.request import Request, urlopen

REPOSITORY = "OpenAnimationLibrary/opentoonz-dev"
WORKFLOWS = {
    ".github/workflows/workflow_windows.yml": "Windows Build",
    ".github/workflows/otdev_branding.yml": "OT-Dev Branded PR Build",
    ".github/workflows/channel_stack_preview.yml": "Channel Stack Preview (Windows)",
}
SHA = r"[0-9a-f]{40}"
BUILD_BRANCH = re.compile(r"otdev-build/pr-([1-9][0-9]*)(?:-.*)?")
APPLICATION = re.compile(
    rf"(?:Opentoonz-Windows-({SHA})|"
    rf"OT-Dev-PR-([1-9][0-9]*)-Windows-({SHA})(?:-([1-9][0-9]*))?|"
    rf"OT-Dev-Branch-[A-Za-z0-9._-]+-Windows-({SHA})-([1-9][0-9]*))")


class SkipBuild(Exception):
    """An expected event that must not produce another or stale package."""


def commit_sha(value):
    if not isinstance(value, str) or not re.fullmatch(SHA, value):
        raise ValueError("Expected a full lowercase commit SHA")
    return value


def api_reader(token):
    def get(path):
        headers = {"Accept": "application/vnd.github+json",
                   "X-GitHub-Api-Version": "2022-11-28"}
        if token:
            headers["Authorization"] = "Bearer " + token
        request = Request(f"https://api.github.com/repos/{REPOSITORY}/{path}",
                          headers=headers)
        with urlopen(request, timeout=30) as response:
            return json.load(response)
    return get


def run_artifacts(get, run_id):
    artifacts = []
    page = 1
    while True:
        result = get(f"actions/runs/{run_id}/artifacts?per_page=100&page={page}")
        batch = result["artifacts"]
        artifacts.extend(batch)
        if len(batch) < 100:
            return artifacts
        page += 1


def artifact_source(artifacts, run_id):
    candidates = []
    for artifact in artifacts:
        name = artifact["name"]
        match = APPLICATION.fullmatch(name)
        if match:
            generic, number, pr_sha, pr_run, branch_sha, branch_run = match.groups()
            package_run = pr_run or branch_run
            if package_run and package_run != str(run_id):
                raise SkipBuild("Application artifact belongs to another run")
            candidates.append((generic or pr_sha or branch_sha, number or "",
                               bool(package_run) and not artifact.get("expired", False)))
        elif (name.startswith("Opentoonz-Windows-") or
              re.match(r"OT-Dev-(?:PR-[^/]+|Branch-[^/]+)-Windows-", name)):
            raise SkipBuild("Unrecognized Windows application artifact name")
    if len({item[0] for item in candidates}) > 1 or len(
            {item[1] for item in candidates if item[1]}) > 1:
        raise SkipBuild("Ambiguous Windows application artifacts")
    if not candidates:
        return "", "", False
    marker = f"OT-Dev-package-manifest-{run_id}"
    complete = any(item[2] for item in candidates) and any(
        item["name"] == marker and not item.get("expired", False)
        for item in artifacts)
    return candidates[0][0], next((item[1] for item in candidates if item[1]), ""), complete


def resolve_pr(get, number, source_sha="", trigger_head="", trigger_base=""):
    if not re.fullmatch(r"[1-9][0-9]*", str(number)):
        raise ValueError("Expected a positive PR number")
    pr = get(f"pulls/{number}")
    if pr["number"] != int(number) or pr["base"]["repo"]["full_name"] != REPOSITORY:
        raise ValueError("PR must target the OT-Dev repository")
    if pr["state"] != "open":
        raise SkipBuild("PR is no longer open")
    head = commit_sha(pr["head"]["sha"])
    base = commit_sha(pr["base"]["sha"])
    merge = pr.get("merge_commit_sha")
    if pr.get("mergeable") is False or not merge:
        raise SkipBuild("PR has no current test merge commit")
    source_sha = commit_sha(source_sha or merge)
    if (source_sha != merge or trigger_head and trigger_head not in (head, source_sha)
            or trigger_base and trigger_base != base):
        raise SkipBuild("PR head or base changed since the source build")
    commit = get(f"commits/{source_sha}")
    if commit["sha"] != source_sha or [p["sha"] for p in commit["parents"]] != [base, head]:
        raise SkipBuild("Source commit does not merge the current PR head and base")
    return source_sha, head, f"refs/pull/{number}/merge", str(number), {"pull_request": pr}


def resolve(event_name, event, get, *, build_run_id, infrastructure_sha,
            ref_name="", pr_number="", source_ref=""):
    """Return output strings plus source_event/manifest; get is an injectable GET."""
    try:
        if event.get("repository", {}).get("full_name", REPOSITORY) != REPOSITORY:
            raise ValueError("Unexpected event repository")
        if not re.fullmatch(r"[1-9][0-9]*", str(build_run_id)):
            raise ValueError("Expected a positive build run ID")
        infrastructure_sha = commit_sha(infrastructure_sha)
        origin = {}
        complete = False
        if event_name == "workflow_run":
            run = event["workflow_run"]
            if (event.get("action") != "completed" or run.get("conclusion") != "success"
                    or run.get("path") not in WORKFLOWS
                    or WORKFLOWS.get(run.get("path")) != run.get("name")):
                raise SkipBuild("Not a successful supported Windows source workflow")
            if run.get("repository", {}).get("full_name", REPOSITORY) != REPOSITORY:
                raise ValueError("Unexpected source run repository")
            run_id = int(run["id"])
            source, number, complete = artifact_source(run_artifacts(get, run_id), run_id)
            origin = {"source_run_id": run_id, "source_run_attempt": run.get("run_attempt", 1)}
            pulls = run.get("pull_requests", [])
            special = run["path"] == ".github/workflows/otdev_branding.yml"
            if special and not number:
                raise SkipBuild("Branded source run has no identifiable PR application")
            if run.get("event") == "pull_request" or number:
                if not source:
                    raise SkipBuild("PR run has no exact application source SHA")
                if not number:
                    if not pulls:
                        commit = get(f"commits/{source}")
                        if len(commit["parents"]) != 2:
                            raise SkipBuild("PR artifact is not a test merge")
                        head = commit_sha(commit["parents"][1]["sha"])
                        pulls = get(f"commits/{head}/pulls?per_page=100")
                        pulls = [p for p in pulls if p.get("state") == "open"
                                 and p["head"]["sha"] == head
                                 and p["base"]["repo"]["full_name"] == REPOSITORY]
                    if len(pulls) != 1:
                        raise SkipBuild("PR association is missing or ambiguous")
                    number = str(pulls[0]["number"])
                if pulls and not any(str(p["number"]) == number for p in pulls):
                    raise SkipBuild("Application artifact identifies a different PR")
                associated = next((p for p in pulls if str(p["number"]) == number), {})
                values = resolve_pr(get, number, source,
                                    "" if special else commit_sha(run["head_sha"]),
                                    associated.get("base", {}).get("sha", ""))
            else:
                head = commit_sha(run["head_sha"])
                if source and source != head:
                    raise SkipBuild("Branch artifact disagrees with the source run commit")
                if get(f"commits/{head}")["sha"] != head:
                    raise ValueError("Source commit could not be verified")
                values = head, head, run["head_branch"], "", {}
        else:
            inputs = event.get("inputs") or {}
            number = str(pr_number or inputs.get("pr_number") or "").strip()
            ref = str(source_ref or inputs.get("ref") or "").strip()
            if event_name in ("create", "push"):
                if event.get("deleted") or event.get("ref_type", "branch") != "branch":
                    raise SkipBuild("Not an active OT-Dev PR build branch")
                if not number and not ref:
                    match = BUILD_BRANCH.fullmatch(ref_name)
                    if not match:
                        raise SkipBuild("Not an OT-Dev PR build branch")
                    number = match[1]
            elif event_name not in ("workflow_dispatch", "workflow_call") and not (number or ref):
                raise SkipBuild("Unsupported build request event")
            if bool(number) == bool(ref):
                raise ValueError("Specify exactly one of pr_number or ref")
            if number:
                values = resolve_pr(get, number)
            else:
                if any(ord(c) < 32 or ord(c) == 127 for c in ref):
                    raise ValueError("Invalid source ref")
                commit = get(f"commits/{quote(ref, safe='')}")
                head = commit_sha(commit["sha"])
                values = head, head, ref, "", {}
        if complete:
            raise SkipBuild("Source run already published a complete OT-Dev Windows package")
        source, head, ref, number, source_event = values
        slug = re.sub(r"[^A-Za-z0-9._-]+", "-", ref).strip("-.")[:60] or "commit"
        identity = f"PR-{number}" if number else f"Branch-{slug}"
        outputs = {"build": "true", "skip_reason": "", "source_sha": source,
                   "head_sha": head, "source_ref": ref, "pr_number": number,
                   "event_name": "pull_request" if number else "push",
                   "artifact_name": f"OT-Dev-{identity}-Windows-{source}-{build_run_id}"}
        manifest = dict(outputs, repository=REPOSITORY, infrastructure_sha=infrastructure_sha,
                        build_run_id=str(build_run_id), **origin)
        return dict(outputs, source_event=source_event, manifest=manifest)
    except SkipBuild as exc:
        return {"build": "false", "skip_reason": str(exc)}


def main():
    if os.environ["GITHUB_REPOSITORY"] != REPOSITORY:
        raise ValueError("This resolver is only for the OT-Dev repository")
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8-sig"))
    result = resolve(os.environ["GITHUB_EVENT_NAME"], event,
                     api_reader(os.getenv("GH_TOKEN") or os.getenv("GITHUB_TOKEN")),
                     build_run_id=os.environ["GITHUB_RUN_ID"],
                     infrastructure_sha=os.getenv("OTDEV_INFRASTRUCTURE_SHA") or os.environ["GITHUB_SHA"],
                     ref_name=os.getenv("GITHUB_REF_NAME", ""),
                     pr_number=os.getenv("OTDEV_PR_NUMBER", ""),
                     source_ref=os.getenv("OTDEV_SOURCE_REF", ""))
    if result["build"] == "true":
        for key, filename, output in (("source_event", "source-event.json", "event_path"),
                                      ("manifest", "source-manifest.json", "manifest_path")):
            path = Path(os.environ["RUNNER_TEMP"]) / filename
            path.write_text(json.dumps(result.pop(key), indent=2) + "\n", encoding="utf-8")
            result[output] = str(path)
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        for key, value in result.items():
            if "\n" in value or "\r" in value:
                raise ValueError("Unexpected multiline workflow output")
            output.write(f"{key}={value}\n")
    print("Resolved Windows source:" if result["build"] == "true" else "Skipped:",
          result.get("source_sha", result["skip_reason"]))


if __name__ == "__main__":
    main()
