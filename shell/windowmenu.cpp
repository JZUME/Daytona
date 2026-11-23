// SPDX-License-Identifier: GPL-3.0-or-later

#include "windowmenu.h"

#include "toplevellist.h"

namespace Daytona {

WindowMenuState windowMenuState(const Toplevel &toplevel)
{
    WindowMenuState state;
    // As in NT: Restore applies to minimised or maximised windows, Minimise
    // to windows that are not minimised, Maximise to windows that are not
    // maximised (a minimised, maximised window can still be restored).
    state.restore = toplevel.minimized || toplevel.maximized;
    state.minimize = !toplevel.minimized;
    state.maximize = !toplevel.maximized || toplevel.minimized;
    // Move and Size are never available from the taskbar:
    // wlr-foreign-toplevel-management has no request to start an interactive
    // move or resize. They remain in the titlebar menu (Alt+Space), which
    // labwc draws. Revisit if the protocol gains such requests.
    state.move = false;
    state.size = false;
    state.close = true;
    // NT's default item is Restore for a minimised window, otherwise Close.
    state.defaultItem
        = toplevel.minimized ? WindowMenuState::Item::Restore : WindowMenuState::Item::Close;
    return state;
}

} // namespace Daytona
