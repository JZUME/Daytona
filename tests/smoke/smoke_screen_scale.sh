#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# A screen's saved font size (Display Properties > Settings) is applied when
# the shell starts: the configuration (DAYTONA_SMOKE_CONFIG) saves 150% for
# HEADLESS-1, which reports no serial number and so is known by its name. It
# also saves a resolution the screen does not offer, which is skipped without
# holding up the font size.
set -euo pipefail

for _ in $(seq 100); do
    if wlr-randr | grep -q '^  Scale: 1\.500000$'; then
        wlr-randr | grep -q '1280x720 px (current)' || {
            echo "the mode changed:" >&2
            wlr-randr >&2
            exit 1
        }
        exit 0
    fi
    sleep 0.1
done
echo "the shell did not apply the saved scale:" >&2
wlr-randr >&2
exit 1
