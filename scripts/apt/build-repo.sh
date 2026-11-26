#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds and signs Daytona's apt repository in SITE from the packages in
# SITE/pool/stable (releases) and SITE/pool/edge (the latest main builds),
# and adds the public key and an index page. The result is a static site
# (published on GitHub Pages by .github/workflows/apt.yml).
#
# Usage: scripts/apt/build-repo.sh SITE
#
# Signs with the secret key whose fingerprint is DAYTONA_APT_KEY (default:
# the key in packaging/apt). Set GPG_PASSPHRASE_FILE if the key has a
# passphrase. Needs apt-ftparchive (apt-utils) and gpg.
set -euo pipefail

site="$(realpath "$1")"
repo="$(cd "$(dirname "$0")/../.." && pwd)"
key="${DAYTONA_APT_KEY:-E359F3E21E4145F2C163F6AAFA49BF57BC03E090}"
suites=(stable edge)
architectures=(amd64 arm64)

gpg_sign=(gpg --batch --yes --local-user "$key")
if [[ -n "${GPG_PASSPHRASE_FILE:-}" ]]; then
    gpg_sign+=(--pinentry-mode loopback --passphrase-file "$GPG_PASSPHRASE_FILE")
fi

cd "$site"
rm -rf dists
for suite in "${suites[@]}"; do
    mkdir -p "pool/$suite"
    for arch in "${architectures[@]}"; do
        dir="dists/$suite/main/binary-$arch"
        mkdir -p "$dir"
        # Paths in Packages are relative to the site root.
        apt-ftparchive --arch "$arch" packages "pool/$suite" >"$dir/Packages"
        gzip -9nk "$dir/Packages"
    done
    apt-ftparchive \
        -o APT::FTPArchive::Release::Origin=Daytona \
        -o APT::FTPArchive::Release::Label=Daytona \
        -o APT::FTPArchive::Release::Suite="$suite" \
        -o APT::FTPArchive::Release::Codename="$suite" \
        -o APT::FTPArchive::Release::Architectures="${architectures[*]}" \
        -o APT::FTPArchive::Release::Components=main \
        -o APT::FTPArchive::Release::Description="Daytona, a Windows NT style desktop for Debian 13 ($suite)" \
        release "dists/$suite" >"$site/Release.tmp"
    mv "$site/Release.tmp" "dists/$suite/Release"
    "${gpg_sign[@]}" --clearsign -o "dists/$suite/InRelease" "dists/$suite/Release"
    "${gpg_sign[@]}" --armor --detach-sign -o "dists/$suite/Release.gpg" "dists/$suite/Release"
done

cp "$repo/packaging/apt/daytona-archive-keyring.asc" .
gpg --batch --yes --dearmor -o daytona-archive-keyring.gpg daytona-archive-keyring.asc

# The newest version of the daytona package in a suite, or "none yet".
newest() {
    local version
    version="$(sed -n 's/^Version: //p' "dists/$1/main/binary-amd64/Packages" | sort -V | tail -n 1)"
    echo "${version:-none yet}"
}
sed -e "s|@STABLE_VERSION@|$(newest stable)|" -e "s|@EDGE_VERSION@|$(newest edge)|" \
    "$repo/packaging/apt/index.html" >index.html
