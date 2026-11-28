// SPDX-License-Identifier: GPL-3.0-or-later

#include "scale.h"

#include <cmath>

namespace Daytona {

double parseScale(const QString &value)
{
    QString text = value.trimmed();
    bool percent = false;
    if (text.endsWith(QLatin1Char('%'))) {
        percent = true;
        text.chop(1);
    }
    bool ok = false;
    double scale = text.toDouble(&ok);
    if (percent)
        scale /= 100.0;
    if (!ok || !std::isfinite(scale) || scale < 1.0 || scale > 3.0)
        return 0;
    return scale;
}

} // namespace Daytona
