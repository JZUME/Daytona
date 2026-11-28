# SPDX-License-Identifier: GPL-3.0-or-later

# daytona-greeter: the optional log on screen, a greetd greeter, packaged on
# its own (daytona-greeter). See docs/greeter.md.

find_package(Qt6 6.8 REQUIRED COMPONENTS DBus Network WaylandClient)
find_package(LayerShellQt 6.3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GreeterWaylandClient REQUIRED IMPORTED_TARGET wayland-client)

# Everything but main(), in a library so tests can link it.
add_library(daytona-greeter-core STATIC
    greetd.cpp
    greetd.h
    greeter.cpp
    greeter.h
    keyboard.cpp
    keyboard.h
    logonui.cpp
    logonui.h
    scale.cpp
    scale.h
    sessions.cpp
    sessions.h
)
target_include_directories(daytona-greeter-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(daytona-greeter-core PUBLIC Qt6::Widgets Qt6::DBus Qt6::Network daytona-style)

# Setting the screens' scale (--scale) needs a compositor, so it is not in
# the library the tests use.
add_executable(daytona-logon main.cpp outputscale.cpp outputscale.h)
qt_generate_wayland_protocol_client_sources(daytona-logon
    FILES ${PROJECT_SOURCE_DIR}/protocols/wlr-output-management-unstable-v1.xml)
target_include_directories(daytona-logon PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(daytona-logon PRIVATE
    daytona-greeter-core LayerShellQt::Interface Qt6::WaylandClient
    PkgConfig::GreeterWaylandClient)
install(TARGETS daytona-logon RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/daytona)

# The command greetd runs, and the compositor configuration it uses.
set(DAYTONA_GREETER_LABWC_DIR "${CMAKE_INSTALL_FULL_DATADIR}/daytona-greeter/labwc")
set(DAYTONA_LOGON "${CMAKE_INSTALL_FULL_LIBEXECDIR}/daytona/daytona-logon")
set(DAYTONA_GREETER_LABWC "${CMAKE_INSTALL_FULL_LIBEXECDIR}/daytona/labwc")
configure_file(daytona-greeter.in daytona-greeter @ONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/daytona-greeter DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES labwc/rc.xml labwc/menu.xml labwc/autostart
    DESTINATION ${CMAKE_INSTALL_DATADIR}/daytona-greeter/labwc)
