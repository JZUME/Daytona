#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Puts newly built main packages on the rolling "edge" prerelease and keeps
# only the two newest builds there; older ones are deleted. The edge suite
# of the apt repository is built from what remains, so it never grows. The
# previous build is kept so a client that read the old index just before an
# update can still download it.
#
# Usage: scripts/apt/update-edge-release.sh DIR COMMIT
#   DIR holds the new .deb files; COMMIT is the main commit they were built
#   from. Needs gh with GH_TOKEN and GH_REPO set.
set -euo pipefail

dir="$1"
commit="$2"
tag=edge
keep=2
notes="Latest packages built from main (commit $commit), for testing. Also available as the edge suite of the apt repository: https://jzu.me/daytona/"

if gh release view "$tag" >/dev/null 2>&1; then
    # Move the tag to the commit the packages were built from.
    gh api -X PATCH "repos/$GH_REPO/git/refs/tags/$tag" -f sha="$commit" -F force=true >/dev/null
    gh release edit "$tag" --notes "$notes"
else
    gh release create "$tag" --prerelease --target "$commit" \
        --title "Latest main build (edge)" --notes "$notes"
fi
gh release upload "$tag" "$dir"/*.deb --clobber

# Versions of the daytona package on the release, newest first.
assets="$(gh release view "$tag" --json assets --jq '.assets[].name')"
mapfile -t versions < <(sed -n 's/^daytona_\([^_]*\)_.*\.deb$/\1/p' <<<"$assets" | sort -u -V -r)
for version in "${versions[@]:keep}"; do
    while read -r asset; do
        [[ "$asset" == *"_${version}_"* ]] || continue
        echo "Deleting $asset"
        gh release delete-asset "$tag" "$asset" --yes
    done <<<"$assets"
done
