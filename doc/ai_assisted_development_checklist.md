# AI-Assisted Development Checklist (Draft)

> **OT-Dev permits broad AI-assisted exploration and development.**
>
> Acceptance in OT-Dev does **not** imply readiness for OpenToonz upstream. Upstream contributions must also pass the **Upstream Readiness Gate** below.
>
> Based on Shun Iwasawa's AI-assisted development policy in [OpenToonz Discussion #6937](https://github.com/opentoonz/opentoonz/discussions/6937).

## 1. OT-Dev Development

AI may assist investigation, prototyping, implementation, debugging, testing, documentation, and refactoring.

- [ ] State the purpose.
- [ ] Test or evaluate the change.
- [ ] Take responsibility for the result.
- [ ] Identify known uncertainties or untested assumptions.
- [ ] Do not introduce licensing or provenance conflicts.
- [ ] Preserve OpenToonz licensing, usage, and distribution requirements for work intended upstream.

Large or experimental changes may remain combined during exploration.

## 2. Human Reviewability

- [ ] Be able to explain the change and its design.
- [ ] Avoid unnecessary complexity.
- [ ] Preserve design decisions needed for maintenance.
- [ ] Use AI review to assist, not replace, human judgment.
- [ ] Make upstream candidates small enough for humans to track and understand.

## 3. Scope

- [ ] Keep related exploratory work together when useful.
- [ ] Separate unrelated work.
- [ ] Identify topics requiring separation before upstream submission.
- [ ] Split upstream candidates into one topic per PR.

## 4. Documentation

- [ ] Describe what changed.
- [ ] Explain why.
- [ ] Record important design decisions.
- [ ] Identify AI assistance.

When possible, include:

- [ ] AI model and version.
- [ ] Important prompts or design documents.
- [ ] Attribution for prompts or designs derived from another contributor.

## 5. Licensing and Provenance

- [ ] Do not introduce code or assets with licensing incompatible with OpenToonz.
- [ ] Check provenance when output resembles third-party material.
- [ ] License shared prompts or design documents when required.
- [ ] Preserve required attribution.
- [ ] Resolve licensing concerns before upstream submission.

---

# Upstream Readiness Gate — OpenToonz

**OT-Dev acceptance is not upstream approval.**

## 6. Human-Reviewable Scope

Before proposing AI-assisted work to `opentoonz/opentoonz`:

- [ ] Limit each PR to **one topic of change or improvement**.
- [ ] Split large or multifaceted work into separate PRs.
- [ ] Keep each PR small enough for humans to track and understand.
- [ ] Keep unrelated changes out.

## 7. Required PR Explanation

Each PR must include:

- [ ] **Details of the changes.**
- [ ] **Reason for the changes.**

## 8. AI Information

When possible, include or attach:

- [ ] AI model name and version.
- [ ] Prompt or design document given to the AI.
- [ ] A **CC-BY license** for the prompt/design document if necessary.
- [ ] `Co-authored-by` credit when a prompt is based on another contributor's posted prompt, as recommended by the upstream policy.

## 9. Human Maintainability

- [ ] A human can explain the change.
- [ ] Humans can understand the important implementation choices.
- [ ] Human review does not depend on AI review.
- [ ] Humans can maintain the code after merge.

## 10. General OpenToonz Requirements

The PR must also pass the OpenToonz development checklist:

- [ ] Do not interfere with existing usages.
- [ ] Do not unnecessarily disrupt existing workflows.
- [ ] Preserve existing scene rendering when modifying existing Fx or rendering behavior.
- [ ] Make conflicting workflow behavior optional when appropriate.
- [ ] Resolve licensing concerns or obtain required owner review.
- [ ] Follow the current release-phase restrictions.

## Final Upstream Check

- [ ] One topic per PR.
- [ ] Human-reviewable scope.
- [ ] Changes and reasons documented.
- [ ] AI information included when possible.
- [ ] Licensing and attribution resolved.
- [ ] Human-understandable and maintainable.
- [ ] General OpenToonz requirements satisfied.

---

## Policy Distinction

**OT-Dev:** Explore, test, learn, and develop responsibly with AI.

**OpenToonz upstream:** Make the resulting contribution small, documented, human-reviewable, and human-maintainable.

The upstream policy is a **promotion requirement**, not a restriction on OT-Dev exploration.

## Source

Shun Iwasawa, [OpenToonz Discussion #6937](https://github.com/opentoonz/opentoonz/discussions/6937).

This checklist adapts the upstream policy to OT-Dev while preserving its requirements for upstream contributions.
