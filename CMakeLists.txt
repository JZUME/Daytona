# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.25)

project(Daytona VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DAYTONA_WERROR "Treat compiler warnings as errors" OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Test)
qt_standard_project_setup()

include(GNUInstallDirs)

add_compile_options(-Wall -Wextra)
if(DAYTONA_WERROR)
    add_compile_options(-Werror)
endif()

add_compile_definitions(
    DAYTONA_VERSION="${PROJECT_VERSION}"
    QT_NO_KEYWORDS
)

set(DAYTONA_LABWC_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/daytona/labwc")

add_subdirectory(style)
add_subdirectory(shell)
add_subdirectory(locker)
add_subdirectory(themetool)
add_subdirectory(labwc)
add_subdirectory(session)
add_subdirectory(xdg)
add_subdirectory(third_party)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
