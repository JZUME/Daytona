# Third-party applications

Daytona makes other applications look like NT 4.0 where their toolkit allows
it, without changing anything outside the Daytona session.

## Qt 6

The `daytona` style is a Qt style plugin (`libdaytonastyle.so`), installed in
Qt 6's `plugins/styles` directory. The Daytona session sets

```bash
export QT_STYLE_OVERRIDE=daytona
```

so every Qt 6 Widgets application started in the session uses it. To try it
elsewhere, set the variable for one application:

```bash
QT_STYLE_OVERRIDE=daytona featherpad
```

With another install prefix, add `<prefix>/lib/<arch>/qt6/plugins` to
`QT_PLUGIN_PATH`. qt6ct also lists `daytona` among its styles.

What applications get:

- **Look:** the NT look for every common control. This covers buttons, check
  boxes and radio buttons, line edits, spin boxes, combo boxes, menus and menu
  bars, tool bars, tabs, scroll bars, sliders (NT trackbar), progress bars (NT's
  segmented blocks), tree views, list views, table views, headers, dock widgets,
  status bars and size grips, MDI windows, splitters, group boxes, dials and the
  calendar.
- **Colours:** the current Daytona colour scheme (`colors.ini`), followed live
  while the application runs (see `docs/colour-schemes.md`).
- **No animation:** menus, combo box lists, tooltips and tool boxes appear
  at once.
- **Icons:** Chicago95 icons when the application has no icon theme of its own
  (no platform theme set one).

`tests/style-preview` renders all of these controls:

```bash
daytona-style-preview --save-controls controls.png --scale 2
```

### Tested applications

Tested in the Debian 13 dev session with `QT_STYLE_OVERRIDE=daytona`:

| Application | Result |
| --- | --- |
| FeatherPad 1.6 (text editor) | NT menus, tool bar, tabs, search bar and status bar. No problems found. |
| PCManFM-Qt 2.1 (file manager) | NT menus, tool bar, side pane, scroll bars and status bar. File icons appear once the Chicago95 icon theme is used (set by the style when the app has none). |
| qt6ct 0.10 (settings tool) | Dialog, tabs, group boxes, combo boxes and buttons in NT style. Its preview pane deliberately shows the style chosen in qt6ct itself (Fusion by default), not Daytona. |

Known limits:

- **Application styling wins.** Applications that set their own style sheet
  or force a style (`QApplication::setStyle`) keep their look in those places.
- **No Qt Quick / QML.** Qt Quick Controls do not use widget styles, so QML
  applications keep their own look.

## Qt 5

Qt 5 cannot load Qt 6 plugins. A Qt 5 application in the session sees
`QT_STYLE_OVERRIDE=daytona`, reports that the style is unknown, and falls back
to its default style (usually Fusion). Qt 5's built-in `Windows` style is the
closest match. It can be chosen per application with
`QT_STYLE_OVERRIDE=windows` or for all Qt 5 applications with qt5ct, but Qt 5
applications do not follow Daytona's colour scheme.

## GTK 3

See `docs/colour-schemes.md`: GTK 3 applications in the session use Chicago95
in the current colour scheme (`GTK_THEME=Daytona`). GTK 4 and libadwaita
applications are out of scope.

## File manager

Daytona has no file manager of its own. PCManFM-Qt is recommended by the
package: it is Qt 6, so it takes the `daytona` style (see Tested
applications above), and the session uses it for folders and searching.

- **Folders:** Daytona installs `share/applications/daytona-mimeapps.list`,
  which makes `pcmanfm-qt.desktop` the default for `inode/directory` in the
  Daytona session only (the freedesktop default-applications lists are read
  per desktop, `daytona-mimeapps.list` for `XDG_CURRENT_DESKTOP=Daytona`).
  `xdg-open`, GLib and Qt applications read it, so a folder typed in Run, a
  folder in Documents and other programs' "open containing folder" open in
  PCManFM-Qt. Choices of the user's own (`~/.config/mimeapps.list`, for
  example from `xdg-mime default`) or the administrator's
  (`/etc/xdg/mimeapps.list`) win. Without PCManFM-Qt, the entry is skipped
  and the system's default file manager opens folders.
