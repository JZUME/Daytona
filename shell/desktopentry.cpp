// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopentry.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>
#include <QTextStream>

namespace Daytona {

namespace {

    // String escapes of the Desktop Entry format: \s \n \t \r \\.
    QString unescapeValue(const QString &value)
    {
        QString out;
        out.reserve(value.size());
        for (int i = 0; i < value.size(); ++i) {
            const QChar c = value[i];
            if (c != QLatin1Char('\\') || i + 1 == value.size()) {
                out += c;
                continue;
            }
            const QChar next = value[++i];
            switch (next.unicode()) {
            case 's':
                out += QLatin1Char(' ');
                break;
            case 'n':
                out += QLatin1Char('\n');
                break;
            case 't':
                out += QLatin1Char('\t');
                break;
            case 'r':
                out += QLatin1Char('\r');
                break;
            case '\\':
                out += QLatin1Char('\\');
                break;
            default:
                // Unknown escapes (including "\;" in lists) are kept for the
                // list splitter.
                out += QLatin1Char('\\');
                out += next;
                break;
            }
        }
        return out;
    }

    // Splits a ';'-separated list, honouring "\;".
    QStringList splitList(const QString &value)
    {
        QStringList items;
        QString current;
        for (int i = 0; i < value.size(); ++i) {
            if (value[i] == QLatin1Char('\\') && i + 1 < value.size()
                && value[i + 1] == QLatin1Char(';')) {
                current += QLatin1Char(';');
                ++i;
            } else if (value[i] == QLatin1Char(';')) {
                items.append(current);
                current.clear();
            } else {
                current += value[i];
            }
        }
        if (!current.isEmpty())
            items.append(current);
        items.removeAll(QString());
        return items;
    }

    bool isTrue(const QString &value)
    {
        return value.trimmed() == QLatin1String("true");
    }

    // Splits Exec into arguments per the specification's quoting rules. Field
    // codes are left in place. Returns nothing for unbalanced quotes.
    std::optional<QStringList> splitExec(const QString &exec)
    {
        QStringList args;
        QString current;
        bool inQuotes = false;
        bool haveArg = false;
        for (int i = 0; i < exec.size(); ++i) {
            const QChar c = exec[i];
            if (inQuotes) {
                if (c == QLatin1Char('"')) {
                    inQuotes = false;
                } else if (c == QLatin1Char('\\') && i + 1 < exec.size()
                    && QStringLiteral("\"`$\\").contains(exec[i + 1])) {
                    current += exec[++i];
                } else {
                    current += c;
                }
            } else if (c == QLatin1Char('"')) {
                inQuotes = true;
                haveArg = true;
            } else if (c == QLatin1Char(' ') || c == QLatin1Char('\t')) {
                if (haveArg)
                    args.append(current);
                current.clear();
                haveArg = false;
            } else {
                current += c;
                haveArg = true;
            }
        }
        if (inQuotes)
            return std::nullopt;
        if (haveArg)
            args.append(current);
        return args;
    }

} // namespace

QStringList localeKeys(const QString &localeName)
{
    // lang_COUNTRY.ENCODING@MODIFIER
    QString name = localeName;
    QString modifier;
    const int at = name.indexOf(QLatin1Char('@'));
    if (at >= 0) {
        modifier = name.mid(at + 1);
        name.truncate(at);
    }
    const int dot = name.indexOf(QLatin1Char('.'));
    if (dot >= 0)
        name.truncate(dot);
    if (name.isEmpty() || name == QLatin1String("C") || name == QLatin1String("POSIX"))
        return {};
    const QString lang = name.section(QLatin1Char('_'), 0, 0);
    const QString country
        = name.contains(QLatin1Char('_')) ? name.section(QLatin1Char('_'), 1) : QString();

    QStringList keys;
    if (!country.isEmpty() && !modifier.isEmpty())
        keys << lang + QLatin1Char('_') + country + QLatin1Char('@') + modifier;
    if (!country.isEmpty())
        keys << lang + QLatin1Char('_') + country;
    if (!modifier.isEmpty())
        keys << lang + QLatin1Char('@') + modifier;
    keys << lang;
    return keys;
}

const QList<MainCategory> &mainCategories()
{
    // Display names follow NT where it had an equivalent (Accessories).
    static const QList<MainCategory> categories = {
        { "Utility", "Accessories" },
        { "Development", "Development" },
        { "Education", "Education" },
        { "Game", "Games" },
        { "Graphics", "Graphics" },
        { "Network", "Internet" },
        { "AudioVideo", "Multimedia" },
        { "Office", "Office" },
        { "Science", "Science" },
        { "Settings", "Settings" },
        { "System", "System Tools" },
    };
    return categories;
}

std::optional<DesktopEntry> DesktopEntry::parse(
    const QString &filePath, const QString &id, const QString &localeName)
{
    QFile file(filePath);
    // Desktop files are small; refuse anything unreasonably large.
    if (file.size() > 1024 * 1024 || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::nullopt;

    QHash<QString, QString> values;
    bool inMainGroup = false;
    bool sawMainGroup = false;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            inMainGroup = line == QLatin1String("[Desktop Entry]");
            sawMainGroup = sawMainGroup || inMainGroup;
            continue;
        }
        if (!inMainGroup)
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        if (!values.contains(key)) // first occurrence wins
            values.insert(key, line.mid(eq + 1).trimmed());
    }
    if (!sawMainGroup)
        return std::nullopt;

