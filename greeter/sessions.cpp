// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>

namespace Daytona {

namespace {

    // The spec's escapes in a value: \s \n \t \r \\.
    QString unescape(const QString &value)
    {
        QString result;
        result.reserve(value.size());
        for (qsizetype i = 0; i < value.size(); ++i) {
            const QChar c = value.at(i);
            if (c != QLatin1Char('\\') || i + 1 == value.size()) {
                result += c;
                continue;
            }
            const QChar next = value.at(++i);
            switch (next.unicode()) {
            case 's':
                result += QLatin1Char(' ');
                break;
            case 'n':
                result += QLatin1Char('\n');
                break;
            case 't':
                result += QLatin1Char('\t');
                break;
            case 'r':
                result += QLatin1Char('\r');
                break;
            default:
                result += next;
                break;
            }
        }
        return result;
    }

    bool isTrue(const QString &value)
    {
        return value.trimmed() == QLatin1String("true");
    }

    // The keys of [Desktop Entry].
    QHash<QString, QString> readEntry(const QString &path)
    {
        QHash<QString, QString> keys;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return keys;
        bool inEntry = false;
        bool seenEntry = false;
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;
            if (line.startsWith(QLatin1Char('['))) {
                inEntry = line == QLatin1String("[Desktop Entry]");
                seenEntry = seenEntry || inEntry;
                continue;
            }
            if (!inEntry)
                continue;
            const qsizetype equals = line.indexOf(QLatin1Char('='));
            if (equals <= 0)
                continue;
            const QString key = line.left(equals).trimmed();
            if (!keys.contains(key))
                keys.insert(key, line.mid(equals + 1).trimmed());
        }
        if (!seenEntry)
            keys.clear();
        return keys;
    }

    // Name[ll_CC@mod], Name[ll_CC], Name[ll@mod], Name[ll], then Name.
    QString localised(
        const QHash<QString, QString> &keys, const QString &key, const QString &locale)
    {
        QString name = locale.section(QLatin1Char('.'), 0, 0);
        QString modifier;
        const qsizetype at = locale.indexOf(QLatin1Char('@'));
        if (at >= 0) {
            modifier = locale.mid(at);
            name = name.section(QLatin1Char('@'), 0, 0);
        }
        const QString language = name.section(QLatin1Char('_'), 0, 0);
        QStringList candidates;
        if (!modifier.isEmpty())
            candidates << name + modifier;
        candidates << name;
        if (!modifier.isEmpty())
            candidates << language + modifier;
        candidates << language;
        for (const QString &candidate : std::as_const(candidates)) {
            const auto it = keys.constFind(key + QLatin1Char('[') + candidate + QLatin1Char(']'));
            if (!candidate.isEmpty() && it != keys.constEnd())
                return unescape(it.value());
        }
        return unescape(keys.value(key));
    }

    bool isInstalled(const QString &program)
    {
        if (program.startsWith(QLatin1Char('/'))) {
            const QFileInfo info(program);
            return info.isFile() && info.isExecutable();
        }
        return !QStandardPaths::findExecutable(program).isEmpty();
    }

} // namespace

QStringList DesktopSession::environment() const
{
    QStringList env { QStringLiteral("XDG_SESSION_TYPE=wayland"),
        QStringLiteral("XDG_SESSION_DESKTOP=") + id };
    if (!desktopNames.isEmpty())
        env << QStringLiteral("XDG_CURRENT_DESKTOP=") + desktopNames.join(QLatin1Char(':'));
    return env;
}

QStringList splitExec(const QString &exec)
{
    QStringList arguments;
    QString current;
    bool inArgument = false;
    bool quoted = false;
    for (qsizetype i = 0; i < exec.size(); ++i) {
        const QChar c = exec.at(i);
        if (quoted) {
            if (c == QLatin1Char('"')) {
                quoted = false;
            } else if (c == QLatin1Char('\\') && i + 1 < exec.size()) {
                current += exec.at(++i);
            } else {
                current += c;
            }
            continue;
        }
        if (c == QLatin1Char(' ') || c == QLatin1Char('\t')) {
            if (inArgument)
                arguments << current;
            current.clear();
            inArgument = false;
            continue;
        }
        inArgument = true;
        if (c == QLatin1Char('"')) {
            quoted = true;
        } else if (c == QLatin1Char('%') && i + 1 < exec.size()) {
            const QChar code = exec.at(++i);
            if (code == QLatin1Char('%'))
                current += code;
            // Other field codes expand to nothing for a session.
        } else {
            current += c;
        }
    }
    if (quoted)
        return {};
    if (inArgument && !current.isEmpty())
        arguments << current;
    arguments.removeAll(QString());
    return arguments;
}

bool readSessionFile(const QString &path, const QString &locale, DesktopSession *session)
{
    const QHash<QString, QString> keys = readEntry(path);
    if (keys.isEmpty())
        return false;
    if (isTrue(keys.value(QStringLiteral("Hidden")))
        || isTrue(keys.value(QStringLiteral("NoDisplay"))))
        return false;
    const QString tryExec = unescape(keys.value(QStringLiteral("TryExec")));
    if (!tryExec.isEmpty() && !isInstalled(tryExec))
        return false;
    const QStringList command = splitExec(unescape(keys.value(QStringLiteral("Exec"))));
    if (command.isEmpty())
        return false;

    session->id = QFileInfo(path).completeBaseName();
    session->name = localised(keys, QStringLiteral("Name"), locale);
    if (session->name.isEmpty())
        session->name = session->id;
    session->command = command;
    session->desktopNames = unescape(keys.value(QStringLiteral("DesktopNames")))
                                .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    return true;
}

QList<DesktopSession> findSessions(const QStringList &dataDirs, const QString &locale)
{
    QList<DesktopSession> sessions;
    QSet<QString> seen;
    for (const QString &dataDir : dataDirs) {
        const QDir dir(dataDir + QStringLiteral("/wayland-sessions"));
        const QStringList files
            = dir.entryList({ QStringLiteral("*.desktop") }, QDir::Files, QDir::Name);
        for (const QString &file : files) {
            const QString id = QFileInfo(file).completeBaseName();
            if (seen.contains(id))
                continue;
            // A file that is hidden or broken still hides the same id in
            // later directories, as the spec says.
            seen.insert(id);
            DesktopSession session;
            if (readSessionFile(dir.filePath(file), locale, &session))
                sessions.append(session);
        }
    }
    std::stable_sort(
        sessions.begin(), sessions.end(), [](const DesktopSession &a, const DesktopSession &b) {
            return a.name.toCaseFolded().localeAwareCompare(b.name.toCaseFolded()) < 0;
        });
    return sessions;
}

QStringList systemDataDirs()
{
    const QString value = qEnvironmentVariable("XDG_DATA_DIRS");
    if (value.isEmpty())
        return { QStringLiteral("/usr/local/share"), QStringLiteral("/usr/share") };
    return value.split(QLatin1Char(':'), Qt::SkipEmptyParts);
}

int defaultSession(const QList<DesktopSession> &sessions, const QString &remembered)
{
    for (const QString &id : { remembered, QStringLiteral("daytona") }) {
        for (qsizetype i = 0; i < sessions.size(); ++i) {
            if (!id.isEmpty() && sessions.at(i).id == id)
                return int(i);
        }
    }
    return sessions.isEmpty() ? -1 : 0;
}

} // namespace Daytona
