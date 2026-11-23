# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent DBus WaylandClient)
find_package(LayerShellQt 6.3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client)

# Shell logic lives in a static library so tests can link it.
add_library(daytona-shell-core STATIC
    applicationindex.cpp
    applicationindex.h
    balloon.cpp
    balloon.h
    clickcatcher.cpp
    clickcatcher.h
    dbusmenu.cpp
    dbusmenu.h
    desktop.cpp
    desktop.h
    displayproperties.cpp
    displayproperties.h
    statusnotifierhost.cpp
    statusnotifierhost.h
    statusnotifieritem.cpp
    statusnotifieritem.h
    statusnotifierwatcher.cpp
    statusnotifierwatcher.h
    clock.cpp
    clock.h
    desktopentry.cpp
    desktopentry.h
    notificationarea.cpp
    notificationarea.h
    notificationqueue.cpp
    notificationqueue.h
    notificationservice.cpp
    notificationservice.h
    recentdocuments.cpp
    recentdocuments.h
    rundialog.cpp
    rundialog.h
    settings.cpp
    settings.h
    shutdowndialog.cpp
    shutdowndialog.h
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
    trayicon.cpp
    trayicon.h
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
)
# The binary dir holds the generated Wayland protocol headers.
target_include_directories(daytona-shell-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(daytona-shell-core PUBLIC
    Qt6::Widgets Qt6::Concurrent Qt6::DBus Qt6::WaylandClient LayerShellQt::Interface
    daytona-style daytona-theme-core)
qt_generate_wayland_protocol_client_sources(daytona-shell-core
    FILES ${PROJECT_SOURCE_DIR}/protocols/wlr-foreign-toplevel-management-unstable-v1.xml)

# The logo is SVG; Qt loads it through the SVG icon engine plugin
# (qt6-svg-plugins) at run time, so no extra library is linked.
qt_add_resources(daytona-shell-core artwork
    PREFIX /daytona
    BASE ${PROJECT_SOURCE_DIR}/artwork
    FILES ${PROJECT_SOURCE_DIR}/artwork/start-logo.svg)

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
