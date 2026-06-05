#!/usr/bin/env node

import fs from "node:fs/promises";
import path from "node:path";

const REPO_OWNER = "bgyss";
const REPO_NAME = "opentoonz";
const QT6_BRANCH = "codex/qt-6-port";
const PROJECT_URL = "https://github.com/users/bgyss/projects/1";

const project = {
  id: "PVT_kwHOAAmYT84BZxGl",
  owner: "bgyss",
  number: 1,
};

const fields = {
  status: {
    id: "PVTSSF_lAHOAAmYT84BZxGlzhUtgE8",
    options: {
      Todo: "f75ad846",
      "In Progress": "47fc9ee4",
      Done: "98236657",
    },
  },
  lastVerified: { id: "PVTF_lAHOAAmYT84BZxGlzhUtgbE" },
  automationCandidate: {
    id: "PVTSSF_lAHOAAmYT84BZxGlzhUtgbI",
    options: {
      No: "61d2cd0d",
      QTest: "0ed7dea3",
      "Codex GUI": "7640b5a4",
      "Actions script": "42c4a03e",
      "Needs triage": "f44e935a",
    },
  },
  evidenceUrl: { id: "PVTF_lAHOAAmYT84BZxGlzhUtgbM" },
  platform: {
    id: "PVTSSF_lAHOAAmYT84BZxGlzhUtgbQ",
    options: {
      "macOS arm64": "02a00c8f",
      "macOS x64": "49c47657",
      "Windows x64": "9d54ae8d",
      "Linux x64": "a7070ecc",
      "Cross-platform": "1e8c21cb",
    },
  },
  validationStatus: {
    id: "PVTSSF_lAHOAAmYT84BZxGlzhUtgbU",
    options: {
      "Blocked by build": "303815d7",
      "Ready for Codex smoke": "9208215e",
      "Codex evidence ready": "63763e90",
      "Needs human pass": "453e73c6",
      Failed: "49700ee6",
      Accepted: "3ba4c0b2",
      "Studio field test": "996192a1",
    },
  },
  area: {
    id: "PVTSSF_lAHOAAmYT84BZxGlzhUtgbY",
    options: {
      "Build/package": "50ba60d3",
      "Startup/workspace": "3665f94a",
      "Viewer/rendering": "956dc5ae",
      "Drawing/input": "337420a2",
      "Timeline/xsheet": "b12871b2",
      "Palette/styles": "935a250d",
      "File IO": "e7effb6f",
      Preferences: "c2b9c225",
      Scripting: "2d19da62",
      "Tablet/input": "37a2ff08",
      "Cross-platform packaging": "cd0890d0",
      Documentation: "8e9fc494",
    },
  },
  risk: {
    id: "PVTSSF_lAHOAAmYT84BZxGlzhUtgbc",
    options: {
      "P0 blocker": "8901d221",
      "P1 high": "0c3e4744",
      "P2 medium": "2d5c0f31",
      "P3 low": "fe34bddb",
    },
  },
  buildSha: { id: "PVTF_lAHOAAmYT84BZxGlzhUtgbg" },
  baseline: { id: "PVTF_lAHOAAmYT84BZxGlzhUtgb8" },
  validationType: {
    id: "PVTSSF_lAHOAAmYT84BZxGlzhUtgcA",
    options: {
      "CI build": "61cc9caa",
      "Codex GUI smoke": "2d9af19a",
      "Codex report review": "5550c620",
      "Human smoke": "45f760c4",
      "Studio field test": "07e2dd39",
      "Regression follow-up": "df104e15",
    },
  },
};

const token = process.env.GH_PROJECT_TOKEN || process.env.GITHUB_TOKEN;
const dryRun = process.env.QT6_PROJECT_SYNC_DRY_RUN === "1";

if (!token && !dryRun) {
  throw new Error("Set GH_PROJECT_TOKEN or GITHUB_TOKEN before running the Project sync.");
}

const today = new Date().toISOString().slice(0, 10);
const apiHeaders = {
  Authorization: `Bearer ${token}`,
  "Content-Type": "application/json",
  "X-GitHub-Api-Version": "2022-11-28",
};

async function rest(pathname) {
  const response = await fetch(`https://api.github.com${pathname}`, {
    headers: apiHeaders,
  });
  const text = await response.text();
  if (!response.ok) {
    throw new Error(`REST ${pathname} failed: ${response.status} ${text}`);
  }
  return JSON.parse(text);
}

