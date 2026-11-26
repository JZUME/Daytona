# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent WaylandClient)
# Qt 6.9 and later ship private modules as packages of their own; with 6.8
# the target comes with WaylandClient.
find_package(Qt6 6.8 QUIET COMPONENTS WaylandClientPrivate)
if(NOT TARGET Qt6::WaylandClientPrivate)
    message(FATAL_ERROR "Qt's private Wayland client headers are needed (qt6-wayland-private-dev)")
endif()
find_package(PkgConfig REQUIRED)
pkg_check_modules(WaylandProtocols REQUIRED wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
find_path(PAM_INCLUDE_DIR security/pam_appl.h REQUIRED)
find_library(PAM_LIBRARY pam REQUIRED)

# The lock screen's dialogs, in a library so tests can link them.
add_library(daytona-lock-core STATIC
    lockui.cpp
    lockui.h
)
target_include_directories(daytona-lock-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(daytona-lock-core PUBLIC Qt6::Widgets daytona-style)

add_executable(daytona-lock
    authenticator.cpp
    authenticator.h
    main.cpp
    sessionlock.cpp
    sessionlock.h
)
qt_generate_wayland_protocol_client_sources(daytona-lock
    FILES ${WAYLAND_PROTOCOLS_DIR}/staging/ext-session-lock/ext-session-lock-v1.xml)
target_include_directories(daytona-lock PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${PAM_INCLUDE_DIR})
target_link_libraries(daytona-lock PRIVATE
    daytona-lock-core
    Qt6::Concurrent
    Qt6::WaylandClient
    Qt6::WaylandClientPrivate
    ${PAM_LIBRARY}
)

# Run by the shell, not by users; next to labwc, where the shell looks.
install(TARGETS daytona-lock RUNTIME DESTINATION libexec/daytona)
