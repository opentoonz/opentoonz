#!/usr/bin/env bash
set -euo pipefail

tag_name="${QT6_EXPERIMENTAL_TAG:-qt-6-experimental}"

if [[ -n "$(git status --porcelain)" ]]; then
  echo "Refusing to release with uncommitted changes." >&2
  echo "Commit or stash the worktree before moving ${tag_name}." >&2
  exit 1
fi

current_branch="$(git branch --show-current)"
if [[ -z "${current_branch}" ]]; then
  echo "Refusing to release from a detached HEAD." >&2
  exit 1
fi

upstream="$(git rev-parse --abbrev-ref --symbolic-full-name '@{upstream}' 2>/dev/null || true)"
if [[ -n "${upstream}" ]]; then
  default_remote="${upstream%%/*}"
  default_branch="${upstream#*/}"
else
  default_remote="origin"
  default_branch="${current_branch}"
fi

remote="${QT6_EXPERIMENTAL_REMOTE:-${default_remote}}"
remote_branch="${QT6_EXPERIMENTAL_BRANCH:-${default_branch}}"
head_sha="$(git rev-parse HEAD)"
remote_tag_sha="$(git ls-remote "${remote}" "refs/tags/${tag_name}" | awk 'NR == 1 { print $1 }')"

echo "Pushing ${head_sha} to ${remote}/${remote_branch}."
git push "${remote}" "HEAD:${remote_branch}"

echo "Moving local ${tag_name} tag to ${head_sha}."
git tag -f "${tag_name}" "${head_sha}"

if [[ -n "${remote_tag_sha}" ]]; then
  echo "Replacing ${remote}/${tag_name} at ${remote_tag_sha}."
  git push --force-with-lease="refs/tags/${tag_name}:${remote_tag_sha}" \
    "${remote}" "refs/tags/${tag_name}"
else
  echo "Creating ${remote}/${tag_name}."
  git push "${remote}" "refs/tags/${tag_name}"
fi

echo "GitHub Actions will build and publish the rolling ${tag_name} prerelease."