async function graphql(query, variables = {}) {
  const response = await fetch("https://api.github.com/graphql", {
    method: "POST",
    headers: apiHeaders,
    body: JSON.stringify({ query, variables }),
  });
  const text = await response.text();
  if (!response.ok) {
    throw new Error(`GraphQL request failed: ${response.status} ${text}`);
  }

  const payload = JSON.parse(text);
  if (payload.errors?.length) {
    throw new Error(`GraphQL errors: ${JSON.stringify(payload.errors)}`);
  }
  return payload.data;
}

async function latestBranchSha() {
  const branch = await rest(
    `/repos/${REPO_OWNER}/${REPO_NAME}/branches/${encodeURIComponent(QT6_BRANCH)}`,
  );
  return branch.commit.sha;
}

async function workflowRunsForBranch() {
  const runs = await rest(
    `/repos/${REPO_OWNER}/${REPO_NAME}/actions/runs?branch=${encodeURIComponent(QT6_BRANCH)}&per_page=100`,
  );
  return runs.workflow_runs ?? [];
}

async function workflowRunsForFile(workflowFile) {
  const runs = await rest(
    `/repos/${REPO_OWNER}/${REPO_NAME}/actions/workflows/${workflowFile}/runs?per_page=20`,
  );
  return runs.workflow_runs ?? [];
}

function latestRunByName(runs, name) {
  return runs
    .filter((run) => run.name === name)
    .sort((a, b) => Date.parse(b.created_at) - Date.parse(a.created_at))[0];
}

function runConclusion(run) {
  if (!run) return "missing";
  if (run.status !== "completed") return run.status;
  return run.conclusion || "completed";
}

function runIsSuccess(run) {
  return run?.status === "completed" && run?.conclusion === "success";
}

function runIsBlocked(run) {
  return !run || run.status !== "completed" || run.conclusion !== "success";
}

function runLine(name, run) {
  if (!run) return `- ${name}: missing`;
  const state = runConclusion(run);
  const sha = run.head_sha ? run.head_sha.slice(0, 12) : "unknown-sha";
  return `- ${name}: ${state} (${sha}) ${run.html_url}`;
}

function pickEvidenceRun(runs) {
  const present = runs.filter(Boolean);
  return (
    present.find((run) => run.status === "completed" && run.conclusion !== "success") ||
    present.find((run) => run.status !== "completed") ||
    present[0]
  );
}

async function collectCiState(buildSha) {
  const branchRuns = await workflowRunsForBranch();
  const qt6Runs = await workflowRunsForFile("workflow_qt6_experimental_binaries.yml");

  const regular = {
    "Linux Build": latestRunByName(branchRuns, "Linux Build"),
    "MacOS Build": latestRunByName(branchRuns, "MacOS Build"),
    "Windows Build": latestRunByName(branchRuns, "Windows Build"),
  };
  const qt6Experimental = qt6Runs
    .sort((a, b) => Date.parse(b.created_at) - Date.parse(a.created_at))[0];

  const required = Object.values(regular);
  const ciReady = required.length === 3 && required.every(runIsSuccess);
  const evidenceRun = pickEvidenceRun(required);
  const qt6Ready = runIsSuccess(qt6Experimental);

  const regularSummary = Object.entries(regular)
    .map(([name, run]) => runLine(name, run))
    .join("\n");
  const qt6Summary = runLine("Qt 6 Experimental Binary Builds", qt6Experimental);

  return {
    buildSha,
    regular,
    qt6Experimental,
    ciReady,
    qt6Ready,
    evidenceUrl: evidenceRun?.html_url || PROJECT_URL,
    qt6EvidenceUrl: qt6Experimental?.html_url || PROJECT_URL,
    summary: `${regularSummary}\n${qt6Summary}`,
  };
}

async function loadManualGoals() {
  const goalPath = path.join(
    process.cwd(),
    ".github",
    "qt6-validation",
    "manual-goals.json",
  );
  const text = await fs.readFile(goalPath, "utf8");
  return JSON.parse(text);
}

