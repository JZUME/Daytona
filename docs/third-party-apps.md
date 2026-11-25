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

## Notification area (tray icons)

The notification area shows icons that applications register over D-Bus
(StatusNotifierItem): Daytona runs the `org.kde.StatusNotifierWatcher`
service and draws each icon, tooltip and menu itself. Qt's
`QSystemTrayIcon`, KDE applications, Electron applications, Chromium,
Firefox and GTK applications using libayatana-appindicator all use it.

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
