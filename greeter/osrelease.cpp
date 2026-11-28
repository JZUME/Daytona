// SPDX-License-Identifier: GPL-3.0-or-later

#include "osrelease.h"

#include <QFile>

namespace Daytona {

QString operatingSystemName(const QString &root)
{
    for (const char *path : { "/etc/os-release", "/usr/lib/os-release" }) {
        QFile file(root + QLatin1String(path));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (!line.startsWith(QLatin1String("NAME=")))
                continue;
            QString value = line.mid(5).trimmed();
            if (value.size() >= 2
                && (value.front() == QLatin1Char('"') || value.front() == QLatin1Char('\''))
                && value.back() == value.front()) {
                value = value.mid(1, value.size() - 2);
                // Shell escapes in double quotes.
                for (const QChar c :
                    { QLatin1Char('\\'), QLatin1Char('$'), QLatin1Char('"'), QLatin1Char('`') })
                    value.replace(QStringLiteral("\\") + c, QString(c));
            }
            value = value.trimmed();
            if (!value.isEmpty())
                return value;
        }
        // The first file found is the one that counts, NAME or not.
        break;
    }
    return QStringLiteral("Linux");
}

} // namespace Daytona
