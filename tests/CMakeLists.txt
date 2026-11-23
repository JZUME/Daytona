# SPDX-License-Identifier: GPL-3.0-or-later
function(daytona_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

daytona_add_test(tst_sanity Qt6::Core)
daytona_add_test(tst_settings daytona-shell-core)
daytona_add_test(tst_toplevellist daytona-shell-core)
daytona_add_test(tst_tasklayout daytona-shell-core)
daytona_add_test(tst_windowmenu daytona-shell-core)
daytona_add_test(tst_desktopentry daytona-shell-core)
daytona_add_test(tst_startmenu daytona-shell-core)
daytona_add_test(tst_themefile daytona-theme-core)
daytona_add_test(tst_recentdocuments daytona-shell-core)
daytona_add_test(tst_rundialog daytona-shell-core)
daytona_add_test(tst_shutdowndialog daytona-shell-core)
set_tests_properties(tst_shutdowndialog PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
set_tests_properties(tst_rundialog PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
set_tests_properties(tst_recentdocuments PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# The inspect command on the sample theme.
add_test(NAME themetool_inspect
    COMMAND daytona-themetool inspect ${CMAKE_CURRENT_SOURCE_DIR}/data/themes/Sample.theme)
set_tests_properties(themetool_inspect PROPERTIES
    PASS_REGULAR_EXPRESSION "ActiveTitle +128   0   0.*Wallpaper: [^\n]*Sample Wallpaper.bmp \\(tiled\\)")
set_tests_properties(tst_startmenu PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
daytona_add_test(tst_bevel daytona-style)
set_tests_properties(tst_bevel PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
daytona_add_test(tst_clock daytona-shell-core)
set_tests_properties(tst_clock PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
daytona_add_test(tst_startbutton daytona-shell-core)
set_tests_properties(tst_startbutton PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
daytona_add_test(tst_styleplugin Qt6::Widgets)
add_dependencies(tst_styleplugin daytona-style-plugin)
set_tests_properties(tst_styleplugin PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins;QT_STYLE_OVERRIDE=daytona;XDG_CONFIG_HOME=${CMAKE_CURRENT_SOURCE_DIR}/data/config")
daytona_add_test(tst_colours daytona-style)
set_tests_properties(tst_colours PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# Outside a Wayland session with layer-shell, the shell must refuse to start
# rather than show the taskbar as an ordinary window.
add_test(NAME shell_requires_layer_shell COMMAND daytona-shell)
set_tests_properties(shell_requires_layer_shell PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DBUS_SESSION_BUS_ADDRESS=disabled:"
    WILL_FAIL TRUE)

add_subdirectory(style-preview)
add_subdirectory(smoke)

# The apply command writes colors.ini and records the scheme name.
add_test(NAME themetool_apply
    COMMAND daytona-themetool apply --scheme "High Contrast Black")
set_tests_properties(themetool_apply PROPERTIES
    ENVIRONMENT "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/themetool-apply;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/themetool-apply/data;LABWC_PID="
    PASS_REGULAR_EXPRESSION "Applied colour scheme 'High Contrast Black' to [^\n]*/themetool-apply/daytona/colors.ini")
daytona_add_test(tst_labwctheme daytona-theme-core)
daytona_add_test(tst_gtktheme daytona-theme-core)

# Applying a .theme file: the wallpaper is copied and recorded, and what
# Daytona does not support is listed.
add_test(NAME themetool_apply_theme
    COMMAND daytona-themetool apply ${CMAKE_CURRENT_SOURCE_DIR}/data/themes/Sample.theme)
set_tests_properties(themetool_apply_theme PROPERTIES
    ENVIRONMENT "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/themetool-apply-theme;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/themetool-apply-theme/data;LABWC_PID="
    PASS_REGULAR_EXPRESSION "Wallpaper: [^\n]*/themetool-apply-theme/data/daytona/wallpaper/Sample Wallpaper.bmp \\(tiled\\).*Warning: ")
daytona_add_test(tst_desktop daytona-shell-core)
set_tests_properties(tst_desktop PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# Tray icons: a host and test items talking over a private session bus.
add_subdirectory(sni)
add_executable(tst_statusnotifier tst_statusnotifier.cpp sni/testitem.h sni/testmenu.h)
target_include_directories(tst_statusnotifier PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_statusnotifier PRIVATE Qt6::Test daytona-shell-core)
add_test(NAME tst_statusnotifier COMMAND dbus-run-session -- $<TARGET_FILE:tst_statusnotifier>)
set_tests_properties(tst_statusnotifier PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# Notification balloons: queue logic, balloon layout and the D-Bus service.
add_executable(tst_notifications tst_notifications.cpp)
target_link_libraries(tst_notifications PRIVATE Qt6::Test daytona-shell-core)
add_test(NAME tst_notifications COMMAND dbus-run-session -- $<TARGET_FILE:tst_notifications>)
set_tests_properties(tst_notifications PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
daytona_add_test(tst_taskbarplacement daytona-shell-core)
set_tests_properties(tst_taskbarplacement PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
daytona_add_test(tst_displayproperties daytona-shell-core)
target_compile_definitions(tst_displayproperties PRIVATE
    DAYTONA_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data"
    DAYTONA_THEMETOOL="$<TARGET_FILE:daytona-themetool>")
add_dependencies(tst_displayproperties daytona-themetool)
set_tests_properties(tst_displayproperties PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
