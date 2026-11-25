#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Starts Qt's Widget Gallery (from qt6-base-examples) in the running
# development session (scripts/dev-session.sh), for checking the Daytona style
# against Qt's standard widgets. Other Qt examples can be named instead, as a
# path under Qt's examples directory.
# Usage: scripts/widget-gallery.sh [widgets/dialogs/standarddialogs]
set -euo pipefail

runtime="${DAYTONA_RUNTIME_DIR:-/tmp/daytona-dev-$(id -u)}"
if [[ ! -f "$runtime/session.env" ]]; then
    echo "No development session found; start scripts/dev-session.sh first" >&2
    exit 1
fi
# shellcheck source=/dev/null
source "$runtime/session.env"

example="${1:-widgets/gallery}"
examples="$(qtpaths6 --query QT_INSTALL_EXAMPLES 2>/dev/null || true)"
program="$examples/$example/bin/$(basename "$example")"
if [[ ! -x "$program" ]]; then
    echo "$program not found; install it with: sudo apt install qt6-base-examples" >&2
    exit 1
fi
"$program" >/dev/null 2>&1 &
disown
echo "Started $(basename "$example")"
