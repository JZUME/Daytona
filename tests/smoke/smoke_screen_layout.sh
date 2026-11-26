#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# A saved monitor arrangement (Display Properties > Settings) is restored when
# the shell starts. The session has three 1280x720 outputs, which labwc lays
# out right to left (HEADLESS-3, -2, -1). The configuration
# (DAYTONA_SMOKE_CONFIG) puts HEADLESS-1 left of HEADLESS-2, switches
# HEADLESS-3 off and makes HEADLESS-2 the primary monitor, so the taskbar is
# there. The outputs report no serial number, so are known by their names.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

placed() {
    local info
    info="$(wlr-randr)"
    grep -A8 '^HEADLESS-1 ' <<<"$info" | grep -q 'Position: 0,0' &&
        grep -A8 '^HEADLESS-2 ' <<<"$info" | grep -q 'Position: 1280,0' &&
        grep -A8 '^HEADLESS-3 ' <<<"$info" | grep -q 'Enabled: no'
}

for _ in $(seq 100); do
    placed && break
    sleep 0.1
done
if ! placed; then
    echo "the shell did not restore the arrangement:" >&2
    wlr-randr >&2
    exit 1
fi

# The taskbar's raised top edge on HEADLESS-2 only; HEADLESS-1 shows the
# desktop there.
top=$((720 - 28))
shot="$XDG_RUNTIME_DIR/primary.png"
other="$XDG_RUNTIME_DIR/other.png"
end=$((SECONDS + 15))
while true; do
    grim -o HEADLESS-2 "$shot"
    grim -o HEADLESS-1 "$other"
    if daytona-pixelcheck "$shot" 640 $((top + 1)) '#ffffff' 640 $((top + 14)) '#c0c0c0' \
        2>"$XDG_RUNTIME_DIR/pixelcheck.err" &&
        daytona-pixelcheck "$other" 640 $((top + 1)) '#008080' 640 $((top + 14)) '#008080' \
            2>>"$XDG_RUNTIME_DIR/pixelcheck.err"; then
        break
    fi
    if ((SECONDS >= end)); then
        echo "the taskbar is not on the primary monitor:" >&2
        cat "$XDG_RUNTIME_DIR/pixelcheck.err" >&2
        exit 1
    fi
    sleep 0.2
done

# The shell is still running after the screen was switched off.
pgrep -x daytona-shell >/dev/null
