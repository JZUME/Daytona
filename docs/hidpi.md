# HiDPI

Daytona is designed in logical pixels and drawn at the output's scale (see
AGENT.md → HiDPI and Multi-Monitor). This page records how scaling is tested
and what was found.

## Running at another scale

The dev session takes the output size in device pixels and a scale:

```bash
DAYTONA_OUTPUT_SIZE=2048x1536 DAYTONA_OUTPUT_SCALE=2 scripts/dev-session.sh
```

```bash
DAYTONA_OUTPUT_SIZE=1536x1152 DAYTONA_OUTPUT_SCALE=1.5 scripts/dev-session.sh
```

Both give a 1024×768 logical desktop. The scale is set with `wlr-randr --scale`
on the headless output, which labwc 0.8.3 and wlr-randr 0.4.1 (Debian 13)
support, including fractional scales.

The smoke tests run each test twice, at scale 1 and as `NAME_2x` at scale 2
(`DAYTONA_SMOKE_SCALE`). Pixel checks are written in logical pixels and
checked at the first device pixel each one covers.

## Results (labwc 0.8.3, Qt 6.8.2)

Checked at 1×, 1.5× and 2×:

- **Taskbar:** height, raised edge and the Start button's bevel.
- **Start menu:** banner and icons.
- **Window buttons and the notification area:** window button icons, tray icons
  and clock text.
- **Notification balloons.**
- **Dialogs:** Run, Taskbar Properties and Display Properties.
- **The desktop wallpaper.**
- **labwc's window frames and caption buttons.**

- **1× and 2×: no blurry lines.** Every bevel line is a whole number of device
  pixels: 1 at 1×, exactly 2 at 2×, with no blended colours. Icons are drawn
  from the icon theme at the device size (Chicago95 has 32 and 48 pixel art;
  larger sizes are scaled from those). labwc draws its frame and the SVG
  caption buttons at the output scale.
- **1.5×: acceptable.** Qt rounds 1 logical pixel to 1 or 2 device pixels, so
  bevel lines are 1 or 2 pixels wide in places, but always solid colours,
  never blurred. Text and icons are sharp. This is the expected look for a
  pixel-art style at a fractional scale.
- **The wallpaper** is drawn one image pixel per device pixel, as NT did, so a
  wallpaper looks half the size at 2×. This is deliberate (AGENT.md), not a
  bug.

No fixes were needed in the shell for these scales. Different scales on
different outputs are covered by the multi-monitor work.