function buildCiItems(ci, baseline) {
  const ciStatus = ci.ciReady ? "Ready for Codex smoke" : "Blocked by build";
  const ciProjectStatus = ci.ciReady ? "Done" : "In Progress";
  const macStatus = ci.qt6Ready ? "Ready for Codex smoke" : "Blocked by build";
  const macProjectStatus = ci.qt6Ready ? "Done" : "In Progress";
  const macEvidenceUrl =
    ci.qt6Experimental?.html_url || ci.regular["MacOS Build"]?.html_url || ci.evidenceUrl;

  return [
    {
      title: "CI gate: latest Qt 6 branch build/package readiness",
      body: [
        `Synced automatically from GitHub Actions for ${QT6_BRANCH}.`,
        "",
        `Current branch SHA: ${ci.buildSha}`,
        "",
        "Latest evidence:",
        ci.summary,
        "",
        ci.ciReady
          ? "Required branch build workflows are green; Codex GUI smoke can proceed once a usable Qt 6 artifact is available."
          : "One or more required branch build workflows are missing, running, or failing. Keep GUI/UX validation blocked until this gate clears.",
      ].join("\n"),
      area: "Build/package",
      platform: "Cross-platform",
      validation_type: "CI build",
      validation_status: ciStatus,
      project_status: ciProjectStatus,
      risk: "P0 blocker",
      automation_candidate: "Actions script",
      evidence_url: ci.evidenceUrl,
      last_verified: today,
      requires_build: false,
    },
    {
      title: "macOS arm64: package and launch Qt 6 build",
      body: [
        "Validate that the Apple Silicon build reaches packaging, bundle verification, and a launchable app before GUI smoke evidence is trusted.",
        "",
        `Current branch SHA: ${ci.buildSha}`,
        "",
        "Latest evidence:",
        runLine("MacOS Build", ci.regular["MacOS Build"]),
        runLine("Qt 6 Experimental Binary Builds", ci.qt6Experimental),
      ].join("\n"),
      area: "Cross-platform packaging",
      platform: "macOS arm64",
      validation_type: "CI build",
      validation_status: macStatus,
      project_status: macProjectStatus,
      risk: "P0 blocker",
      automation_candidate: "Actions script",
      evidence_url: macEvidenceUrl,
      last_verified: today,
      requires_build: false,
    },
  ];
}

function normalizeManualGoal(goal, ci, baseline) {
  const requiresBuild = goal.requires_build !== false;
  const blockedByCi = requiresBuild && !ci.ciReady;
  const evidenceUrl = goal.evidence_url || (blockedByCi ? ci.evidenceUrl : "");

  return {
    title: goal.title,
    body: [
      goal.body || "",
      "",
      `Source: ${goal.source || "manual-goals.json"}`,
      `Current branch SHA: ${ci.buildSha}`,
      blockedByCi
        ? "Current automation gate: blocked by CI/build readiness. This goal remains queued until the build gate clears."
        : "Current automation gate: build-ready for this goal.",
    ]
      .filter(Boolean)
      .join("\n"),
    area: goal.area,
    platform: goal.platform,
    validation_type: goal.validation_type,
    validation_status: blockedByCi ? "Blocked by build" : goal.validation_status,
    project_status: goal.project_status || "Todo",
    risk: goal.risk,
    automation_candidate: goal.automation_candidate || "Needs triage",
    evidence_url: evidenceUrl,
    last_verified: goal.last_verified || "",
    requires_build: requiresBuild,
  };
}

async function loadProjectItems() {
  const items = [];
  let after = null;

  do {
    const data = await graphql(
      `
      query($projectId: ID!, $after: String) {
        node(id: $projectId) {
          ... on ProjectV2 {
            items(first: 100, after: $after) {
              pageInfo {
                hasNextPage
                endCursor
              }
              nodes {
                id
                content {
                  __typename
                  ... on DraftIssue {
                    id
                    title
                    body
                  }
                  ... on Issue {
                    id
                    title
                    url
                  }
                  ... on PullRequest {
                    id
                    title
                    url
                  }
                }
              }
            }
          }
        }
      }
      `,
      { projectId: project.id, after },
    );

    const page = data.node.items;
    items.push(...page.nodes);
    after = page.pageInfo.hasNextPage ? page.pageInfo.endCursor : null;
  } while (after);

  return items;
}

function optionId(field, optionName) {
  const id = field.options[optionName];
  if (!id) {
    throw new Error(`Unknown project option: ${optionName}`);
  }
  return id;
}

async function createDraftItem(goal) {
  if (dryRun) {
    console.log(`[dry-run] create draft item: ${goal.title}`);
    return { id: `dry-run-${goal.title}`, content: { title: goal.title } };
  }

  const data = await graphql(
    `
    mutation($projectId: ID!, $title: String!, $body: String!) {
      addProjectV2DraftIssue(input: {projectId: $projectId, title: $title, body: $body}) {
        projectItem {
          id
          content {
            ... on DraftIssue {
              id
              title
              body
            }
          }
        }
      }
    }
    `,
    { projectId: project.id, title: goal.title, body: goal.body },
  );
  return data.addProjectV2DraftIssue.projectItem;
}

