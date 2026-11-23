// SPDX-License-Identifier: GPL-3.0-or-later

#include "tasklayout.h"

#include <algorithm>

namespace Daytona {

TaskLayout computeTaskLayout(int count, int available, const TaskLayoutMetrics &m)
{
    if (count <= 0 || available <= 0)
        return {};

    const int sharedWidth = (available - m.spacing * (count - 1)) / count;
    if (sharedWidth >= m.minWidth)
        return { std::min(sharedWidth, m.maxWidth), count, false };

    // Not enough room: page through the buttons, leaving space for arrows.
    const int forButtons = available - m.arrowsWidth - m.spacing;
    const int perPage = std::max(1, (forButtons + m.spacing) / (m.minWidth + m.spacing));
    const int width = std::max(1, (forButtons - m.spacing * (perPage - 1)) / perPage);
    return { std::min(width, m.maxWidth), perPage, true };
}

} // namespace Daytona