    auto localised = [&](const QString &key) {
        for (const QString &suffix : localeKeys(localeName)) {
            const QString localKey = key + QLatin1Char('[') + suffix + QLatin1Char(']');
            if (values.contains(localKey))
                return unescapeValue(values.value(localKey));
        }
        return unescapeValue(values.value(key));
    };

    DesktopEntry entry;
    entry.id = id;
    entry.filePath = filePath;
    entry.type = values.value(QStringLiteral("Type"));
    entry.name = localised(QStringLiteral("Name"));
    entry.icon = unescapeValue(values.value(QStringLiteral("Icon")));
    entry.exec = unescapeValue(values.value(QStringLiteral("Exec")));
    entry.tryExec = unescapeValue(values.value(QStringLiteral("TryExec")));
    entry.workingDirectory = unescapeValue(values.value(QStringLiteral("Path")));
    entry.terminal = isTrue(values.value(QStringLiteral("Terminal")));
    entry.noDisplay = isTrue(values.value(QStringLiteral("NoDisplay")));
    entry.hidden = isTrue(values.value(QStringLiteral("Hidden")));
    entry.categories = splitList(values.value(QStringLiteral("Categories")));
    entry.onlyShowIn = splitList(values.value(QStringLiteral("OnlyShowIn")));
    entry.notShowIn = splitList(values.value(QStringLiteral("NotShowIn")));
    return entry;
}

bool DesktopEntry::shouldShow(const QString &desktopName) const
{
    if (type != QLatin1String("Application") || noDisplay || hidden || name.isEmpty()
        || exec.isEmpty()) {
        return false;
    }
    if (!onlyShowIn.isEmpty() && !onlyShowIn.contains(desktopName))
        return false;
    if (notShowIn.contains(desktopName))
        return false;
    if (!tryExec.isEmpty()) {
        if (QFileInfo(tryExec).isAbsolute()) {
            if (!QFileInfo(tryExec).isExecutable())
                return false;
        } else if (QStandardPaths::findExecutable(tryExec).isEmpty()) {
            return false;
        }
    }
    return true;
}

QString DesktopEntry::mainCategory() const
{
    // The first main category listed wins.
    for (const QString &category : categories) {
        for (const MainCategory &main : mainCategories()) {
            if (category == QLatin1String(main.key))
                return category;
        }
    }
    return {};
}

std::optional<QStringList> DesktopEntry::commandLine(const QStringList &files) const
{
    const std::optional<QStringList> split = splitExec(exec);
    if (!split || split->isEmpty())
        return std::nullopt;

    QStringList args;
    for (const QString &arg : *split) {
        // Codes that stand alone and expand to several (or no) arguments.
        if (arg == QLatin1String("%F") || arg == QLatin1String("%U")) {
            args << files;
            continue;
        }
        if (arg == QLatin1String("%i")) {
            if (!icon.isEmpty())
                args << QStringLiteral("--icon") << icon;
            continue;
        }
        QString expanded;
        bool drop = false;
        for (int i = 0; i < arg.size(); ++i) {
            if (arg[i] != QLatin1Char('%')) {
                expanded += arg[i];
                continue;
            }
            if (i + 1 == arg.size())
                return std::nullopt; // trailing '%'
            const QChar code = arg[++i];
            switch (code.unicode()) {
            case '%':
                expanded += QLatin1Char('%');
                break;
            case 'f':
            case 'u':
                if (files.isEmpty())
                    drop = expanded.isEmpty() && i == arg.size() - 1;
                else
                    expanded += files.first();
                break;
            case 'c':
                expanded += name;
                break;
            case 'k':
                expanded += filePath;
                break;
            case 'd':
            case 'D':
            case 'n':
            case 'N':
            case 'v':
            case 'm':
                break; // deprecated codes are removed
            default:
                return std::nullopt; // %F/%U/%i inside an argument, or unknown
            }
        }
        if (!drop)
            args << expanded;
    }
    if (args.isEmpty() || args.first().isEmpty())
        return std::nullopt;
    return args;
}

} // namespace Daytona
