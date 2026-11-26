# Daytona Desktop Environment

<img width="2880" height="1800" alt="Daytona Test VM-2026-11-26-13-07-47" src="https://github.com/user-attachments/assets/e9d4e730-6e39-48c2-ac4d-58628074cf7c" />

Inspired by Windows NT, [Chicago95](https://github.com/grassmunk/Chicago95) and built with Qt6 Widgets, a functional Qt desktop environment with the familiarity of the NT and 9X era.

> [!WARNING]
> **Early development.** Daytona currently targets **Debian 13** and may contain incomplete features, bugs or breaking changes. See [AGENT.md](AGENT.md) for the project's design principles and development rules.

> [!IMPORTANT]
> **THIS PROJECT IS VIBE-CODED.** Development is primarily performed by AI agents working from GitHub issues and defined project rules. Code may not have undergone comprehensive human review. Users should exercise appropriate caution, particularly when installing on their primary systems.

## Installing on Debian 13

Daytona has an apt repository for Debian 13 (amd64 and arm64) at
<https://soupbowl.io/Project-Daytona/>, with two suites:

- `stable`: tagged releases.
- `edge`: the latest build of `main`, for testing. Only the newest builds are kept.

Add the signing key (fingerprint `E359 F3E2 1E41 45F2 C163 F6AA FA49 BF57 BC03 E090`):

```bash
sudo curl -fsSLo /etc/apt/keyrings/daytona.gpg https://soupbowl.io/Project-Daytona/daytona-archive-keyring.gpg
```

Add the repository, with `Suites: stable` or `Suites: edge`:

```bash
sudo tee /etc/apt/sources.list.d/daytona.sources <<'END'
Types: deb
URIs: https://soupbowl.io/Project-Daytona
Suites: edge
Components: main
Signed-By: /etc/apt/keyrings/daytona.gpg
END
```

Then install:

```bash
sudo apt update && sudo apt install daytona
```

Updates then come with `sudo apt update && sudo apt upgrade`. To report a crash, also install
`daytona-dbgsym` (debug symbols for the shell and Daytona's labwc) so backtraces show function
names. Log out and back in after an update
of Daytona; your settings (colour scheme, wallpaper, taskbar) are kept.

The packages can also be installed by hand: download them from a GitHub release, or from the
**Packages (amd64)** or **Packages (arm64)** artifact of a CI run, or build them yourself (below),
then install both:

```bash
sudo apt install ./daytona_*.deb ./daytona-chicago95_*.deb
```

The packages:

- `daytona` holds the shell, the `daytona` Qt style, `daytona-themetool`, the labwc configuration
  and the session file, and its own build of labwc with Daytona's window frame patch, installed
  privately in `/usr/libexec/daytona` so a system labwc is left alone. It pulls in the Qt
  runtime it needs. It recommends WirePlumber, for the volume keys, pavucontrol-qt, the
  mixer that the speaker icon opens and the Control Panel lists, UPower, for the battery icon,
  and brightnessctl, for the brightness keys (members of the `video` group can change the
  backlight; Debian's installer adds the first user to it).
- `daytona-chicago95` holds the Chicago95 GTK theme, icons and cursors, built from the pinned
  submodule.

Then log out and choose **Daytona** in your login manager's session menu. It is installed as
a Wayland session in `/usr/share/wayland-sessions/daytona.desktop`. GDM, SDDM and LightDM all list
it there.

To build the packages:

```bash
sudo apt install dpkg-dev debhelper fakeroot && sudo apt build-dep .
```

```bash
dpkg-buildpackage -us -uc -b
```

The packages are written to the parent directory. To give them a snapshot version as CI does
(below), run `scripts/snapshot-version.sh` first; it adds an entry to `debian/changelog`.

### Versions and updates

Builds of `main` have a snapshot version made from the commit they were built from, such as
`0.1.0+git20261125222910.412d7ba`: the release version, the commit's date and time (UTC) and its
short hash. Each build is newer than the last, and newer than the release before it, so apt
upgrades to it. Packages installed by hand are upgraded the same way, by installing newer ones over
them. `dpkg -l daytona` shows which version is installed.

Packages built before snapshot versions all have version `0.1.0`. To replace one of those with
another `0.1.0` build, add `--reinstall`.

### Releasing

1. Run `scripts/release.sh 0.2.0 "Summary of the release"` to add the version to
   `debian/changelog`, then commit it and merge it to `main`.
2. Tag that commit and push the tag: `git tag v0.2.0 <commit> && git push origin v0.2.0`.

CI builds the packages for the tag (it checks that the tag matches `debian/changelog`). The apt
workflow (`.github/workflows/apt.yml`) then publishes them as the `v0.2.0` GitHub release and in
the `stable` suite. Builds of `main` keep going to the rolling `edge` prerelease and the `edge`
suite, which keep only the two newest builds. Every deployment rebuilds the whole site, so removed
builds leave it, and the repository is signed with the key in the `GPG_SIGN_PRIVATE` secret
(public key: `packaging/apt/daytona-archive-keyring.asc`).

## Building

Daytona uses git submodules for [Chicago95](https://github.com/grassmunk/Chicago95) and [labwc](https://github.com/labwc/labwc). Clone with:

```bash
git clone --recurse-submodules https://github.com/soup-bowl/Project-Daytona.git
```

or, in an existing clone:

```bash
git submodule update --init
```

Daytona targets Debian 13 (trixie). Install the build dependencies:

```bash
sudo apt install build-essential cmake ninja-build pkgconf qt6-base-dev \
    liblayershellqtinterface-dev libwayland-dev libwayland-bin \
    qt6-wayland-dev qt6-wayland-dev-tools qt6-svg-plugins libpulse-dev clang-format
```

The build also compiles labwc from its submodule with Daytona's patch for NT's raised window
frames (`patches/labwc/`), which needs labwc's own build dependencies:

```bash
sudo apt install meson libwlroots-0.18-dev wayland-protocols libxkbcommon-dev libxcb1-dev \
    libxcb-icccm4-dev libdrm-dev libxml2-dev libglib2.0-dev libcairo2-dev libpango1.0-dev \
    libinput-dev libpixman-1-dev libpng-dev librsvg2-dev libsfdo-dev
```

Configure with `-DDAYTONA_BUILD_LABWC=OFF` to skip it and use the system's labwc instead; window
frames are then flat.

Configure, build and run the tests:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

Pass `-DDAYTONA_WERROR=ON` to treat compiler warnings as errors, as CI does.

### Smoke tests

Smoke tests run a real headless labwc session against an install of the build and check screenshots. They are enabled when these are installed:

```bash
sudo apt install labwc grim wlr-randr qt6-wayland qt6-svg-plugins dbus
```

The pointer tests also need `wlrctl`, and the autostart test `dex` and `gdbus` (`libglib2.0-bin`).

Run only the smoke tests with:

```bash
ctest --test-dir build -L smoke --output-on-failure
```

To add one, write a script in `tests/smoke/` that starts what it needs and checks the screen with `wait_for_pixels` from `lib.sh` (it retries until the pixels match or the time runs out), then register it in `tests/smoke/CMakeLists.txt`:

```cmake
daytona_add_smoke_test(smoke_my_feature smoke_my_feature.sh)
```

The script runs inside the session with `WAYLAND_DISPLAY` set, and the build's `bin/` and test helpers on `PATH`. Check a few specific pixels (a colour at a known position) rather than comparing whole images.

Format code before committing:

```bash
clang-format -i $(git ls-files '*.cpp' '*.h')
```

## Developing in Codespaces

The repository has a dev container (`.devcontainer/`, Debian 13) that works in GitHub Codespaces or any dev container tool. It needs no GPU or display: labwc runs headless with software rendering, and the desktop is shown in your browser through noVNC.

1. Create a Codespace on the `main` branch (Code → Codespaces). Submodules are fetched automatically.
2. Run the VS Code task **Daytona: Run desktop** (Terminal → Run Task…). It configures, builds and
   installs Daytona into `build/prefix`, then starts the desktop. Wait for "Daytona development
   session is running" in its terminal.
3. Open the forwarded port **6080** from the Ports tab and choose `vnc.html` → Connect.
4. In a second terminal, run applications inside the session:

   ```bash
   source /tmp/daytona-dev-$(id -u)/session.env
   qterminal &
   ```

5. Take a screenshot with the **Daytona: Screenshot** task (or `scripts/screenshot.sh [file.png]`).
   Stop the session with **Daytona: Stop desktop** or Ctrl+C in its terminal.

**Daytona: Widget Gallery** opens Qt's Widget Gallery (`qt6-base-examples`) in the running desktop:
most standard widgets in one window, with a check box to disable them all, for checking the Daytona
style. `scripts/widget-gallery.sh widgets/dialogs/standarddialogs` (or `widgets/mainwindows/menus`)
opens another Qt example instead.

Other tasks: **Daytona: Build** (the default build task, Ctrl+Shift+B), **Daytona: Run tests**, and
**Daytona: Run desktop (2×)** and **(1.5×)**, which start the desktop at HiDPI scales (see
[docs/hidpi.md](docs/hidpi.md)). At those scales the noVNC canvas is larger than the logical
desktop: keep noVNC's Scaling Mode on None to check pixels, or use Local Scaling for an overview.
Without VS Code, run the same steps by hand: configure and build as above, then
`cmake --install build --prefix build/prefix` and `scripts/dev-session.sh`. The session uses
`build/prefix` when it exists, so the Daytona and Chicago95 themes, icons and cursors are
available, and starts its own D-Bus session bus if there is none.

Set `DAYTONA_OUTPUT_SIZE` (default `1024x768`) to change the virtual screen size. The VNC server and noVNC listen on localhost only; Codespaces forwards port 6080 privately to you.

The same scripts work outside a container on any Debian 13 machine with the packages from `.devcontainer/Dockerfile` installed.

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE).
