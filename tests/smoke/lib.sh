# SPDX-License-Identifier: GPL-3.0-or-later
# Helpers for smoke tests. Source from a test script run by run-session.sh.

# wait_for_pixels SECONDS X Y #RRGGBB [...]: take screenshots until every pixel
# matches or SECONDS pass. Leaves the last screenshot in $SCREENSHOT.
wait_for_pixels() {
    local seconds="$1"
    shift
    SCREENSHOT="$XDG_RUNTIME_DIR/screenshot.png"
    local end=$((SECONDS + seconds))
    while true; do
        grim "$SCREENSHOT"
        if daytona-pixelcheck "$SCREENSHOT" "$@" 2>"$XDG_RUNTIME_DIR/pixelcheck.err"; then
            return 0
        fi
        if ((SECONDS >= end)); then
            cat "$XDG_RUNTIME_DIR/pixelcheck.err" >&2
            return 1
        fi
        sleep 0.2
    done
}
