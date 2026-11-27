# Daytona's labwc patches

Each `.patch` here is applied, in name order, to a copy of the
`third_party/labwc` submodule by `scripts/build-labwc.sh`. They are
`git format-patch` files against the pinned labwc tag, kept small and written
so they could be submitted upstream as they are.

| Patch | What it adds |
| --- | --- |
| `0001-ssd-add-optional-bevelled-Windows-9x-NT-style-window.patch` | Theme option `window.border.bevel` with four colours (`window.border.bevel.{light,highlight,shadow,darkshadow}.color`): the outer two pixels of the window border, round the whole frame, are drawn as a raised 3D edge, as Windows NT frames are. Off by default. Fixes issue #86. |
| `0002-ssd-show-a-pressed-image-while-a-titlebar-button-is-.patch` | Pressed titlebar button images (`BUTTON[_toggled]_pressed`, for example `close_pressed-active.svg`), shown while the left mouse button is held on a button with the cursor over it, as NT's caption buttons sink in. Themes without them look as before. |

## Updating the pinned labwc version

1. Check out the new tag in the submodule:

   ```bash
   git -C third_party/labwc fetch --tags && git -C third_party/labwc checkout <tag>
   ```

2. Rebase the patches onto it in a scratch clone:

   ```bash
   git clone third_party/labwc /tmp/labwc && cd /tmp/labwc && git checkout <tag>
   ```

   ```bash
   git am /path/to/Daytona/patches/labwc/*.patch
   ```

   Fix any conflicts, then write the patches back:

   ```bash
   git format-patch <tag> -o /path/to/Daytona/patches/labwc/
   ```

3. Rebuild (`cmake --build build`; the script notices the change) and run the
   tests, including `smoke_frame`.

When a labwc release includes one of these changes and Daytona can use it,
delete the patch.

## Licence

These patches modify labwc and are GPL-2.0-only, like labwc.
