#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage Chicago95's GTK theme and icon theme for installation.

Copies Theme/Chicago95 and Icons/Chicago95 from the third_party/Chicago95
submodule into OUT_DIR/themes/Chicago95 and OUT_DIR/icons/Chicago95, which
the build installs, leaving out:

- Microsoft's Windows logo. Chicago95 draws the Windows flag on its menu and
  start-here icons, the Windows Update and .exe icons and the theme's start
  buttons. Daytona ships no Microsoft trademarks (AGENT.md).
- Icons that replace an application's own icon. Daytona uses Chicago95 for
  system icons (places, devices, MIME types, actions, status, settings and
  other generic names), but applications keep the icon their developer gave
  them. In apps/ only generic names are kept (KEEP_APPS); any other name
  there is an application's and is removed from every directory, since icon
  lookup ignores directories. Reverse-DNS application IDs and application
  tray icons (BRANDED) go too. A kept name that was a link to a removed
  icon becomes a copy of it.

The submodule itself is never modified.

Usage: scripts/stage-chicago95.py CHICAGO95_DIR OUT_DIR
"""

import hashlib
import os
import re
import shutil
import sys
from pathlib import Path

# Icon names drawn with the Windows logo in at least one size. Every size
# goes, so no size falls back to a flag.
WINDOWS_LOGO = {
    "start-here", "start-here-symbolic",
    "distributor-logo", "distributor-logo-xfce", "distributor-logo-xubuntu",
    "gnome-main-menu", "novell-button",
    "xfce4-panel-menu", "xfce4-whiskermenu", "xfdesktop-menu",
    "desktop-effects", "wine",
    "software-update-available", "software-update-urgent",
    "application-x-ms-dos-executable",
    "gnome-mime-application-x-ms-dos-executable", "application-x-msi",
}

# Generic names kept in apps/: freedesktop and GTK stock names for kinds of
# program and settings, not any one application.
KEEP_APPS = re.compile(
    r"^((accessories|applications|preferences|system|utilities|help|internet"
    r"|multimedia|office|x-office|drive|user|config|gtk|gnome-fs|gnome-dev"
    r"|gnome-stock|gnome-mime)-|stock_)"
    r"|^(accessibility-directory|addressbook|administration"
    r"|application-default-icon|appointment|archive-manager|audio-player"
    r"|background|bluetooth|browser|calendar|clock|date|display-capplet|email"
    r"|file-manager|fonts|force-exit|help|im|image-viewer|key_bindings"
    r"|keyring-manager|locale|mail|menu-editor|multimedia|news-feed"
    r"|package-manager-icon|password|postscript-viewer|scanner|screensaver"
    r"|session-properties|shares|sleep|software|software-manager"
    r"|software-properties|style|taskbar|terminal|text-editor|time|users"
    r"|video-player|volume-knob|wallpaper|web-browser|window"
    r"|workspace-switcher|www-browser|x-pdf)$")

# Application tray and status icons, and reverse-DNS application IDs.
BRANDED = re.compile(
    r"^(org|com|io|net)\."
    r"|^(banshee|blueberry|caffeine|dropboxstatus|empathy|gnome-do|gtg"
    r"|haguichi|hexchat|onboard|pidgin|pithos|rhythmbox|steadyflow|tomboy"
    r"|transmission|ubuntuone|xchat)([-_.]|$)"
    r"|^(stock_xfburn|xfce4-terminal)$")

ICON_SUFFIXES = {".png", ".svg"}


def icon_files(root):
    for dirpath, _, filenames in os.walk(root):
        for filename in filenames:
            path = Path(dirpath, filename)
            if path.suffix in ICON_SUFFIXES:
                yield path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stage_icons(src, icons):
    # Names to drop: the Windows logo, and every non-generic name in apps/.
    drop = set(WINDOWS_LOGO)
    for path in icon_files(src / "apps"):
        if not KEEP_APPS.search(path.stem):
            drop.add(path.stem)

    def dropped(path):
        return path.stem in drop or BRANDED.search(path.stem)

    # Content of every Windows logo, to catch copies under other names.
    logos = {digest(p) for p in icon_files(src)
             if p.stem in WINDOWS_LOGO and not p.is_symlink()}

    shutil.copytree(src, icons, symlinks=True)
    removed = 0

    def chain(path):
        # The links a link passes through, and what it ends at.
        seen = []
        while path.is_symlink() and path not in seen:
            seen.append(path)
            path = path.parent / os.readlink(path)
        return seen[1:] + [path]

    # A kept link through an icon that goes becomes a copy of what it shows.
    for path in list(icon_files(icons)):
        if path.is_symlink() and not dropped(path):
            target = path.resolve()
            if target.exists() and any(dropped(p) for p in chain(path)):
                path.unlink()
                shutil.copy2(target, path)

    for path in list(icon_files(icons)):
        if dropped(path):
            path.unlink()
            removed += 1
    for path in list(icon_files(icons)):
        if not path.is_symlink() and digest(path) in logos:
            path.unlink()
            removed += 1

    # Links left pointing at removed icons, until none are left.
    while True:
        dangling = [p for p in icons.rglob("*") if p.is_symlink() and not p.exists()]
        if not dangling:
            break
        for path in dangling:
            path.unlink()
            removed += 1

    # Non-icon files in apps/ (a helper script and stray extension-less
    # copies), the upstream preview pages and the icon cache, which list
    # icons that are no longer there. The cache is rebuilt on install by the
    # system's icon cache trigger.
    for path in (icons / "apps").rglob("*"):
        if not path.is_dir() and path.suffix not in ICON_SUFFIXES:
            path.unlink()
    for path in icons.rglob("icons.html"):
        path.unlink()
    (icons / "icon-theme.cache").unlink(missing_ok=True)
    return removed


def stage_theme(src, theme):
    shutil.copytree(src, theme, symlinks=True)
    # The start-button images: the Windows logo, alone or with "Start", for
    # panels that are not Daytona's.
    misc = theme / "misc"
    for path in misc.glob("windows*.png"):
        path.unlink()
    shutil.rmtree(misc / "GTK2 start buttons")
    shutil.rmtree(misc / "Legacy GTK3 start buttons")
    (theme / "gnome-shell" / "assets" / "start.png").unlink()


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__.strip().splitlines()[-1])
    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    if not (src / "Icons/Chicago95/index.theme").exists():
        sys.exit("The Chicago95 submodule is missing. Run: git submodule update --init")

    icons = out / "icons" / "Chicago95"
    theme = out / "themes" / "Chicago95"
    for path in (icons, theme):
        shutil.rmtree(path, ignore_errors=True)
    removed = stage_icons(src / "Icons" / "Chicago95", icons)
    stage_theme(src / "Theme" / "Chicago95", theme)
    print(f"Chicago95 staged in {out}: {removed} icon files left out")


if __name__ == "__main__":
    main()
