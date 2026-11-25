# SPDX-License-Identifier: GPL-3.0-or-later

# Smoke tests start a real headless labwc session against an install of this
# build. They need labwc, grim and wlr-randr at run time.
find_program(LABWC_EXECUTABLE labwc)
find_program(GRIM_EXECUTABLE grim)
find_program(WLR_RANDR_EXECUTABLE wlr-randr)
if(NOT LABWC_EXECUTABLE OR NOT GRIM_EXECUTABLE OR NOT WLR_RANDR_EXECUTABLE)
    message(STATUS "labwc, grim or wlr-randr not found: smoke tests disabled")
    return()
endif()

add_executable(daytona-pixelcheck pixelcheck.cpp)
target_link_libraries(daytona-pixelcheck PRIVATE Qt6::Gui)

add_executable(daytona-smoke-window testwindow.cpp)
target_link_libraries(daytona-smoke-window PRIVATE Qt6::Widgets)

set(SMOKE_PREFIX "${CMAKE_BINARY_DIR}/smoke-root")

# Install the build into a private prefix once, before any smoke test.
add_test(NAME smoke_install
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${SMOKE_PREFIX})
set_tests_properties(smoke_install PROPERTIES FIXTURES_SETUP smoke LABELS smoke)

# daytona_add_smoke_test(NAME SCRIPT): run SCRIPT inside a headless session.
# Each also runs at output scale 2 as NAME_2x.
function(daytona_add_smoke_test name script)
    foreach(scale 1 2)
        set(test ${name})
        if(scale GREATER 1)
            set(test ${name}_${scale}x)
        endif()
        add_test(NAME ${test}
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run-session.sh ${SMOKE_PREFIX} 60
                ${CMAKE_CURRENT_SOURCE_DIR}/${script})
        set_tests_properties(${test} PROPERTIES
            FIXTURES_REQUIRED smoke
            LABELS smoke
            TIMEOUT 90
            ENVIRONMENT "PATH=$<TARGET_FILE_DIR:daytona-pixelcheck>:$ENV{PATH};DAYTONA_SMOKE_SCALE=${scale}")
    endforeach()
endfunction()

daytona_add_smoke_test(smoke_window smoke_window.sh)
daytona_add_smoke_test(smoke_taskbar smoke_taskbar.sh)
daytona_add_smoke_test(smoke_taskbuttons smoke_taskbuttons.sh)
daytona_add_smoke_test(smoke_startmenu smoke_startmenu.sh)
# The bevelled frame comes from Daytona's labwc patch, so needs that build.
if(DAYTONA_BUILD_LABWC)
    daytona_add_smoke_test(smoke_frame smoke_frame.sh)
endif()
