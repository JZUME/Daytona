#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Gives the Debian packages a snapshot version for the checked-out commit, so
# each build is newer than the last and apt upgrades an installed copy:
#
#   0.1.0+git20261125222910.412d7ba
#
# The release version from debian/changelog, then the commit's date and time
# (UTC, so builds of later commits sort later) and its short hash. Adds a
# changelog entry for it; run again, it replaces its own entry. Run from the
# top of the repository before dpkg-buildpackage. CI does this for every
# package build.
set -euo pipefail

changelog=debian/changelog
source_name="$(dpkg-parsechangelog -l "$changelog" -S Source)"
version="$(dpkg-parsechangelog -l "$changelog" -S Version)"

# Drop an earlier snapshot entry: everything up to the next entry.
if [[ "$version" == *+git* ]]; then
    sed -i '1,/^ -- /d' "$changelog"
    sed -i '1{/^$/d}' "$changelog"
    version="$(dpkg-parsechangelog -l "$changelog" -S Version)"
fi

stamp="$(TZ=UTC git log -1 --date=format-local:%Y%m%d%H%M%S --format=%cd)"
hash="$(git rev-parse --short=7 HEAD)"
snapshot="$version+git$stamp.$hash"
maintainer="$(dpkg-parsechangelog -l "$changelog" -S Maintainer)"
date="$(LC_ALL=C TZ=UTC git log -1 --date=format-local:'%a, %d %b %Y %H:%M:%S +0000' --format=%cd)"

{
    printf '%s (%s) unstable; urgency=medium\n\n' "$source_name" "$snapshot"
    printf '  * Snapshot build of commit %s.\n\n' "$(git rev-parse HEAD)"
    printf ' -- %s  %s\n\n' "$maintainer" "$date"
    cat "$changelog"
} >"$changelog.new"
mv "$changelog.new" "$changelog"
echo "$snapshot"
