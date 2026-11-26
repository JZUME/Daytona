// SPDX-License-Identifier: GPL-3.0-or-later

#include "systemlogo.h"

#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(lcLogo, "daytona.logo", QtInfoMsg)

namespace Daytona {

namespace {

    // Logos distributions install without naming them in os-release. The
    // first file that exists is used, so sharper ones come first.
    const QHash<QString, QStringList> &knownLogos()
    {
        static const QHash<QString, QStringList> logos = {
            { QStringLiteral("debian"),
                {
                    // desktop-base, installed with every Debian desktop: the
                    // red swirl as an SVG. (Its debian-logos/ are white, for
                    // dark login screens, and would vanish on the button.)
                    QStringLiteral(
                        "/usr/share/icons/desktop-base/scalable/emblems/emblem-debian.svg"),
                    // debconf, installed on every Debian system: 48 pixels.
                    QStringLiteral("/usr/share/pixmaps/debian-logo.png"),
                } },
        };
        return logos;
    }

    QString readOsRelease(const QString &root)
    {
        // The os-release(5) search order.
        for (const char *path : { "/etc/os-release", "/usr/lib/os-release" }) {
            QFile file(root + QLatin1String(path));
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
                return QString::fromUtf8(file.read(64 * 1024));
        }
        return {};
    }

    QIcon iconFromFile(const QString &path)
    {
        if (!QFileInfo(path).isFile())
            return {};
        QIcon icon(path);
        // An SVG needs the SVG icon engine; check the file really loads.
        if (icon.pixmap(16).isNull())
            return {};
        return icon;
    }

    QIcon iconByName(const QString &name, const QString &root)
    {
        // Icon names are plain names, not paths.
        static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9._+-]+$"));
        if (!valid.match(name).hasMatch())
            return {};
        if (root.isEmpty() && QIcon::hasThemeIcon(name))
            return QIcon::fromTheme(name);
        for (const char *suffix : { ".svg", ".png", ".xpm" }) {
            QIcon icon = iconFromFile(
                root + QStringLiteral("/usr/share/pixmaps/") + name + QLatin1String(suffix));
            if (!icon.isNull())
                return icon;
        }
        return {};
    }

} // namespace

QHash<QString, QString> parseOsRelease(const QString &text)
{
    QHash<QString, QString> fields;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0)
            continue;
        const QString key = line.left(equals).trimmed();
        QString value = line.mid(equals + 1).trimmed();
        if (value.size() >= 2
            && (value.front() == QLatin1Char('"') || value.front() == QLatin1Char('\''))
            && value.back() == value.front()) {
            value = value.mid(1, value.size() - 2);
            // Shell-style escapes in double quotes.
            static const QRegularExpression escape(QStringLiteral(R"(\\([\\$"`]))"));
            value.replace(escape, QStringLiteral("\\1"));
        }
        fields.insert(key, value);
    }
    return fields;
}

QIcon systemLogo(const QString &root)
{
    const QHash<QString, QString> release = parseOsRelease(readOsRelease(root));
    const QString logo = release.value(QStringLiteral("LOGO"));
    if (!logo.isEmpty()) {
        const QIcon icon = iconByName(logo, root);
        if (!icon.isNull()) {
            qCDebug(lcLogo) << "Using the os-release logo" << logo;
            return icon;
        }
        qCDebug(lcLogo) << "The os-release logo" << logo << "is not installed";
    }
    const QString id = release.value(QStringLiteral("ID"));
    for (const QString &path : knownLogos().value(id)) {
        const QIcon icon = iconFromFile(root + path);
        if (!icon.isNull()) {
            qCDebug(lcLogo) << "Using" << path;
            return icon;
        }
    }
    return {};
}

} // namespace Daytona
