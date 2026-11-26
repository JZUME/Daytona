# SPDX-License-Identifier: GPL-3.0-or-later

# Theme parsing and conversion, in a library so tests can link it.
add_library(daytona-theme-core STATIC
    colourscheme.cpp
    colourscheme.h
    gtktheme.cpp
    gtktheme.h
    labwctheme.cpp
    labwctheme.h
    xcursor.cpp
    xcursor.h
)
# Daytona's own labwc theme is the template for generated ones.
qt_add_resources(daytona-theme-core labwc-theme-template
    PREFIX /labwc-theme
    BASE ${PROJECT_SOURCE_DIR}/labwc/themes/Daytona/openbox-3
    FILES
        ${PROJECT_SOURCE_DIR}/labwc/themes/Daytona/openbox-3/themerc
        ${PROJECT_SOURCE_DIR}/labwc/themes/Daytona/openbox-3/close.svg
        ${PROJECT_SOURCE_DIR}/labwc/themes/Daytona/openbox-3/iconify.svg
        ${PROJECT_SOURCE_DIR}/labwc/themes/Daytona/openbox-3/max.svg
        ${PROJECT_SOURCE_DIR}/labwc/themes/Daytona/openbox-3/max_toggled.svg
)
target_include_directories(daytona-theme-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(daytona-theme-core PUBLIC daytona-colours)

add_executable(daytona-themetool
    main.cpp
)

target_link_libraries(daytona-themetool PRIVATE daytona-theme-core)

install(TARGETS daytona-themetool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Daytona's cursor theme: Chicago95's cursors at every pointer scheme and
# screen scale (see xcursor.h), generated at build time from the submodule.
add_executable(daytona-cursorgen cursorgen.cpp)
target_link_libraries(daytona-cursorgen PRIVATE daytona-theme-core)

set(DAYTONA_CURSOR_SOURCE
    "${PROJECT_SOURCE_DIR}/third_party/Chicago95/Cursors/Chicago95 Standard Cursors/cursors")
set(DAYTONA_CURSOR_THEME "${CMAKE_BINARY_DIR}/icons/Daytona Cursors")
file(GLOB DAYTONA_CURSOR_FILES CONFIGURE_DEPENDS "${DAYTONA_CURSOR_SOURCE}/*")
add_custom_command(
    OUTPUT "${DAYTONA_CURSOR_THEME}/index.theme"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DAYTONA_CURSOR_THEME}"
    COMMAND daytona-cursorgen "${DAYTONA_CURSOR_SOURCE}" "${DAYTONA_CURSOR_THEME}"
    DEPENDS daytona-cursorgen ${DAYTONA_CURSOR_FILES}
    COMMENT "Generating the Daytona Cursors theme"
    VERBATIM)
add_custom_target(daytona-cursor-theme ALL DEPENDS "${DAYTONA_CURSOR_THEME}/index.theme")
install(DIRECTORY "${DAYTONA_CURSOR_THEME}" DESTINATION "${CMAKE_INSTALL_DATADIR}/icons")
