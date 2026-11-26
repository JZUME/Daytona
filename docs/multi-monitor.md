# Multiple outputs

## Behaviour

- **One desktop per output.** Each output has its own desktop surface (colour,
  wallpaper and right-click menu), created and removed as outputs come and go
  (`QGuiApplication::screenAdded` / `screenRemoved`).
- **One taskbar, on the primary monitor,** as in NT 4.0. Wayland has no
  primary output of its own, so Daytona keeps its own: the monitor chosen with
  Display Properties > Settings > **Use this device as the primary monitor**
  (`[Display] PrimaryScreen` in `daytona.ini`), while it is connected and on.
  Otherwise Qt's primary screen is used. When the taskbar's output goes away,
  or another primary is chosen, the taskbar's surface is made afresh on the
  new one, at the same edge and size.
- **Shell popups on the taskbar's output.** The Start menu (and the transparent
  surface that closes it on an outside click) and notification balloons open
  there.
- **Dialogs are placed by labwc.** These are Run, Shut Down, Taskbar
  Properties and Display Properties; a Wayland client cannot choose their
  output, and labwc puts new windows on the output with the pointer.
- **Each output draws at its own scale.** The wallpaper stays one image pixel
  per device pixel on each.

## Arranging monitors

With more than one output, Display Properties' Settings tab adds Windows
2000's multi-monitor controls above and below NT's own (with one output it
stays as NT 4.0 had it):

- **The monitor diagram.** Each output is drawn to scale where it sits on the
  desktop and numbered as in the **Display** list. Dragging one moves it; when
  dropped it snaps edge to edge with another (lining up with that one's edges
  when close) and never overlaps one. The desktop is then moved so its top
  left is at 0,0. Changing a monitor's Desktop Area or Font Size changes its
  size, and monitors beyond its right or bottom edge move with that edge.
- **Display.** Chooses the monitor that the Desktop Area, Refresh Frequency,
  Font Size and check boxes below change; clicking a monitor in the diagram
  does the same.
- **Use this device as the primary monitor.** Puts the taskbar there on
  Apply. It cannot be unticked: tick it on another monitor instead.
- **Extend my desktop onto this monitor.** Unticked, Apply switches the output
  off; it is drawn greyed below the desktop. The primary monitor cannot be
  switched off. Ticked again, it goes back on at the right of the desktop in
  the mode chosen for it (its preferred one at first).

Positions, sizes and on/off state go to the compositor in one
`zwlr_output_management_v1` configuration, with the same "Do you want to keep
these display settings?" prompt (reverting after 15 seconds) as a resolution
change. Once kept they are saved per monitor (its make, model and serial
number, or its connector without a serial), as modes and scales are:

```ini
[ScreenPosition]
HEADLESS-1=0,0
HEADLESS-2=1280,0

[ScreenEnabled]
HEADLESS-3=false

[Display]
PrimaryScreen=HEADLESS-2
```

When the shell starts, and when a monitor is connected, the saved state is
restored: switched-off monitors are switched off again (never the last one
on), and the saved positions are applied only if every monitor that is on has
one and they do not overlap at the monitors' current sizes. Otherwise labwc's
own layout is kept. Mirroring, a taskbar per monitor and moving windows
between monitors are not offered.

## Testing

The dev session can start several headless outputs:

```bash
DAYTONA_OUTPUTS=2 scripts/dev-session.sh
```

Arrange them in Display Properties > Settings, or with wlr-randr (this also
gives one a different scale):

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

The `smoke_screen_layout` test starts a session with three outputs
(`DAYTONA_SMOKE_OUTPUTS=3`) and a saved arrangement that moves two of them,
switches the third off and makes the second primary, then checks the
positions, that the third is off and that the taskbar is on the second.

Results with labwc 0.8.3 (Ubuntu 25.04, headless), arranging two outputs in
the dialog: dragging, Apply and Yes moved the outputs and saved them; letting
the prompt time out put them back; choosing the other primary moved the
taskbar at once; unticking Extend switched the output off, and the shell and
the dialog (moved by labwc to the remaining output) kept running.
