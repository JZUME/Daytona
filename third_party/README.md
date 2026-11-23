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
