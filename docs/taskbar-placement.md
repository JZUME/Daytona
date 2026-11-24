# Taskbar placement: spike results

Before building taskbar dragging and docking (AGENT.md → Taskbar Placement),
two compositor behaviours it relies on were checked. Both hold. This page
records how they were tested and what the implementation must take into
account.

## Environment

- **Compositor:** labwc 0.8.3 (Debian 13 package), headless backend with the
  pixman renderer, 1024×768 output at scale 1.
- **Shell:** Qt 6.8.2 Widgets with LayerShellQt 6.3, `daytona-shell` built
  from the Qt edition (then on the `qt-edition` branch) with temporary debug code. The debug code was not
  merged; it lived only in a throwaway checkout.
- **Input:** wayvnc + noVNC, driven by Playwright (`mouse.down`, stepped
  `mouse.move`, `mouse.up`), so the compositor saw a real pointer device.
- **Windows:** the smoke-test window, maximised, to watch re-flow.

A real (non-headless) session was not available for this test.

## 1. Implicit pointer grab: **works**

**Test:** an application-wide event filter logged every press, motion and
release on the shell's top-level windows. The pointer was pressed on an empty
part of the taskbar (x 700, 15 px into the 28 px bar), then dragged up out of
the surface, across the screen to (20, 20), and released.

**Result:** after the press, every motion event kept arriving at the taskbar,
with positions outside the surface (local y down to −720), and the release
was delivered to the taskbar too. 24 motion events, none lost.

```
MouseButtonPress   Daytona::Taskbar local (700,15)   buttons 1
MouseMove          Daytona::Taskbar local (700,0)    buttons 1
MouseMove          Daytona::Taskbar local (697,-10)  buttons 1
...
MouseMove          Daytona::Taskbar local (20,-720)  buttons 1
MouseButtonRelease Daytona::Taskbar local (20,-720)  buttons 0
```

**Caveat for the implementation:** on layer surfaces, Qt's "global"
position is the same as the local one. Qt does not know where a layer
surface sits on the output (layer-shell does not tell clients). The pointer's
position on the screen must be worked out from the taskbar's own placement,
for example for a bottom taskbar `screen.y = screenHeight − barHeight + local.y`.
The nearest edge is then the one closest to that point.

## 2. Re-anchoring a mapped surface at runtime: **works, with a repaint**

**Test:** a debug timer changed the mapped taskbar's LayerShellQt anchors,
exclusive zone and widget size, moving it bottom → top → left (120 px wide) →
right → bottom, with a maximised window open.

**Result:** the taskbar moved to every edge, and labwc re-flowed the
maximised window to the new usable area each time (below a top taskbar,
beside a left or right one).

**Caveat for the implementation:** `setAnchors()`, `setExclusiveZone()` and
`setMargins()` only take effect on the next surface commit. A move that also
changes the widget's size (bottom → left, right → bottom) commits by itself,
because the new size makes Qt draw a new frame. A move that keeps the same
size (bottom → top, left → right) **did not appear at all until the taskbar
was repainted**. Calling `update()` on the taskbar after changing the layer
properties fixed it in every case. The implementation should always request a
repaint after re-anchoring.

Also seen:

- **Size after a move is set by the compositor.** After switching to a
  vertical edge the widget was 120×28 at first, then 120×768 once labwc sent
  its configure. Layouts should follow `resizeEvent` rather than assume the
  size at the moment of the change.
- **No need to recreate the native window.** Re-anchoring a surface that
  stays mapped worked without destroying it, unlike hide/show, which loses the
  layer role in LayerShellQt 6.3 (see the Start menu code).

## Conclusion

Both mechanisms AGENT.md relies on work in labwc 0.8.3 with LayerShellQt 6.3.
No workaround is needed beyond:

- computing screen positions from the taskbar's known placement
- repainting after changing anchors
