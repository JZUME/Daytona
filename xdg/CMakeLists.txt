# SPDX-License-Identifier: GPL-3.0-or-later
# Daytona's versions of some applications' XDG autostart entries; the labwc
# autostart file puts this directory first for dex.
install(FILES autostart/nm-applet.desktop autostart/nm-tray-autostart.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/daytona/xdg/autostart)
