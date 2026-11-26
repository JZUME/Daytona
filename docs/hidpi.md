# HiDPI

Daytona is designed in logical pixels and drawn at the output's scale (see
AGENT.md → HiDPI and Multi-Monitor). This page records how scaling is tested
and what was found.

## Setting the scale

Display Properties > **Settings** > **Font Size** sets the scale of the screen
the dialog is on, as NT's font size setting did:

- **Small Fonts** is 100% and **Large Fonts** 150%. NT's Large Fonts was
  125% (120 DPI); Daytona uses 150%, the more common choice for today's
  high-density screens (an approved exception in AGENT.md). 125% is under
  Custom....
- **Custom...** opens NT's Custom Font Size dialog for any size from 100% to
  300%, such as 150% or 200% for a high-density screen.

Apply changes the screen at once and asks whether to keep the new size. If
nobody answers within 15 seconds, the previous size comes back, so a size
that makes the screen hard to use undoes itself.

A kept size is saved in `daytona.ini` (`[ScreenScale]`) and applied whenever
that screen appears, including at login. A monitor that reports a serial
number is remembered by make, model and serial number, so the size follows it
to another port. Otherwise the size belongs to the connector (for example
`eDP-1`). The shell sets scales through wlr-output-management, as `wlr-randr`
does, so a size set with `wlr-randr` or kanshi is replaced by the saved one
only at the next login or when the screen next appears.

## The mouse pointer

Daytona's cursor theme ("Daytona Cursors") has every cursor at each screen
scale and pointer scheme, so the pointer is NT's size at every font size:
the session sets the scheme's size (`XCURSOR_SIZE` 32, 64 or 96) and labwc
and applications multiply it by the screen scale. Chicago95's own cursors
come in one size only (32 pixels), so at 200% they were drawn at half size;
`smoke_pointer` checks this no longer happens.

Mouse Properties (Start > Settings > Mouse...) chooses the scheme: Standard,
Standard (large) or Standard (extra large), from the next log on. Larger
sizes are redrawn as Windows' large cursors were, with the body enlarged and
the outline left 1 pixel thin, instead of every pixel becoming a block.

## Running at another scale

The dev session takes the output size in device pixels and a scale:

```bash
DAYTONA_OUTPUT_SIZE=2048x1536 DAYTONA_OUTPUT_SCALE=2 scripts/dev-session.sh
```

```bash
DAYTONA_OUTPUT_SIZE=1536x1152 DAYTONA_OUTPUT_SCALE=1.5 scripts/dev-session.sh
```

In VS Code the tasks **Daytona: Run desktop (2×)** and **Daytona: Run desktop
(1.5×)** run these. In noVNC, leave Scaling Mode on None to see device pixels
one for one; Local Scaling (or `vnc.html?resize=scale`) fits the desktop to the
browser but blurs it, so don't judge sharpness that way. The **Daytona:
Screenshot** task saves the full-resolution image to `build/screenshot.png`.

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