- **Find:** Start > Find > Files or Folders runs the command in
  `Commands/Find` in `daytona.ini`, as a command line without a shell. When
  that is not set and PCManFM-Qt is installed, it opens PCManFM-Qt's Find
  Files window, looking in the home folder. With neither, the item is greyed
  out. To use another search tool:

  ```ini
  [Commands]
  Find=catfish --path /home/me
  ```

- **Desktop:** PCManFM-Qt can also draw a desktop with icons
  (`pcmanfm-qt --desktop`). Daytona's desktop is its own, so this must not
  run: LXQt's autostart entry for it (`lxqt-desktop.desktop`) is
  `OnlyShowIn=LXQt` and does not start in the Daytona session, and Daytona
  never starts it.

## Autostart applications

At log on, after the shell has started, Daytona starts the applications in
the XDG autostart directories, as other desktops do: `~/.config/autostart`
(`$XDG_CONFIG_HOME/autostart`) and `/etc/xdg/autostart` (each directory in
`$XDG_CONFIG_DIRS`). This is how tray helpers such as `blueman-applet`
and programs such as `xdg-user-dirs-update` start. A few helpers whose own
entries leave Daytona out are started by the labwc `autostart` file
instead: the polkit agent, the keyring and the network icon (see below).

The rules are those of the freedesktop Desktop Application Autostart
specification:

- A user entry replaces the system entry with the same file name. To stop a
  system entry starting, copy it to `~/.config/autostart` and add
  `Hidden=true`.
