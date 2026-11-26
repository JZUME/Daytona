# Colour schemes

Daytona uses Windows colour schemes: the same 29 named colours as NT 4.0's
Display Properties, with values from a built-in scheme or a Windows 95, NT or
Plus! `.theme` file.

## Applying a scheme

```bash
daytona-themetool apply --scheme "High Contrast Black"
```

```bash
daytona-themetool apply "/path/to/Dangerous Creatures.theme"
```

`apply` writes the scheme to `colors.ini` and records its name as
`Appearance/ColourScheme` in `daytona.ini`. The shell, and every Qt
application using the Daytona style, follows changes to `colors.ini` and
recolours at once, without a restart.

Built-in schemes: Windows Standard, High Contrast Black and High Contrast White.

`apply` also:

- **Window frames:** writes a labwc theme named `Daytona` to
  `$XDG_DATA_HOME/themes/Daytona/openbox-3`, which labwc finds before the
  shipped one, and runs `labwc --reconfigure`.
- **GTK 3 applications:** writes a `Daytona` GTK theme to
  `$XDG_DATA_HOME/themes/Daytona/gtk-3.0/gtk.css`. It imports Chicago95 and
  redefines Chicago95's named colours (`bg_color`, `selected_bg_color`, ...).
  The Daytona session sets `GTK_THEME=Daytona`, so only GTK applications in
  that session are affected. GTK applications pick up the change when they
  next start. Chicago95's images (check boxes, scroll arrows) keep their
  Windows Standard colours.
- **GTK icons and cursors:** sets `gtk-icon-theme-name=Chicago95`,
  `gtk-cursor-theme-name=Daytona Cursors` and `gtk-cursor-theme-size` (the
  pointer scheme's size) in `$XDG_CONFIG_HOME/gtk-3.0/settings.ini`, keeping
  the rest of the file. Once that file names a cursor theme, the shell keeps
  its cursor size in step with Mouse Properties at each log on. The
  first time it changes an existing file, the original is kept as
  `settings.ini.daytona-backup`. On systems with GNOME's
  `org.gnome.desktop.interface` settings schema installed, GTK on Wayland can read those
  instead; Daytona does not change them.
- **Wallpaper:** copies a `.theme` file's wallpaper to
  `$XDG_DATA_HOME/daytona/wallpaper/` and records it, with its tile, centre, stretch, fit or fill mode, in
  `daytona.ini`.

For Windows Standard, the generated labwc and GTK themes are removed, so the
shipped ones apply. Only directories Daytona generated (marked with a
`.daytona-generated` file) are ever replaced or removed.

`apply` lists the parts of a `.theme` file Daytona does not support: Windows
cursors (`.cur`/`.ani`; the Chicago95 cursors stay in use), sounds, screen
savers, desktop icons, and fonts and window metrics.

## The colour file

`$XDG_CONFIG_HOME/daytona/colors.ini` (normally `~/.config/daytona/colors.ini`)
is a small INI file:

```ini
[Scheme]
Name="Windows Standard"

[Colors]
Scrollbar=192 192 192
Background=0 128 128
ActiveTitle=0 0 128
; ... one line for each Windows colour key
```

- `[Scheme] Name` is the scheme's name, for display only. It is quoted so any
  text survives.
- `[Colors]` holds Windows colour keys with `R G B` values from 0 to 255, as in
  the `[Control Panel\Colors]` section of a `.theme` file.
- A missing key, or a value that is not three numbers from 0 to 255, uses the
  Windows Standard colour. A missing or unreadable file means Windows Standard.
- Files over 64 KiB are ignored.
- `daytona-themetool` writes every key, in Windows order, and replaces the file
  atomically (it writes a temporary file and renames it into place), so readers
  never see half a file.

The file can be edited by hand; changes are picked up when it is saved.

## Windows colours and Qt palette roles

| Windows key | Qt palette role | Notes |
|---|---|---|
| ButtonFace | Window, Button, AlternateBase; Disabled Base | Dialogs are drawn in the 3D face colour. |
| ButtonText | WindowText, ButtonText | Text on the face colour. |
| ButtonHilight | Light, BrightText | Outer highlight of raised edges. |
| ButtonLight | Midlight | Inner highlight. The same as ButtonFace in Windows Standard. |
| ButtonShadow | Mid, Dark; Disabled Highlight | Inner shadow. |
| ButtonDkShadow | Shadow | Outer shadow. |
| Window | Base | Edit fields and lists. |
| WindowText | Text | Text in edit fields and lists. |
| Hilight | Highlight | Selected items. |
| HilightText | HighlightedText | |
| InfoWindow | ToolTipBase | |
| InfoText | ToolTipText | |
| HotTrackingColor | Link | |
| GrayText | PlaceholderText; Disabled WindowText, ButtonText, Text, HighlightedText | The style adds NT's embossed look. |

The Inactive colour group is the same as Active.

The other keys have no Qt palette role. Daytona reads them with
`Daytona::Style::schemeColour()`:

- **ActiveTitle, InactiveTitle, TitleText, InactiveTitleText,
  GradientActiveTitle, GradientInactiveTitle, ActiveBorder, InactiveBorder,
  WindowFrame:** window decorations, which labwc draws. They will be converted
  to its theme by a later change.
- **Background:** the desktop colour.
- **Menu, MenuText:** menus. In NT's built-in schemes these equal ButtonFace and
  ButtonText, which menus currently use.
- **Scrollbar, AppWorkspace, ButtonAlternateFace:** kept for completeness.

## Third-party applications

A Qt 6 application run with `QT_STYLE_OVERRIDE=daytona` (with the Daytona style
plugin installed) loads `colors.ini` when it starts and follows changes to it,
the same as the shell.
