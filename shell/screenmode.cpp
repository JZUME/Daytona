// SPDX-License-Identifier: GPL-3.0-or-later

#include "screenmode.h"

#include <QRegularExpression>

namespace Daytona {

QString modeKey(const ScreenMode &mode)
{
    QString key = QStringLiteral("%1x%2").arg(mode.size.width()).arg(mode.size.height());
    if (mode.refresh > 0)
        key += QStringLiteral("@%1").arg(mode.refresh);
    return key;
}

ScreenMode parseModeKey(const QString &text)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^(\d{1,5})x(\d{1,5})(?:@(\d{1,7}))?$)"));
    const QRegularExpressionMatch match = pattern.match(text.trimmed());
    ScreenMode mode;
    if (!match.hasMatch())
        return mode;
    mode.size = QSize(match.captured(1).toInt(), match.captured(2).toInt());
    mode.refresh = match.captured(3).toInt();
    if (!mode.isValid())
        return {};
    return mode;
}

ScreenMode matchMode(const QList<ScreenMode> &modes, const ScreenMode &wanted)
{
    ScreenMode best;
    for (const ScreenMode &mode : modes) {
        if (mode.size != wanted.size)
            continue;
        if (wanted.refresh > 0 && mode.refresh == wanted.refresh)
            return mode;
        // Otherwise the preferred mode of that size, else its fastest.
        if (!best.isValid() || (mode.preferred && !best.preferred)
            || (mode.preferred == best.preferred && mode.refresh > best.refresh))
            best = mode;
    }
    return best;
}

} // namespace Daytona
