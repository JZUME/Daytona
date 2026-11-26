#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Programs started by D-Bus activation (portal dialogs, D-Bus-activated
# applications) get the Daytona theme variables: the labwc autostart file
# adds them to the activation environment beside labwc's own. Run with
# DAYTONA_SMOKE_ACTIVATION=1 (see run-session.sh) and with activation/ in
# XDG_DATA_DIRS for the bus, so org.daytona.SmokeEnvironment is activatable.
set -euo pipefail

file="$XDG_RUNTIME_DIR/activation.env"
dbus-update-activation-environment SMOKE_ENVIRONMENT_FILE="$file"

# The autostart file runs at labwc's start, alongside the shell; wait until
# the activation environment has the theme.
end=$((SECONDS + 20))
while true; do
    rm -f "$file"
    # The program exits without taking its name, so the activation fails.
    gdbus call --session --dest org.daytona.SmokeEnvironment --object-path / \
        --method org.freedesktop.DBus.Peer.Ping >/dev/null 2>&1 || true
    if [[ -f "$file" ]] && grep -qx 'GTK_THEME=Daytona' "$file"; then
        break
    fi
    if ((SECONDS >= end)); then
        echo "the activation environment did not get the theme:" >&2
        cat "$file" >&2 2>/dev/null || true
        exit 1
    fi
    sleep 0.5
done

for expected in GTK_THEME=Daytona QT_STYLE_OVERRIDE=daytona XDG_SESSION_DESKTOP=Daytona \
    "WAYLAND_DISPLAY=$WAYLAND_DISPLAY"; do
    if ! grep -qxF "$expected" "$file"; then
        echo "activation environment lacks $expected:" >&2
        cat "$file" >&2
        exit 1
    fi
done
