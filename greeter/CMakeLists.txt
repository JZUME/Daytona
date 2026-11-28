# SPDX-License-Identifier: GPL-3.0-or-later

# daytona-greeter: the optional log on screen, a greetd greeter, packaged on
# its own (daytona-greeter). See docs/greeter.md.

find_package(Qt6 6.8 REQUIRED COMPONENTS DBus Network)
find_package(LayerShellQt 6.3 REQUIRED)

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
    sessions.cpp
    sessions.h
)
target_include_directories(daytona-greeter-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(daytona-greeter-core PUBLIC Qt6::Widgets Qt6::DBus Qt6::Network daytona-style)

add_executable(daytona-logon main.cpp)
target_link_libraries(daytona-logon PRIVATE daytona-greeter-core LayerShellQt::Interface)
install(TARGETS daytona-logon RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/daytona)

# The command greetd runs, and the compositor configuration it uses.
set(DAYTONA_GREETER_LABWC_DIR "${CMAKE_INSTALL_FULL_DATADIR}/daytona-greeter/labwc")
set(DAYTONA_LOGON "${CMAKE_INSTALL_FULL_LIBEXECDIR}/daytona/daytona-logon")
set(DAYTONA_GREETER_LABWC "${CMAKE_INSTALL_FULL_LIBEXECDIR}/daytona/labwc")
configure_file(daytona-greeter.in daytona-greeter @ONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/daytona-greeter DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES labwc/rc.xml labwc/menu.xml labwc/autostart
    DESTINATION ${CMAKE_INSTALL_DATADIR}/daytona-greeter/labwc)
