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

# The theme and icon theme are staged in the build tree without the Windows
# logo and without icons that replace an application's own icon
# (scripts/stage-chicago95.py), and installed from there.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CHICAGO95_STAGE "${CMAKE_BINARY_DIR}/chicago95")
set(CHICAGO95_STAGE_SCRIPT "${PROJECT_SOURCE_DIR}/scripts/stage-chicago95.py")
add_custom_command(
    OUTPUT "${CHICAGO95_STAGE}/stamp"
    COMMAND Python3::Interpreter -I "${CHICAGO95_STAGE_SCRIPT}"
        "${CHICAGO95_DIR}" "${CHICAGO95_STAGE}"
    COMMAND ${CMAKE_COMMAND} -E touch "${CHICAGO95_STAGE}/stamp"
    DEPENDS "${CHICAGO95_STAGE_SCRIPT}"
        "${CHICAGO95_DIR}/Theme/Chicago95/index.theme"
        "${CHICAGO95_DIR}/Icons/Chicago95/index.theme"
    COMMENT "Staging the Chicago95 theme and icons"
    VERBATIM)
add_custom_target(daytona-chicago95 ALL DEPENDS "${CHICAGO95_STAGE}/stamp")

install(DIRECTORY "${CHICAGO95_STAGE}/themes/Chicago95"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/themes")

install(DIRECTORY "${CHICAGO95_STAGE}/icons/Chicago95"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons")

# "Standard Cursors", the Windows 95 cursor set Chicago95 recommends, is not
# installed as it is: themetool/ generates Daytona's cursor theme ("Daytona
# Cursors") from it at every size, without the cursors that show a parody
# of the Windows logo.
