# How to contribute

This document describes some points about the contribution process for OpenToonz.

## Contributing to OpenAnimationLibrary/OT-Dev

OT-Dev accepts contributions based on official OpenToonz as well as work based
on OT-Dev. Choose the pull request base in GitHub explicitly:

| Your change is based on | Base repository | Base branch |
| --- | --- | --- |
| Official `opentoonz/opentoonz:master` | `OpenAnimationLibrary/opentoonz-dev` | `upstream/master` |
| OT-Dev features or code in `main` | `OpenAnimationLibrary/opentoonz-dev` | `main` |

### Changes based on official OpenToonz

Keep your feature branch based on official `opentoonz/opentoonz:master`.
Open your OT-Dev pull request with **base: `upstream/master`** and
**compare: your feature branch**. You may use your existing fork of official
OpenToonz; you do not need to merge OT-Dev `main` into your branch.

`upstream/master` is a clean mirror of official OpenToonz `master`, not a
branch for accumulating OT-Dev features. Maintainers update it by fast-forward
from official upstream. If it is behind the upstream version your change uses,
ask a maintainer to refresh it before reviewing the comparison.

PRs targeting this branch are reviewed as upstream-based proposals. Maintainers
must not merge feature PRs into the mirror. To include an accepted change in
OT-Dev, a maintainer creates a separate integration branch from `main`,
cherry-picks the reviewed commits, resolves OT-Dev-specific conflicts, and opens
an integration PR targeting `main`. Preserve original authorship and link both
PRs. Close the upstream-based proposal with a link to the integration PR when
the change has been incorporated.

This keeps OT-Dev integration conflicts with OT-Dev maintainers. Contributors
may still need to resolve conflicts with newer official upstream changes.
Build and test both the upstream-based proposal and its OT-Dev integration;
checks that run for one base may differ from checks that run for the other.

A PR here does not submit the change to official OpenToonz. For official
acceptance, open a separate PR against `opentoonz/opentoonz:master`.

#### Contributor steps

1. Fork official [OpenToonz](https://github.com/opentoonz/opentoonz) to your
   GitHub account, then clone your fork. Replace `YOUR-USERNAME` below:

   ```sh
   git clone https://github.com/YOUR-USERNAME/opentoonz.git
   cd opentoonz
   git remote add upstream https://github.com/opentoonz/opentoonz.git
   git remote add oal https://github.com/OpenAnimationLibrary/opentoonz-dev.git
   ```

   If you already have a clone, use it. Run `git remote -v` and add only the
   remotes that are missing. Here `origin` is your writable fork, `upstream`
   is official OpenToonz, and `oal` is OT-Dev.

2. Fetch both bases and check whether the OT-Dev mirror matches official master:

   ```sh
   git fetch upstream
   git fetch oal
   git rev-parse upstream/master
   git rev-parse oal/upstream/master
   ```

   The two commit IDs should match. If they differ, ask an OT-Dev maintainer
   to refresh the mirror before opening the PR. `oal/upstream/master` is the
   local remote-tracking name for OT-Dev's branch named `upstream/master`.

3. For new work, create a feature branch from the mirror with a clean working
   tree:

   ```sh
   git switch -c add-new-commands oal/upstream/master
   ```

   For an existing upstream-based feature branch, switch to that branch instead.
   Keep its existing commits; there is no need to restart or merge OT-Dev
   `main`. Substitute your actual feature branch name throughout these steps.

4. Implement, build, and test the change. Format changed C++ code using
   `toonz/sources/.clang-format`, then commit and push to your fork:

   ```sh
   git add PATH-TO-CHANGED-FILE
   git commit -m "Add new commands"
   git push -u origin add-new-commands
   ```

   Replace the file placeholder with the files you changed. Before submitting,
   inspect the proposed changes against the mirror:

   ```sh
   git log --oneline oal/upstream/master..HEAD
   git diff --stat oal/upstream/master...HEAD
   ```

5. In [OT-Dev pull requests](https://github.com/OpenAnimationLibrary/opentoonz-dev/pulls),
   choose **New pull request**, then **compare across forks**. Set:
   - **base repository:** `OpenAnimationLibrary/opentoonz-dev`
   - **base:** `upstream/master`
   - **head repository:** your fork of OpenToonz
   - **compare:** `add-new-commands` (or your existing feature branch)

   Review the commit list and Files changed before creating the PR. They should
   contain your proposed change, without OT-Dev's unrelated features. Include
   the purpose, testing performed, and any related official OpenToonz PR.

6. Push review fixes to the same feature branch; GitHub updates the PR
   automatically. If newer official changes must be included, fetch upstream
   and merge `upstream/master` into your feature branch, resolve conflicts,
   retest, and push. Ask the maintainer to refresh the OT-Dev mirror as well.
   Coordinate any rebase of an already shared branch with collaborators.

#### Maintainer steps: refresh the mirror

The mirror is updated manually. From a clone with the remotes defined above
and write access to OT-Dev:

```sh
git fetch upstream
git fetch oal
git merge-base --is-ancestor oal/upstream/master upstream/master
git push oal refs/remotes/upstream/master:refs/heads/upstream/master
git fetch oal
git rev-parse upstream/master
git rev-parse oal/upstream/master
```

Proceed with the push only if the ancestry check succeeds (exit status 0).
The push is a normal fast-forward push; do not add `--force`. If the check
fails or the push is rejected, investigate the divergence or concurrent update
before proceeding. The final two commit IDs should match. Do not merge
feature PRs or OT-Dev `main` into this mirror.

#### Maintainer steps: integrate an accepted proposal

1. Fetch OT-Dev and create a separate branch from its current `main`:

   ```sh
   git fetch oal
   git switch -c integrate/add-new-commands oal/main
   ```

2. Open the proposal's **Commits** tab and identify only the feature commits.
   Cherry-pick them in oldest-to-newest order, substituting actual commit IDs:

   ```sh
   git cherry-pick -x FEATURE-COMMIT-SHA
   ```

   Repeat for each feature commit. Do not blindly cherry-pick upstream sync
   commits or merge commits. Cherry-picking preserves the original author;
   retain any `Co-authored-by` trailers. The `-x` option records the source
   commit.

3. Resolve any OT-Dev conflicts, stage the resolved files, and run
   `git cherry-pick --continue`. Use `git cherry-pick --abort` if the
   integration needs to be restarted. Build and test the integrated result.

4. Push the integration branch and open a separate PR with **base: `main`**:

   ```sh
   git push oal integrate/add-new-commands
   ```

   Link the upstream-based proposal in the integration PR and link back from
   the proposal. After the integration PR is merged, close the proposal with
   a link to the merged integration PR; leave `upstream/master` unchanged.

### Changes based on OT-Dev

For changes that depend on OT-Dev features, branch from OT-Dev `main` and
target `main` directly.

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