- Entries with `Hidden=true` do not start.
- `OnlyShowIn` and `NotShowIn` are checked against the desktop name
  `Daytona` (the session's `XDG_CURRENT_DESKTOP`), so an entry with
  `OnlyShowIn=GNOME;` or `NotShowIn=Daytona;` does not start.
- An entry whose `TryExec` program is not installed does not start.

Daytona's labwc `autostart` file starts the shell, waits for the shell's
notification area to appear on the session bus (up to 30 seconds), so that
tray icons register with it, and then runs
[`dex`](https://github.com/jceb/dex) `--autostart --environment Daytona`.
dex starts every entry without waiting for it, and all of this runs in the
background, so an entry that is slow or fails to start cannot hold up the
shell. dex and `gdbus` (`libglib2.0-bin`) are recommended by the Daytona
package; without dex, no autostart entries start.

There is no Startup folder in the Start menu and no editor for these
entries.

To see what would start, and why other entries are skipped:

```bash
dex --autostart --environment Daytona --dry-run --verbose
```

## Programs started by D-Bus and systemd

Some programs are not started from the session but by D-Bus activation or
as systemd user services: portal dialogs such as the file chooser (see
below), `gnome-terminal`'s server, applications with `DBusActivatable=true`
and `systemd --user` services. They get the environment of the D-Bus and
systemd user managers, not the session's, so without help they would open
without the Daytona theme.

labwc adds `WAYLAND_DISPLAY`, `XDG_CURRENT_DESKTOP`, `DISPLAY`, the cursor
variables, `XDG_SESSION_TYPE`, `LABWC_PID` and `LABWC_VER` to both
activation environments when it starts. Daytona's labwc `autostart` file adds
`GTK_THEME`, `QT_STYLE_OVERRIDE` and `XDG_SESSION_DESKTOP`, so these programs
take the Daytona GTK theme and Qt style like the rest of the session.

When the session ends (Log off, or labwc exiting for any other reason, even
if the shell crashed), labwc runs Daytona's labwc `shutdown` file, which
takes them out again (`systemctl --user unset-environment`; D-Bus can only
set them empty), as labwc does for its own. Otherwise a later GNOME or KDE
session for the same user on the same systemd user manager would start
programs in the Daytona theme. If labwc itself crashes, neither labwc nor
Daytona can clean up, and the variables stay until the user's systemd
manager stops (all of the user's sessions end) or the next Daytona log off.

Both follow labwc's own rule for when to touch the activation environments:
only when labwc runs the screens itself (its DRM backend), not when it runs
in a window inside another desktop, whose environment that is.
`daytona-session` makes that decision for both and passes it on as
`LABWC_UPDATE_ACTIVATION_ENV`; set that variable yourself to override it.

To check, in a Daytona session:

```bash
systemctl --user show-environment | grep -E 'GTK_THEME|QT_STYLE_OVERRIDE'
```

## File dialogs and screen sharing (portals)

Sandboxed applications (Flatpak, Snap) and many others (Firefox and
Chromium for screen sharing, and for their file dialogs when configured to,
GTK applications run with `GTK_USE_PORTAL=1`) ask
[xdg-desktop-portal](https://flatpak.github.io/xdg-desktop-portal/) for file
dialogs, screen sharing and screenshots. The portal hands each request to a
backend chosen for the desktop. Daytona's choice is
`share/xdg-desktop-portal/daytona-portals.conf`:

| Request | Backend | Why |
| --- | --- | --- |
| File Open and Save dialogs | `xdg-desktop-portal-lxqt`, else `-gtk` | LXQt's dialog is Qt, takes the `daytona` style and Chicago95 icons, and has NT's layout (the folder list, File name and Files of type, Open and Cancel). GTK's has a sidebar and path bar of its own. |
| Screen sharing (ScreenCast), screenshots | `xdg-desktop-portal-wlr` | Captures labwc's screens through wlroots' protocols. |
| Everything else (Open With, printing, appearance settings) | `xdg-desktop-portal-gtk` | GTK 3 dialogs in the Daytona theme. |

The Daytona package recommends all three backends. Each is started by D-Bus
when first needed, with the theme variables described above.

When a program asks to share the screen, Daytona asks which screen (or
window, with an xdg-desktop-portal-wlr and labwc new enough to share single
windows) it may share, in its own Share Screen dialog
(`daytona-shell --choose-screen`). With one screen the list holds "Entire
screen"; Cancel refuses the request. xdg-desktop-portal-wlr runs the dialog
as its chooser, set in `/etc/xdg/xdg-desktop-portal-wlr/Daytona`;
xdg-desktop-portal-wlr reads that file only from `/etc/xdg`, so an install
with another prefix needs it copied there. Without a chooser,
xdg-desktop-portal-wlr refuses every request.

To see the portals running in the session:

```bash
busctl --user list | grep portal
```

## Administrator password prompts (polkit)

When an application needs administrator rights through polkit (`pkexec`,
mounting a drive in a file manager, changing the time zone, joining a system
Wi-Fi network, or Shut Down when logind asks for authentication), a polkit
authentication agent asks for the password. Daytona uses LXQt's agent,
`lxqt-policykit-agent` from the `lxqt-policykit` package, which the Daytona
package recommends. It is a Qt 6 application, so its dialog takes the
`daytona` style.

Its own autostart entry is limited to LXQt (`OnlyShowIn=LXQt;`), so
Daytona's labwc `autostart` file starts it directly, alongside the shell,
whenever it is installed. Without it, actions that need a password fail or
do nothing.

To check it works:

```bash
pkexec true
```

## Saved passwords (keyring)

Applications keep saved passwords (Wi-Fi and VPN keys, mail accounts,
browser passwords, sign-ins of Electron applications) in the keyring, through
the freedesktop Secret Service (`org.freedesktop.secrets`). Daytona uses
GNOME's keyring, `gnome-keyring-daemon` from the `gnome-keyring` package,
which the Daytona package recommends. LXQt has no keyring of its own, and
gnome-keyring is the one libsecret applications expect. It has no window of
its own in Daytona; there is no keyring manager in the Control Panel.

Its own autostart entry is limited to GNOME, Unity and MATE, so Daytona's
labwc `autostart` file starts it
(`gnome-keyring-daemon --start --components=secrets`), before the autostart
applications.

**Unlocked at log on.** The keyring is unlocked with the log-on password
when `libpam-gnome-keyring` (also recommended) is installed. The login
managers in Debian 13, GDM, LightDM, SDDM and greetd, already have
`pam_gnome_keyring` in their PAM settings; it starts the daemon while
logging on and unlocks the "Login" keyring with the password typed there.
The `autostart` line then finishes starting that daemon instead of starting
a second one. The first time, the Login keyring is created with the log-on
password. Without the PAM module, or after an automatic log on (no password
is typed), the keyring is locked, and the first application to use it asks
for the keyring's password (or, the first time, for a password for a new
keyring) in a GTK 3 dialog. Changing the log-on password with `passwd`
changes the keyring's too.

**Chromium and Electron applications** choose where to keep passwords from
the desktop's name (`XDG_CURRENT_DESKTOP`). They know GNOME, KDE, Xfce,
Cinnamon and a few others; for any other desktop, Daytona and LXQt included,
they use their "basic" store, which encrypts with a key that is the same on
every computer, and VS Code warns that it could not identify the keyring.
Daytona does not pretend to be another desktop to change this: a desktop
name such as `Daytona:GNOME` would also start GNOME's autostart entries, make
Qt load its GTK platform theme and make Chromium read GNOME's proxy settings,
and the older `GNOME_DESKTOP_SESSION_ID` makes `xdg-open` and
`xdg-screensaver` treat the session as GNOME. Instead, they are told to use
the keyring with `--password-store=gnome-libsecret`:

- **Chromium** (Debian's package): Daytona installs
  `/etc/chromium.d/daytona`, which Debian's `chromium` wrapper reads, and
  which adds the option in the Daytona session only.
- **VS Code**: add `"password-store": "gnome-libsecret"` to
  `~/.vscode/argv.json` (Command Palette, Preferences: Configure Runtime
  Arguments).
- **Google Chrome and other Electron applications** have no file for
  options: add `--password-store=gnome-libsecret` to the `Exec` line of a
  copy of the application's `.desktop` file in
  `~/.local/share/applications`.

Passwords an application already saved in its basic store stay readable
after the switch. Firefox and Thunderbird keep passwords in their own
profile and do not use the keyring.

To check the keyring works (`secret-tool` is in `libsecret-tools`):

```bash
echo hello | secret-tool store --label=Test daytona test
secret-tool lookup daytona test
```

## Network

NetworkManager's applet, `nm-applet` from the `network-manager-applet`
package (which the Daytona package recommends), is the network icon in the
notification area. Clicking it shows its menu, drawn by Daytona as an NT
menu: the Wi-Fi networks in range, VPN connections, Enable Networking and
Edit Connections. It asks for Wi-Fi and VPN passwords itself, in GTK 3
dialogs that take the Daytona colour scheme. Joining a network for all users
asks for an administrator password through the polkit agent.

The applet's own icon for the X11 tray would not show on Wayland, so Daytona
replaces its system autostart entry with one that runs
`nm-applet --indicator` (a StatusNotifierItem). Daytona's entries are in
`share/daytona/xdg/autostart`, which the labwc `autostart` file puts first
in `XDG_CONFIG_DIRS` for dex when `nm-applet` is installed; users' own
entries in `~/.config/autostart` still win. While `nm-applet` is installed,
LXQt's `nm-tray` (installed with LXQt) does not start in Daytona, so there is
only one network icon.

Edit Connections opens NetworkManager's Network Connections
(`nm-connection-editor`). The same program is in the Control Panel as
"Advanced Network Configuration", as its own `.desktop` entry is a Settings
application.

## Notification area (tray icons)

The notification area shows icons that applications register over D-Bus
(StatusNotifierItem): Daytona runs the `org.kde.StatusNotifierWatcher`
service and draws each icon, tooltip and menu itself. Qt's
`QSystemTrayIcon`, KDE applications, Electron applications, Chromium,
Firefox and GTK applications using libayatana-appindicator all use it.

A left click activates an icon and a right click shows its menu, beside the
taskbar at the icon. Icons that are only a menu (`ItemIsMenu`), and
libayatana-appindicator icons, which have no activate action, show their
menu on a left click too.

Applications that only know the older X11 tray (XEmbed), which embeds the
application's own small window in the taskbar, show no icon. This includes
GTK 2 and GTK 3's `GtkStatusIcon` (removed in GTK 4), older Wine programs and
plain X11 utilities. Wayland does not let one program embed another
program's window, so there is nowhere to put such an icon; the rest of the
application works as usual. Other Wayland desktops have the same limit. A
bridge such as KDE's `xembedsniproxy`, which turns XEmbed icons of Xwayland
applications into StatusNotifierItems, could be added later.

Notifications sent through `org.freedesktop.Notifications` (`notify-send`,
libnotify) are shown as Windows 2000 balloons pointing at the notification
area.

To try both in the dev session, with the desktop running:

```bash
scripts/widget-gallery.sh widgets/desktop/systray
```

```bash
source /tmp/daytona-dev-$(id -u)/session.env
notify-send "Hello" "A test notification"
```
