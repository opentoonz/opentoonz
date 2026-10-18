# How to contribute

This document describes some points about the contribution process for OpenToonz.

## Contributing to OpenAnimationLibrary/OT-Dev

OT-Dev develops focused changes for official OpenToonz. Accepted changes return
through OT-Master synchronization. OT-Dev Edition is assembled separately.

| Branch | Purpose |
| --- | --- |
| `upstream/master` | Exact mirror of `opentoonz/opentoonz:master`; new upstream-based proposals |
| `main` | Retained base for existing explorations and PRs |
| `edition` | Edition integration, packaging, and workflow guidance |
| `preserve/otdev-main-2026-10-18` | Preserved Edition source before this transition |

### New work

1. Fetch official upstream and OT-Dev. Create a feature branch from official
   `master` or OT-Dev's `upstream/master`. Keep unrelated Edition changes out.
2. Open the OT-Dev proposal with **base: `upstream/master`**. Choose the base
   explicitly; the repository default may differ during the transition.
3. Keep the change focused, retain authors and co-authors, and validate the
   affected behavior. Virtual checks are preferred when they provide useful
   evidence. State any remaining application-testing limit briefly.
4. Submit the reviewed change to `opentoonz/opentoonz:master`. An OT-Dev PR does
   not submit it to official OpenToonz automatically.
5. After upstream acceptance, let the mirror inherit the change. Feature PRs
   are not merged into the mirror or `main` to make them available downstream.

For an existing clone, inspect `git remote -v` first and add only missing
remotes. The example uses `upstream` for official OpenToonz and `oal` for OT-Dev:

```sh
git remote add upstream https://github.com/opentoonz/opentoonz.git
git remote add oal https://github.com/OpenAnimationLibrary/opentoonz-dev.git
git fetch upstream
git fetch oal
git switch -c fix/example oal/upstream/master
```

Before submission, inspect `git diff --stat oal/upstream/master...HEAD` and the
commit list. Submit the same focused feature branch upstream when possible.

### Existing explorations and dependencies

Keep current PR head branches, base branches, and discussion intact. The
[preservation inventory](.github/ot-dev/preservation-2026-10-18.json) records the
83 open PRs observed at the transition. Their full Git histories are retained
as separate branches in the preservation release bundle.

Continue an existing exploration on its established base. When it is ready for
upstream review, extract its focused commits to a new branch from the mirror;
preserve original authors and co-author trailers. Check the resulting diff
before changing any PR base. Retargeting alone can expose unrelated changes.

Prefer independent PRs. A genuinely dependent exploration may remain a draft
with its prerequisite identified, but the prerequisite should land in
OT-Master first. Do not merge it into OT-Dev merely to unblock the next PR.
Existing stacks remain intact while they are evaluated individually.

### Synchronization and Edition assembly

A scheduled task checks the clean mirror each morning in America/Chicago.
Only fast-forward updates from official `master` are allowed. Read the current
heads, verify ancestry, update with an expected-head guard, and verify the
resulting ref. A divergent mirror requires investigation; do not force it.
The task leaves `main`, `edition`, preservation refs, and all PR heads/bases alone.

Edition updates are deliberate integration work. Merge accepted upstream
changes from the mirror when preparing the next Edition build, resolve Edition
conflicts there, and generate the release patch against the exact upstream
commit. Keep experimental dependencies separate until their inclusion is
explicitly chosen. Preserve the original contribution history and build both
clean proposals and the Edition as applicable.

The preservation release includes the current Windows build, source patches,
full histories for the observed open PRs, and SHA-256 checksums. Release
publication remains an explicitly initiated operation.

The remaining instructions describe the official OpenToonz contribution process.

## Pull-requests

The OpenToonz organization loves any kind of contributions, such as fixing typos and code refactoring.
If you fixed or added something useful to OpenToonz, please send pull-requests to us.
We will first review the request, then we will accept it, add comments for rework, or decline it.

### Workflow

0. `fork` OpenToonz to your GitHub account from `opentoonz/opentoonz`.
  - (use the `fork` button at the https://github.com/opentoonz/opentoonz)
0. `clone` the repository.
  - `git clone git@github.com:your-github-account/opentoonz.git`
  - `git remote add upstream https://github.com/opentoonz/opentoonz.git`, additionally.
0. modify the codes.
  - `git checkout -b your-branch-name`
    - `your-branch-name` is a name of your modifications, for example,
      `fix/fatal-bugs`, `feature/new-useful-gui` and so on.
  - fix codes, then test them.
  - `git commit` them with good commit messages.
0. `pull` the latest changes form the `master` branch of the upstream.
  - `git pull upstream master` or `git pull --rebase upstream master`.
  - apply [clang-format](http://clang.llvm.org/docs/ClangFormat.html) with `toonz/sources/.clang-format`.
    - `cd toonz/sources`
    - `./beautification.sh` or `beautification.bat`.
  - `git commit` them.
  - `git push origin your-branch-name`.
0. make a pull request.

## Bugs

If you find bugs, please report details about them using [issues](https://github.com/opentoonz/opentoonz/issues).
Please include information needed to reproduce the bug, including the operating system 
and information directly relating to the issue. Links to screen captures of what is 
observed on screen or video of specific steps to produce the problem are very helpful.  
Then we will try to reproduce the bugs and fix them.
Unfortunately, bugs can sometimes only be reproduced in your own environment, 
so we cannot reproduce them. 
If you believe you can fix the bug, please submit a pull request.

## Features

If you had an idea about a new feature, please implement it and send a pull request to us.
If you cannot implement the feature, please open a topic on the [Google Group Page](https://groups.google.com/forum/#!forum/opentoonz_en).
It enables us to discuss implementations of the feature there.
Feature requests posted on GitHub without an active developer or funding will be closed 
to keep the issue tracker from becoming cluttered with unfulfilled feature requests.

## Translations

Translation sources (`.ts`) are in `toonz/sources/translations`. Contributors can
edit them with Qt Linguist and submit a PR, or attach the updated files to an
issue/discussion for a maintainer to integrate. A full application build is not
required to contribute wording. Share editable sources; compiled `.qm` files are
optional test artifacts unless a maintainer requests them.

See [Contributing and maintaining translations](toonz/sources/translations/README.md)
for the two workflows: sharing translations, and reviewing/merging/updating them.
The guide includes existing Qt Linguist, shell-script, and CMake/Visual Studio
processes, validation, runtime paths, and packaging checks.
