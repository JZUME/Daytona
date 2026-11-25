# Third-party components

Everything in this directory is a git submodule. Do not copy third-party files
into Daytona's own tree; update the submodule pin instead, as its own change.

## Chicago95

- Source: https://github.com/grassmunk/Chicago95
- Pinned to: tag `v3.0.1` (commit `149a54c`)
- Installed by Daytona:
  - GTK2/3 theme: `Theme/Chicago95` → `share/themes/Chicago95`
  - Icon theme: `Icons/Chicago95` → `share/icons/Chicago95`
  - Cursor theme: `Cursors/Chicago95 Standard Cursors` → `share/icons/Chicago95 Standard Cursors`

### Licence

Chicago95 `v3.0.1` has **no top-level licence file**. Its `README.md`
("Code and license") states `License: GPL-3.0+/MIT`, without saying which parts
are under which licence. No licence files exist inside `Theme/`, `Icons/` or
`Cursors/` (the only licence files in the repository belong to the LightDM
theme and the LibreOffice icon set, which Daytona does not install).

Both GPL-3.0-or-later and MIT are compatible with Daytona's GPL-3.0-or-later
licence, so whichever applies to each installed component, it can be
distributed with Daytona. Packaging must reproduce the README's licence
statement and credits (`CREDITS`) for these files.

## labwc

- Source: https://github.com/labwc/labwc
- Pinned to: tag `0.8.3` (commit `1fe4797`), the version Debian 13 ships, so
  Daytona behaves as it does on Debian's labwc apart from its patches.
- Patched and built by `scripts/build-labwc.sh` (run by the CMake build when
  `DAYTONA_BUILD_LABWC` is on, the default): the submodule is copied into the
  build tree, the patches in `patches/labwc/` are applied there, and labwc is
  built with meson. The submodule itself is never modified.
- Installed as `libexec/daytona/labwc`, which `daytona-session` runs, so it
  does not clash with a system labwc.

### Licence

labwc is GPL-2.0-only, except `src/img/img-xpm.c`, `src/img/xpm-color-table.h`
and `src/img/gen-color-table.pl` (LGPL-2.0-or-later). It is a separate
program from Daytona (GPL-3.0-or-later): Daytona runs it but does not link to
it. Daytona's patches to it are GPL-2.0-only, like the code they change.
