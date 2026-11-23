# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(daytona-sni-test-item main.cpp testitem.h testmenu.h)
target_link_libraries(daytona-sni-test-item PRIVATE Qt6::Gui Qt6::DBus)
