// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationindex.h"

#include <QDir>
#include <QDirIterator>
#include <QLoggingCategory>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

Q_LOGGING_CATEGORY(lcApps, "daytona.applications", QtInfoMsg)

namespace Daytona {

namespace {
    constexpr int RescanDelayMs = 500; // package installs touch many files at once
} // namespace

QString messagesLocale()
{
    for (const char *var : { "LC_ALL", "LC_MESSAGES", "LANG" }) {
        const QString value = QString::fromLocal8Bit(qgetenv(var));
        if (!value.isEmpty())
            return value;
    }
    return QStringLiteral("C");
}

ApplicationIndex::ApplicationIndex(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(RescanDelayMs);
    connect(&m_debounce, &QTimer::timeout, this, &ApplicationIndex::rescan);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce,
        qOverload<>(&QTimer::start));
    connect(&m_scan, &QFutureWatcher<QList<DesktopEntry>>::finished, this, [this] {
        m_entries = m_scan.result();
        qCDebug(lcApps) << "indexed" << m_entries.size() << "applications";
        watch();
        Q_EMIT changed();
        if (m_rescanPending) {
            m_rescanPending = false;
            rescan();
        }
    });
}

QStringList ApplicationIndex::defaultDirectories()
{
    QStringList dirs;
    for (const QString &base :
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        dirs << QDir(base).filePath(QStringLiteral("applications"));
    return dirs;
}

QList<DesktopEntry> ApplicationIndex::scan(
    const QStringList &dirs, const QString &desktopName, const QString &localeName)
{
    QSet<QString> seen;
    QList<DesktopEntry> entries;
    for (const QString &dirPath : dirs) {
        const QDir dir(dirPath);
        if (!dir.exists())
            continue;
        QDirIterator it(
            dirPath, { QStringLiteral("*.desktop") }, QDir::Files, QDirIterator::Subdirectories);
        QStringList files;
        while (it.hasNext())
            files << it.next();
        files.sort(); // deterministic order within a directory
        for (const QString &file : files) {
            // The desktop-file ID: the path below applications/, with '/'
            // replaced by '-'.
            QString id = dir.relativeFilePath(file);
            id.replace(QLatin1Char('/'), QLatin1Char('-'));
            if (seen.contains(id))
                continue;
            seen.insert(id);
            std::optional<DesktopEntry> entry = DesktopEntry::parse(file, id, localeName);
            if (entry && entry->shouldShow(desktopName))
                entries.append(*entry);
        }
    }
    std::sort(entries.begin(), entries.end(), [](const DesktopEntry &a, const DesktopEntry &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return entries;
}

void ApplicationIndex::start(const QStringList &dirs)
{
    m_dirs = dirs;
    rescan();
}

void ApplicationIndex::rescan()
{
    if (m_scan.isRunning()) {
        m_rescanPending = true;
        return;
    }
    const QStringList dirs = m_dirs;
    const QString locale = messagesLocale();
    m_scan.setFuture(QtConcurrent::run(
        [dirs, locale] { return scan(dirs, QString::fromLatin1(DesktopName), locale); }));
}

void ApplicationIndex::watch()
{
    // Watch every existing applications directory and its subdirectories.
    // A directory created later is picked up once its parent changes.
    QStringList paths;
    for (const QString &dirPath : std::as_const(m_dirs)) {
        if (!QDir(dirPath).exists())
            continue;
        paths << dirPath;
        QDirIterator it(dirPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext())
            paths << it.next();
    }
    const QStringList current = m_watcher.directories();
    QStringList toAdd;
    for (const QString &path : std::as_const(paths)) {
        if (!current.contains(path))
            toAdd << path;
    }
    if (!toAdd.isEmpty())
        m_watcher.addPaths(toAdd);
}

LaunchResult launch(
    const DesktopEntry &entry, const QStringList &files, const QStringList &terminalCommand)
{
    std::optional<QStringList> command = entry.commandLine(files);
    if (!command)
        return { false, QStringLiteral("%1 has an invalid Exec line").arg(entry.id) };
    if (entry.terminal) {
        if (terminalCommand.isEmpty())
            return { false, QStringLiteral("no terminal is configured") };
        *command = terminalCommand + *command;
    }
    const QString program = command->takeFirst();
    QString workingDirectory = entry.workingDirectory;
    if (workingDirectory.isEmpty() || !QDir(workingDirectory).exists())
        workingDirectory = QDir::homePath();
    qint64 pid = 0;
    if (!QProcess::startDetached(program, *command, workingDirectory, &pid))
        return { false, QStringLiteral("could not start %1").arg(program) };
    qCDebug(lcApps) << "launched" << entry.id << "as pid" << pid;
    return { true, {} };
}

} // namespace Daytona