async function updateDraftContent(item, goal) {
  if (item.content?.__typename !== "DraftIssue" || !item.content.id) {
    return;
  }
  if (dryRun) {
    console.log(`[dry-run] update draft content: ${goal.title}`);
    return;
  }

  try {
    await graphql(
      `
      mutation($draftIssueId: ID!, $title: String!, $body: String!) {
        updateProjectV2DraftIssue(input: {draftIssueId: $draftIssueId, title: $title, body: $body}) {
          draftIssue {
            id
          }
        }
      }
      `,
      {
        draftIssueId: item.content.id,
        title: goal.title,
        body: goal.body,
      },
    );
  } catch (error) {
    console.warn(`Could not update draft body for "${goal.title}": ${error.message}`);
  }
}

async function setField(itemId, fieldId, value) {
  if (dryRun) {
    console.log(`[dry-run] set field ${fieldId} on ${itemId}: ${JSON.stringify(value)}`);
    return;
  }

  await graphql(
    `
    mutation($projectId: ID!, $itemId: ID!, $fieldId: ID!, $value: ProjectV2FieldValue!) {
      updateProjectV2ItemFieldValue(
        input: {projectId: $projectId, itemId: $itemId, fieldId: $fieldId, value: $value}
      ) {
        projectV2Item {
          id
        }
      }
    }
    `,
    {
      projectId: project.id,
      itemId,
      fieldId,
      value,
    },
  );
}

async function setGoalFields(item, goal, ci, baseline) {
  await setField(item.id, fields.status.id, {
    singleSelectOptionId: optionId(fields.status, goal.project_status),
  });
  await setField(item.id, fields.validationStatus.id, {
    singleSelectOptionId: optionId(fields.validationStatus, goal.validation_status),
  });
  await setField(item.id, fields.area.id, {
    singleSelectOptionId: optionId(fields.area, goal.area),
  });
  await setField(item.id, fields.platform.id, {
    singleSelectOptionId: optionId(fields.platform, goal.platform),
  });
  await setField(item.id, fields.validationType.id, {
    singleSelectOptionId: optionId(fields.validationType, goal.validation_type),
  });
  await setField(item.id, fields.risk.id, {
    singleSelectOptionId: optionId(fields.risk, goal.risk),
  });
  await setField(item.id, fields.automationCandidate.id, {
    singleSelectOptionId: optionId(fields.automationCandidate, goal.automation_candidate),
  });
  await setField(item.id, fields.buildSha.id, { text: ci.buildSha });
  await setField(item.id, fields.baseline.id, { text: baseline });

  if (goal.evidence_url) {
    await setField(item.id, fields.evidenceUrl.id, { text: goal.evidence_url });
  }
  if (goal.last_verified) {
    await setField(item.id, fields.lastVerified.id, { date: goal.last_verified });
  }
}

async function upsertGoals(goals, ci, baseline) {
  const existingItems = await loadProjectItems();
  const byTitle = new Map(
    existingItems
      .map((item) => [item.content?.title, item])
      .filter(([title]) => typeof title === "string" && title.length > 0),
  );

  const synced = [];
  for (const goal of goals) {
    let item = byTitle.get(goal.title);
    if (!item) {
      item = await createDraftItem(goal);
    } else {
      await updateDraftContent(item, goal);
    }

    await setGoalFields(item, goal, ci, baseline);
    synced.push(goal.title);
    console.log(`synced: ${goal.title}`);
  }

  return synced;
}

async function appendSummary(lines) {
  const summaryPath = process.env.GITHUB_STEP_SUMMARY;
  if (!summaryPath) return;
  await fs.appendFile(summaryPath, `${lines.join("\n")}\n`, "utf8");
}

async function main() {
  const manualGoals = await loadManualGoals();
  const buildSha = await latestBranchSha();
  const ci = await collectCiState(buildSha);
  const baseline = manualGoals.baseline || "OpenToonz 1.7.1 / Qt 5";

  const ciGoals = buildCiItems(ci, baseline);
  const manualProjectGoals = manualGoals.goals.map((goal) =>
    normalizeManualGoal(goal, ci, baseline),
  );
  const goals = [...ciGoals, ...manualProjectGoals];

  const synced = await upsertGoals(goals, ci, baseline);
  const ciState = ci.ciReady ? "ready for Codex smoke" : "blocked by build";

  await appendSummary([
    "## Qt 6 Project Sync",
    "",
    `Project: ${PROJECT_URL}`,
    `Branch: ${QT6_BRANCH}`,
    `Build SHA: ${ci.buildSha}`,
    `CI state: ${ciState}`,
    "",
    "### Latest runs",
    ci.summary,
    "",
    "### Synced items",
    ...synced.map((title) => `- ${title}`),
  ]);
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
