# SPDX-License-Identifier: GPL-3.0-or-later
set(DAYTONA_LABWC_DIR "${CMAKE_INSTALL_FULL_DATADIR}/daytona/labwc")
configure_file(daytona-session.in daytona-session @ONLY)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/daytona-session
    DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES daytona.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions)

# The Daytona GTK 3 theme, which applies Chicago95 (see gtk-3.0/gtk.css).
install(FILES gtk-3.0/gtk.css
    DESTINATION ${CMAKE_INSTALL_DATADIR}/themes/Daytona/gtk-3.0)
