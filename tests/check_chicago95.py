#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks the staged Chicago95 tree (scripts/stage-chicago95.py): no Windows
logo, no icons that replace an application's own, every icon Daytona's
shell asks for still there, and no broken links.

Usage: tests/check_chicago95.py STAGE_DIR
"""

import sys
from pathlib import Path

stage = Path(sys.argv[1])
icons = stage / "icons" / "Chicago95"
theme = stage / "themes" / "Chicago95"
names = {p.stem for p in icons.rglob("*") if p.suffix in (".png", ".svg")}
failures = []

for name in ("start-here", "distributor-logo", "xfdesktop-menu", "xfce4-whiskermenu",
             "gnome-main-menu", "software-update-urgent",
             "application-x-ms-dos-executable", "wine"):
    if name in names:
        failures.append(f"Windows logo icon {name} is installed")
logos = sorted(str(p.relative_to(theme)) for p in theme.rglob("windows*.png"))
if logos:
    failures.append(f"Windows logo theme images are installed: {logos}")

for name in ("firefox", "libreoffice-writer", "gimp", "thunderbird", "google-chrome",
             "gnome-terminal", "xfce4-terminal", "org.xfce.thunar", "pidgin",
             "spotify-client", "virtualbox", "com.visualstudio.code"):
    if name in names:
        failures.append(f"application icon {name} is installed")
branded = sorted(n for n in names if n.startswith(("org.", "com.")))
if branded:
    failures.append(f"application ID icons are installed: {branded[:10]}")

# Icons the shell, Control Panel and dialogs ask for by name.
for name in ("application-x-executable", "applications-other", "folder-documents",
             "folder-open", "input-mouse", "preferences-desktop-display",
             "preferences-system", "preferences-system-windows", "system-run",
             "system-search", "system-shutdown", "text-x-generic",
             "utilities-terminal", "accessories-text-editor", "internet-web-browser",
             "computer", "user-trash", "folder"):
    if name not in names:
        failures.append(f"system icon {name} is missing")

broken = sorted(str(p.relative_to(stage)) for p in stage.rglob("*")
                if p.is_symlink() and not p.exists())
if broken:
    failures.append(f"broken links: {broken[:10]}")

for failure in failures:
    print("FAIL:", failure)
if failures:
    sys.exit(1)
print(f"Chicago95 stage OK: {len(names)} icon names")
