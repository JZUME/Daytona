#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Saves a screenshot of the running development session (scripts/dev-session.sh).
# Usage: scripts/screenshot.sh [output.png]
set -euo pipefail

runtime="${DAYTONA_RUNTIME_DIR:-/tmp/daytona-dev-$(id -u)}"
if [[ ! -f "$runtime/session.env" ]]; then
    echo "No development session found; start scripts/dev-session.sh first" >&2
    exit 1
fi
# shellcheck source=/dev/null
source "$runtime/session.env"

out="${1:-daytona-$(date +%Y%m%d-%H%M%S).png}"
grim "$out"
echo "$out"
