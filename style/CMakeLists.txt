# SPDX-License-Identifier: GPL-3.0-or-later

# Colour schemes and the colors.ini file, shared by the style, the shell
# and the theme tool. Needs only Qt GUI.
add_library(daytona-colours STATIC
    colours.cpp
    colours.h
)
set_target_properties(daytona-colours PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(daytona-colours PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(daytona-colours PUBLIC Qt6::Gui)

# The style itself, linked into the shell and wrapped by the plugin below.
add_library(daytona-style STATIC
    bevel.cpp
    bevel.h
    daytonastyle.cpp
    daytonastyle.h
    glyphs.cpp
    glyphs.h
)
set_target_properties(daytona-style PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(daytona-style PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(daytona-style PUBLIC daytona-colours Qt6::Widgets)

# Qt style plugin so other Qt 6 applications can use QT_STYLE_OVERRIDE=daytona.
qt_add_plugin(daytona-style-plugin CLASS_NAME DaytonaStylePlugin plugin.cpp)
# Built into plugins/styles so tests can find it with QT_PLUGIN_PATH.
set_target_properties(daytona-style-plugin PROPERTIES
    OUTPUT_NAME daytonastyle
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/styles)
target_link_libraries(daytona-style-plugin PRIVATE daytona-style)

# Relative to the install prefix: with /usr this is the system Qt plugin
# directory; with another prefix, add <prefix>/<this path> to QT_PLUGIN_PATH.
install(TARGETS daytona-style-plugin
    LIBRARY DESTINATION ${QT6_INSTALL_PLUGINS}/styles)
