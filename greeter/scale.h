// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace Daytona {

// Parses the log on screen's scale (daytona-greeter --scale): a size from 1
// (100%) to 3 (300%), such as 1.5 or 2, or a percentage such as 150%. 0 if
// it is not one.
double parseScale(const QString &value);

} // namespace Daytona
