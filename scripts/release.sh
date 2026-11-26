#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Starts a release: adds a debian/changelog entry for VERSION. Commit it,
# merge it to main, then tag that commit vVERSION and push the tag. CI builds
# the packages for the tag, and the apt workflow publishes them as a GitHub
# release and in the stable suite of the apt repository. Builds of main
# after that are versioned VERSION+git..., so they stay newer than the
# release.
#
# Usage: scripts/release.sh VERSION ["Change summary"]
set -euo pipefail

version="${1:?usage: scripts/release.sh VERSION [\"Change summary\"]}"
summary="${2:-Release $version.}"
changelog=debian/changelog
cd "$(dirname "$0")/.."

current="$(dpkg-parsechangelog -l "$changelog" -S Version)"
if [[ "$current" == *+git* ]]; then
    echo "debian/changelog has a snapshot entry ($current); remove it first (git checkout debian/changelog)" >&2
    exit 1
fi
if ! dpkg --compare-versions "$version" gt "$current"; then
    echo "$version is not newer than $current" >&2
    exit 1
fi
source_name="$(dpkg-parsechangelog -l "$changelog" -S Source)"
maintainer="$(dpkg-parsechangelog -l "$changelog" -S Maintainer)"
{
    printf '%s (%s) unstable; urgency=medium\n\n' "$source_name" "$version"
    printf '  * %s\n\n' "$summary"
    printf ' -- %s  %s\n\n' "$maintainer" "$(LC_ALL=C date -u -R)"
    cat "$changelog"
} >"$changelog.new"
mv "$changelog.new" "$changelog"
echo "Added $version to $changelog. Next:"
echo "  git commit -am \"Release $version\"   (and merge it to main)"
echo "  git tag v$version <commit on main> && git push origin v$version"
