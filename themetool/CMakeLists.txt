# SPDX-License-Identifier: GPL-3.0-or-later

# Theme parsing and conversion, in a library so tests can link it.
add_library(daytona-theme-core STATIC
    colourscheme.cpp
    colourscheme.h
    gtktheme.cpp
    gtktheme.h
    labwctheme.cpp
    labwctheme.h
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
