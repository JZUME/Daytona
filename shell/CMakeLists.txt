# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent DBus WaylandClient Xml)
find_package(LayerShellQt 6.3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client)
# The notification area's volume control (PulseAudio or pipewire-pulse).
pkg_check_modules(PulseAudio REQUIRED IMPORTED_TARGET libpulse libpulse-mainloop-glib)
# Event sounds through the freedesktop sound theme.
pkg_check_modules(Canberra REQUIRED IMPORTED_TARGET libcanberra)

# Shell logic lives in a static library so tests can link it.
add_library(daytona-shell-core STATIC
    applicationindex.cpp
    applicationindex.h
    balloon.cpp
    balloon.h
    battery.cpp
    battery.h
    batteryicon.cpp
    batteryicon.h
    blankscreen.cpp
    blankscreen.h
    clickcatcher.cpp
    clickcatcher.h
    dbusmenu.cpp
    dbusmenu.h
    datetimeproperties.cpp
    datetimeproperties.h
    desktop.cpp
    desktop.h
    desktopfiles.cpp
    desktopfiles.h
    desktopicons.cpp
    desktopicons.h
    displayproperties.cpp
    displayproperties.h
    endsession.cpp
    endsession.h
    eventsounds.cpp
    eventsounds.h
    inputlocaleicon.cpp
    inputlocaleicon.h
    inputlocales.cpp
    inputlocales.h
    keyboardlayout.cpp
    keyboardlayout.h
    keyboardproperties.cpp
    keyboardproperties.h
    statusnotifierhost.cpp
    statusnotifierhost.h
    statusnotifieritem.cpp
    statusnotifieritem.h
    statusnotifierwatcher.cpp
    statusnotifierwatcher.h
    clock.cpp
    clock.h
    controlpanel.cpp
    controlpanel.h
    desktopentry.cpp
    desktopentry.h
    notificationarea.cpp
    notificationarea.h
    notificationqueue.cpp
    notificationqueue.h
    monitorlayout.cpp
    monitorlayout.h
    mouseproperties.cpp
    mouseproperties.h
    notificationservice.cpp
    notificationservice.h
    outputmanager.cpp
    outputmanager.h
    printscreen.cpp
    printscreen.h
    recentdocuments.cpp
    recentdocuments.h
    rundialog.cpp
    rundialog.h
    screenchooser.cpp
    screenchooser.h
    screenmode.cpp
    screenmode.h
    screenlock.cpp
    screenlock.h
    screensaverservice.cpp
    screensaverservice.h
    settings.cpp
    settings.h
    shutdowndialog.cpp
    shutdowndialog.h
    soundsproperties.cpp
    soundsproperties.h
    soundtheme.cpp
    soundtheme.h
    systemlogo.cpp
    systemlogo.h
    shellservice.cpp
    shellservice.h
    startbutton.cpp
    startbutton.h
    startmenu.cpp
    startmenu.h
    tasklayout.cpp
    tasklayout.h
    tasklist.cpp
    tasklist.h
    timedate.cpp
    timedate.h
    trayicon.cpp
    trayicon.h
    volumecontrol.cpp
    volumecontrol.h
    volumeicon.cpp
    volumeicon.h
    volumepopup.cpp
    volumepopup.h
    taskbar.cpp
    taskbar.h
    taskbarplacement.cpp
    taskbarplacement.h
    taskbarproperties.cpp
    taskbarproperties.h
    taskbutton.cpp
    taskbutton.h
    toplevellist.cpp
    toplevellist.h
    toplevelmanager.cpp
    toplevelmanager.h
    windowmenu.cpp
    windowmenu.h
    xdgmenu.cpp
    xdgmenu.h
)
# The binary dir holds the generated Wayland protocol headers.
target_include_directories(daytona-shell-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(daytona-shell-core PUBLIC
    Qt6::Widgets Qt6::Concurrent Qt6::DBus Qt6::WaylandClient Qt6::Xml LayerShellQt::Interface
    daytona-style daytona-theme-core)
target_link_libraries(daytona-shell-core PRIVATE PkgConfig::PulseAudio PkgConfig::Canberra PkgConfig::WaylandClient)
qt_generate_wayland_protocol_client_sources(daytona-shell-core
    FILES ${PROJECT_SOURCE_DIR}/protocols/wlr-foreign-toplevel-management-unstable-v1.xml
          ${PROJECT_SOURCE_DIR}/protocols/wlr-output-management-unstable-v1.xml)

# The logo is SVG; Qt loads it through the SVG icon engine plugin
# (qt6-svg-plugins) at run time, so no extra library is linked.
qt_add_resources(daytona-shell-core artwork
    PREFIX /daytona
    BASE ${PROJECT_SOURCE_DIR}/artwork
    FILES ${PROJECT_SOURCE_DIR}/artwork/start-logo.svg)
# Start > Programs when no menu file is installed (see xdgmenu.h).
qt_add_resources(daytona-shell-core menu
    PREFIX /daytona
    BASE ${PROJECT_SOURCE_DIR}/xdg/menus
    FILES ${PROJECT_SOURCE_DIR}/xdg/menus/daytona-applications.menu)

add_executable(daytona-shell
    main.cpp
    waylandcheck.cpp
    waylandcheck.h
)

target_link_libraries(daytona-shell PRIVATE
    daytona-shell-core
    daytona-style
    Qt6::Widgets
    LayerShellQt::Interface
    PkgConfig::WaylandClient
)

install(TARGETS daytona-shell RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
