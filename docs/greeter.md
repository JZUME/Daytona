# Log on screen (daytona-greeter)

`daytona-greeter` is an optional graphical log on screen that matches the Daytona desktop. It is
its own package: `daytona` only suggests it, so installing Daytona never changes your login
manager, and you can keep GDM, SDDM, LightDM or any other.

It is a Windows 98-style dialog on the desktop colour, welcoming you to the operating system by
its name (os-release's `NAME`, such as "Welcome to Debian GNU/Linux", as Windows said "Welcome to
Windows"): **User name**, **Password**, the session
to start with a **Session...** button to choose another, **OK**, **Cancel** (which clears both
fields) and **Shut Down...**. It remembers the last user name and session, puts the cursor in
Password, and uses the computer's keyboard layout, so a password is typed the same way as on the
desktop.

## How it works

`daytona-greeter` is a greeter for [greetd](https://sr.ht/~kennylevinsen/greetd/), a small login
manager. greetd does the privileged work (PAM, the logind session, starting the desktop as the
user); the log on screen runs as greetd's unprivileged `_greetd` user and only talks to greetd
over a socket.

- `daytona-greeter` (the command greetd runs) starts Daytona's labwc with a configuration of its
  own (`/usr/share/daytona-greeter/labwc`: no menus, no key bindings, dialogs centred) and the
  keyboard layout from systemd-localed (`localectl set-x11-keymap`; on Debian this is
  `/etc/default/keyboard`).
- labwc runs the dialog, `/usr/libexec/daytona/daytona-logon`. When greetd has the session to
  start, the dialog and labwc exit and greetd starts the session.
- The session list is every Wayland session installed in `wayland-sessions/` under
  `/usr/share` and `/usr/local/share` (the `XDG_DATA_DIRS`), as other display managers show them.
  Daytona is chosen by default, then whatever was used last.
- Whatever PAM asks or says is shown: a one-time code or PIN in a prompt of its own labelled with
  PAM's text, and messages such as "Your account has expired" in a message box. A wrong password
  shows "The system could not log you on", keeps the user name and clears the password.
- **Shut Down...** offers Shut down, Restart and (where the computer can) Stand by, through
  systemd-logind.
- The last user name and session are kept in `/var/lib/daytona-greeter/state`. Passwords are never
  stored.

X11-only sessions (`xsessions/`) are not listed: greetd does not start an X server itself.

**Expired passwords:** greetd 0.10 (Debian 13) does not let PAM change an expired password at log
on, so the log on screen says the password has expired and must be changed. Change it at a text
console (Ctrl+Alt+F3, log in, follow the prompts) and log on again.

## Installing

With the Daytona apt repository set up (see [README.md](../README.md#installing-on-debian-13)):

```bash
sudo apt install daytona-greeter
```

This installs greetd too, but changes nothing yet: your current login manager stays in charge
until you switch.

### Switching from your current login manager

1. Point greetd at the log on screen. In `/etc/greetd/config.toml`, set the `[default_session]`
   command (the package leaves this file to greetd, so you edit it once):

   ```toml
   [terminal]
   vt = 7

   [default_session]
   command = "daytona-greeter"
   user = "_greetd"
   ```

   The same file is in `/usr/share/doc/daytona-greeter/examples/config.toml`, so you can also
   copy it: `sudo cp /usr/share/doc/daytona-greeter/examples/config.toml /etc/greetd/config.toml`.

2. Make greetd the display manager. Find the current one, disable it and enable greetd:

   ```bash
   systemctl status display-manager      # shows gdm3, sddm, lightdm, ...
   sudo systemctl disable gdm3           # or sddm, lightdm: whichever was shown
   sudo systemctl enable --force greetd
   ```

3. Restart the computer: `sudo reboot`.

The log on screen then appears at start-up and after logging off.

If the screen stays black or greetd keeps restarting, switch to a text console (Ctrl+Alt+F3),
log in and look at `journalctl -b -u greetd`.

### Going back

```bash
sudo systemctl disable greetd
sudo systemctl enable --force gdm3       # or sddm, lightdm
sudo reboot
```

To remove the log on screen as well: `sudo apt purge daytona-greeter` (and `greetd`, if nothing
else uses it).

### Other settings

- **High-density (HiDPI) screens:** add `--scale` with the size, from 1 (100%) to 3 (300%), to
  the command in `/etc/greetd/config.toml`:

  ```toml
  [default_session]
  command = "daytona-greeter --scale 2"
  user = "_greetd"
  ```

  Fractional sizes such as `--scale 1.5` (or `150%`) work too. Every screen, including one plugged
  in while the log on screen is showing, gets that scale; labwc draws the window frames and the
  mouse pointer at the same size. Without `--scale` the screens keep the compositor's default
  (100%). The desktop's own Font Size setting (Display Properties) is per user and applies after
  log on.

- **Automatic log on:** greetd's own `[initial_session]` in `/etc/greetd/config.toml` logs a user
  on at start-up without the log on screen (see `man 5 greetd`).
- **Keyboard layout:** `sudo localectl set-x11-keymap gb` (or `sudo dpkg-reconfigure
  keyboard-configuration`) sets the layout the log on screen uses.
