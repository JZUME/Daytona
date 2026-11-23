// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "desktopentry.h"

#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>

namespace Daytona {

// The installed applications the Start menu lists, kept up to date as
// .desktop files change. Scanning happens off the UI thread.
class ApplicationIndex : public QObject
{
    Q_OBJECT

public:
    static constexpr const char *DesktopName = "Daytona";

    explicit ApplicationIndex(QObject *parent = nullptr);

    // $XDG_DATA_HOME/applications, then each $XDG_DATA_DIRS/applications.
    static QStringList defaultDirectories();

    // Reads every .desktop file under dirs. A desktop-file ID found in an
    // earlier directory hides the same ID in later ones (even when the
    // earlier one is Hidden). Returns the entries to show, sorted by name.
    static QList<DesktopEntry> scan(
        const QStringList &dirs, const QString &desktopName, const QString &localeName);

    // Scans dirs in the background and keeps watching them for changes.
    void start(const QStringList &dirs = defaultDirectories());

    QList<DesktopEntry> entries() const { return m_entries; }

Q_SIGNALS:
    void changed();

private:
    void rescan();
    void watch();

    QStringList m_dirs;
    QList<DesktopEntry> m_entries;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QFutureWatcher<QList<DesktopEntry>> m_scan;
    bool m_rescanPending = false;
};

// The current POSIX locale for messages (LC_ALL, LC_MESSAGES, LANG).
QString messagesLocale();

struct LaunchResult
{
    bool ok = false;
    QString error;
};

// Starts an application detached, with structured arguments and no shell.
// Terminal applications run inside terminalCommand.
LaunchResult launch(const DesktopEntry &entry, const QStringList &files = {},
    const QStringList &terminalCommand
    = { QStringLiteral("x-terminal-emulator"), QStringLiteral("-e") });

} // namespace Daytona
