# Multiple outputs

## Behaviour

- **One desktop per output.** Each output has its own desktop surface (colour,
  wallpaper and right-click menu), created and removed as outputs come and go
  (`QGuiApplication::screenAdded` / `screenRemoved`).
- **One taskbar, on the primary output,** as in NT 4.0. Wayland has no
  primary output of its own; Qt's primary screen is used. When that output
  goes away, Qt picks a new primary and the taskbar's surface is made afresh
  there, at the same edge and size.
- **Shell popups on the taskbar's output.** The Start menu (and the transparent
  surface that closes it on an outside click) and notification balloons open
  there.
- **Dialogs are placed by labwc.** These are Run, Shut Down, Taskbar
  Properties and Display Properties; a Wayland client cannot choose their
  output, and labwc puts new windows on the output with the pointer.
- **Each output draws at its own scale.** The wallpaper stays one image pixel
  per device pixel on each.

## Testing

The dev session can start several headless outputs:

```bash
DAYTONA_OUTPUTS=2 scripts/dev-session.sh
```

Arrange them and give one a different scale with wlr-randr:

```bash
wlr-randr --output HEADLESS-1 --pos 0,0 --output HEADLESS-2 --custom-mode 2048x1536 --scale 2 --pos 1024,0
```

Results with labwc 0.8.3 (Debian 13):

- **Two outputs, one at scale 2:** both showed their desktop. The 2× output
  drew its wallpaper at half the logical size, as expected. Only one output
  showed the taskbar.
- **Removing an output at runtime** (`wlr-randr --output NAME --off`):
  - removing the output without the taskbar removed its desktop
  - removing the taskbar's output moved the taskbar to the remaining output,
    where the Start menu then opened
  - the shell kept running in both cases
- **Re-enabling an output** (`--on`) **crashes labwc itself** in this headless
  setup: wlroots' output management asserts (`head_send_state: Assertion
  'found' failed`) after "failed to get output layout box". The shell's
  handling of an added output is the same code that creates desktops at
  start-up, but adding an output at runtime could not be tested here.
