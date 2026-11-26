# SPDX-License-Identifier: GPL-3.0-or-later

# Chicago95 is a git submodule. Its files are installed from the submodule and
# never copied into Daytona's own tree.
set(CHICAGO95_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Chicago95")

if(NOT EXISTS "${CHICAGO95_DIR}/Theme/Chicago95/index.theme")
    message(FATAL_ERROR
        "The Chicago95 submodule is missing. Run:\n"
        "  git submodule update --init\n"
        "or clone with --recurse-submodules.")
endif()

install(DIRECTORY "${CHICAGO95_DIR}/Theme/Chicago95"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/themes")

install(DIRECTORY "${CHICAGO95_DIR}/Icons/Chicago95"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons")

# "Standard Cursors", the Windows 95 cursor set Chicago95 recommends, is not
# installed as it is: themetool/ generates Daytona's cursor theme ("Daytona
# Cursors") from it at every size, without the cursors that show a parody
# of the Windows logo.
