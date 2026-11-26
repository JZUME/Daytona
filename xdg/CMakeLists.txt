# SPDX-License-Identifier: GPL-3.0-or-later
# Daytona's versions of some applications' XDG autostart entries; the labwc
# autostart file puts this directory first for dex.
install(FILES autostart/nm-applet.desktop autostart/nm-tray-autostart.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/daytona/xdg/autostart)

# The portal backends for the session (XDG_CURRENT_DESKTOP=Daytona).
install(FILES xdg-desktop-portal/daytona-portals.conf
    DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal)

# xdg-desktop-portal-wlr's settings for the Daytona session: the Share Screen
# dialog. It looks only in XDG_CONFIG_HOME and in /etc/xdg (its own
# sysconfdir), not in data directories, so this only takes effect when
# sysconfdir is /etc, as for the Debian package.
configure_file(xdg-desktop-portal-wlr/Daytona.in xdg-desktop-portal-wlr/Daytona @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xdg-desktop-portal-wlr/Daytona
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/xdg-desktop-portal-wlr)
