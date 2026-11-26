# SPDX-License-Identifier: GPL-3.0-or-later
install(FILES rc.xml menu.xml autostart shutdown
    DESTINATION ${DAYTONA_LABWC_INSTALL_DIR})

# labwc looks for themes in the data directories' themes/ folders.
set(DAYTONA_THEME_DIR "${CMAKE_INSTALL_DATADIR}/themes/Daytona/openbox-3")
install(FILES themes/Daytona/openbox-3/themerc DESTINATION ${DAYTONA_THEME_DIR})

# labwc 0.8.3 needs separate images for active/inactive windows and for the
# hover state. NT caption buttons look the same in all of these (and have no
# hover effect), so each source image is installed under all four names.
# The pressed (sunken) images are read by Daytona's labwc patch only.
foreach(button iconify max max_toggled close)
    foreach(state active inactive)
        foreach(variant "" "_hover")
            install(FILES themes/Daytona/openbox-3/${button}.svg
                DESTINATION ${DAYTONA_THEME_DIR}
                RENAME ${button}${variant}-${state}.svg)
        endforeach()
        install(FILES themes/Daytona/openbox-3/${button}_pressed.svg
            DESTINATION ${DAYTONA_THEME_DIR}
            RENAME ${button}_pressed-${state}.svg)
    endforeach()
endforeach()

# Daytona's patched labwc (see patches/labwc): built from the third_party/labwc
# submodule by scripts/build-labwc.sh, which leaves the submodule untouched,
# and installed as libexec/daytona/labwc for daytona-session to run. Turn off
# to use the system's labwc instead (window frames are then flat).
option(DAYTONA_BUILD_LABWC "Build labwc with Daytona's patches" ON)
if(DAYTONA_BUILD_LABWC)
    find_program(MESON_EXECUTABLE meson)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DAYTONA_LABWC_WLROOTS wlroots-0.18)
    if(NOT MESON_EXECUTABLE OR NOT DAYTONA_LABWC_WLROOTS_FOUND)
        message(FATAL_ERROR
            "Building Daytona's labwc needs meson and labwc's build dependencies "
            "(libwlroots-0.18-dev and others; see README.md), or configure with "
            "-DDAYTONA_BUILD_LABWC=OFF to use the system's labwc.")
    endif()
    set(DAYTONA_LABWC_BUILD_DIR "${CMAKE_BINARY_DIR}/labwc-build")
    add_custom_target(daytona-labwc ALL
        COMMAND "${PROJECT_SOURCE_DIR}/scripts/build-labwc.sh" "${DAYTONA_LABWC_BUILD_DIR}"
        BYPRODUCTS "${DAYTONA_LABWC_BUILD_DIR}/meson/labwc"
        COMMENT "Building labwc with Daytona's patches"
        USES_TERMINAL
        VERBATIM)
    install(PROGRAMS "${DAYTONA_LABWC_BUILD_DIR}/meson/labwc"
        DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/daytona")
endif()
