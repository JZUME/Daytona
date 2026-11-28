# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Xcb REQUIRED IMPORTED_TARGET xcb xcb-composite xcb-damage)

add_executable(daytona-xembed-proxy
    main.cpp
    xembedproxy.cpp
    xembedproxy.h
)
target_link_libraries(daytona-xembed-proxy PRIVATE Qt6::Core Qt6::DBus PkgConfig::Xcb)

# Run by the session, not by users.
install(TARGETS daytona-xembed-proxy RUNTIME DESTINATION libexec/daytona)
