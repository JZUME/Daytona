#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# A normal (not maximised) window gets NT's raised 3D frame from Daytona's
# labwc: light lines top and left, dark lines bottom and right.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

daytona-smoke-window --normal &
window=$!
trap 'kill $window 2>/dev/null || true' EXIT

SCREENSHOT="$XDG_RUNTIME_DIR/screenshot.png"
end=$((SECONDS + 15))
until grim "$SCREENSHOT" && daytona-pixelcheck --frame "$SCREENSHOT" "${DAYTONA_SCALE:-1}" \
    2>"$XDG_RUNTIME_DIR/pixelcheck.err"; do
    if ((SECONDS >= end)); then
        cat "$XDG_RUNTIME_DIR/pixelcheck.err" >&2
        exit 1
    fi
    sleep 0.2
done
