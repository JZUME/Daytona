#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks that each .deb is a whole, readable package: the ar archive has
# no damaged or trailing members (dpkg-deb alone misses that, but
# apt-ftparchive doesn't), and the control member and every file in the
# data member decompress. apt only finds out when it unpacks, after the
# package has been published. Needs ar (binutils) and dpkg-deb.
#
# Usage: scripts/apt/check-debs.sh [--drop] DEB...
#   Fails if any package is broken. With --drop, deletes broken packages
#   and warns instead, so a bad file already on a release stays out of the
#   repository index.
set -euo pipefail

drop=false
if [[ "${1:-}" == --drop ]]; then
    drop=true
    shift
fi

broken=0
for deb in "$@"; do
    if ar t "$deb" >/dev/null &&
        dpkg-deb --ctrl-tarfile "$deb" | tar -t >/dev/null &&
        dpkg-deb --fsys-tarfile "$deb" | tar -t >/dev/null; then
        continue
    fi
    if $drop; then
        echo "::warning::Leaving out the broken package ${deb##*/}"
        rm -f "$deb"
    else
        echo "::error::Broken package ${deb##*/}"
        broken=1
    fi
done
exit "$broken"
