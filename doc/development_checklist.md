# Development Checklist (Draft)

> Drafted from Shun Iwasawa's development policy posted in [OpenToonz Discussion #6332](https://github.com/opentoonz/opentoonz/discussions/6332).
>
> **Core principle:** A pull request can be merged if it will not interfere with existing usages.

This checklist is intended as a practical aid for contributors, reviewers, and maintainers. It does not replace maintainer judgment or project-specific review.

## 1. Core Merge Requirement

Before merging a pull request:

- [ ] Confirm that the change does **not interfere with existing usages** of OpenToonz.
- [ ] Consider whether a change that benefits one workflow could disadvantage another workflow.
- [ ] Where workflows may conflict, consider making the behavior **optional or user-selectable**, preferably through user profiles or appropriate settings.

## Exceptions and Escalation

A checklist conflict does not automatically reject a change. It identifies a decision that requires explicit approval.

Changes that break existing usage, compatibility, licensing expectations, or other policy may require maintainer or owner approval before acceptance. Approval may take time; identify these conflicts early.

Major changes may require a working demonstration in OT-Dev or a separate fork before approval. Use the demonstration to compare benefits, regressions, migration costs, and affected workflows.

Do not treat an exception as implicit approval. Record the exception, its impact, and the decision that permits it.

## 2. License Review

- [ ] Check whether the PR introduces code, libraries, assets, algorithms, or other material that could conflict with OpenToonz's existing license.
- [ ] If there is a possibility of a license conflict, **do not merge until the owner has reviewed it**.

## 3. Stable Release Restrictions

During the period beginning several months before an announced stable release:

- [ ] Determine whether the PR is a **feature PR**.
- [ ] Do **not merge feature PRs** during the pre-release stabilization period.
- [ ] Continue to evaluate appropriate bug fixes and other non-feature changes according to the core principle.

## 4. Changes Generally Suitable for Merge

The following types of changes are generally acceptable, provided they satisfy the core merge requirement and normal review standards:

- [ ] **Bug fixes**
- [ ] **Refactoring**
- [ ] **Enhancements / speed improvements**
- [ ] **Translations**
- [ ] **New Effects (Fx)**
- [ ] **New commands**
- [ ] **New panels**
- [ ] **New presets**, including Fx presets, camera settings, shortcuts, and similar additions
- [ ] **Changes to default user profiles**, including preferences, panel layouts, and shortcuts

## 5. User Interface Changes

For UI changes that **cannot be controlled through user profiles**, such as adding buttons to panels or commands to context menus:

- [ ] Confirm that the addition will not disturb conventional/existing usage.
- [ ] Check whether the new UI element unnecessarily obstructs, displaces, or complicates an established workflow.
- [ ] Consider whether the behavior or interface can reasonably be made optional when it could affect different workflows differently.

These changes are basically acceptable to merge when they preserve conventional usage.

## 6. Existing Effects and Rendering Behavior

For changes to an existing Fx or rendering behavior:

- [ ] Test scenes/workflows created with the previous OpenToonz version.
- [ ] Ensure the change does **not alter the rendering result of an existing scene** created with the previous version.
- [ ] Treat backward-compatible rendering as a merge requirement unless there is an explicitly reviewed reason to break compatibility.
- [ ] Where new behavior is desirable but would change existing results, consider providing it as an optional/new mode rather than silently changing established behavior.

## 7. Workflow Compatibility Check

Because OpenToonz supports many different animation-production workflows:

- [ ] Consider workflows beyond the one targeted by the PR.
- [ ] Ask whether improving one workflow creates a disadvantage or behavioral change for another.
- [ ] Preserve existing workflows whenever practical.
- [ ] Prefer selectable/optional behavior when multiple valid workflows have conflicting requirements.

## Final Merge Check

Before merging, verify:

- [ ] Existing usages remain functional.
- [ ] Existing workflows are not unnecessarily disrupted.
- [ ] Existing scene rendering remains compatible when existing Fx/rendering behavior is modified.
- [ ] Potentially conflicting workflow changes are optional where appropriate.
- [ ] No unresolved licensing concern requires owner review.
- [ ] The PR is permitted under the current stable-release development phase.

---

## Source

Shun Iwasawa, [OpenToonz Discussion #6332](https://github.com/opentoonz/opentoonz/discussions/6332).

This checklist is a practical restatement of the development policy described in the post. The source policy's central rule is:

> **PR can be merged if it will not interfere existing usages.**

A separate checklist covers project policy for **AI-assisted contributions**.
