// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Daytona {

struct Toplevel;

// Which items of the NT window menu apply to a window, and which one is the
// default (bold) item.
struct WindowMenuState
{
    enum class Item
    {
        Restore,
        Move,
        Size,
        Minimize,
        Maximize,
        Close
    };

    bool restore = false;
    bool move = false;
    bool size = false;
    bool minimize = false;
    bool maximize = false;
    bool close = true;
    Item defaultItem = Item::Close;
};

WindowMenuState windowMenuState(const Toplevel &toplevel);

} // namespace Daytona
