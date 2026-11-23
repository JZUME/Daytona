// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Daytona {

// How the taskbar's window buttons fit their space, as NT does it: buttons
// share the width equally, up to a maximum; once they would be narrower than
// a minimum, the buttons are shown a page at a time with up/down arrows.
struct TaskLayout
{
    int buttonWidth = 0;
    int perPage = 0; // buttons shown at once
    bool paged = false; // arrows needed

    bool operator==(const TaskLayout &o) const
    {
        return buttonWidth == o.buttonWidth && perPage == o.perPage && paged == o.paged;
    }
};

struct TaskLayoutMetrics
{
    int maxWidth = 160; // NT's widest taskbar button
    int minWidth = 40; // room for the icon and a few characters
    int spacing = 3; // between buttons
    int arrowsWidth = 16; // the up/down page arrows
};

TaskLayout computeTaskLayout(int count, int available, const TaskLayoutMetrics &metrics = {});

} // namespace Daytona
