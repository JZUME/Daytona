# Project Daytona (Qt edition)

A Windows 95 / Windows NT 4.0 style desktop shell for Linux.

Daytona is a small, standalone Qt 6 Widgets shell (taskbar, Start menu, notification area, dialogs) that runs on the [labwc](https://github.com/labwc/labwc) Wayland compositor. It ships a Win95 Qt widget style and a converter for Windows `.theme` files, and uses [Chicago95](https://github.com/grassmunk/Chicago95) for GTK applications, icons and cursors.

Status: early development. See [AGENT.md](AGENT.md) for the design and project rules.

Development happens on the `qt-edition` branch.

## Building

Daytona uses a git submodule for [Chicago95](https://github.com/grassmunk/Chicago95). Clone with:

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
    qt6-wayland-dev qt6-wayland-dev-tools qt6-svg-plugins clang-format
```

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

1. Create a Codespace on the `qt-edition` branch (Code → Codespaces). Submodules are fetched automatically.
2. Build as described above.
3. Start the session:

   ```bash
   scripts/dev-session.sh
   ```

4. Open the forwarded port **6080** from the Ports tab and choose `vnc.html` → Connect.
5. In a second terminal, run applications inside the session:

   ```bash
   source /tmp/daytona-dev-$(id -u)/session.env
   qterminal &
   ```

6. Take a screenshot with `scripts/screenshot.sh [file.png]`. Stop the session with Ctrl+C.

Set `DAYTONA_OUTPUT_SIZE` (default `1024x768`) to change the virtual screen size. The VNC server and noVNC listen on localhost only; Codespaces forwards port 6080 privately to you.

The same scripts work outside a container on any Debian 13 machine with the packages from `.devcontainer/Dockerfile` installed.

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE).
