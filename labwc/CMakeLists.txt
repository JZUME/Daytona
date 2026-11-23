# SPDX-License-Identifier: GPL-3.0-or-later
install(FILES rc.xml menu.xml autostart
    DESTINATION ${DAYTONA_LABWC_INSTALL_DIR})

# labwc looks for themes in the data directories' themes/ folders.
set(DAYTONA_THEME_DIR "${CMAKE_INSTALL_DATADIR}/themes/Daytona/openbox-3")
install(FILES themes/Daytona/openbox-3/themerc DESTINATION ${DAYTONA_THEME_DIR})

# labwc 0.8.3 needs separate images for active/inactive windows and for the
# hover state. NT caption buttons look the same in all of these (and have no
# hover effect), so each source image is installed under all four names.
foreach(button iconify max max_toggled close)
    foreach(variant "" "_hover")
        foreach(state active inactive)
            install(FILES themes/Daytona/openbox-3/${button}.svg
                DESTINATION ${DAYTONA_THEME_DIR}
                RENAME ${button}${variant}-${state}.svg)
        endforeach()
    endforeach()
endforeach()
