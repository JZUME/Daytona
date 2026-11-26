// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "colours.h"

#include <QObject>
#include <QStringList>
#include <functional>

class QProcess;

namespace Daytona {

class Settings;

enum class LockKind
{
    // Lock Workstation: the desktop colour, with the password box showing.
    Workstation,
    // A password-protected screen saver: a black screen until a key is
    // pressed, as Blank Screen looks.
    ScreenSaver
};

// swaylock's arguments for a lock in the colour scheme's colours. It ignores
// the user's own swaylock configuration, so Daytona's look and the
// daemonising the shell relies on cannot be changed by it.
QStringList swaylockArguments(LockKind kind, const SchemeColours &colours);

// swayidle's arguments: the screen saver after timeoutSeconds idle (none if
// 0), and a lock before sleep and when logind asks (loginctl lock-session).
// shell is daytona-shell's path; swayidle runs the commands with sh -c.
QStringList swayidleArguments(int timeoutSeconds, const QString &shell);

// Locks the session with swaylock (ext-session-lock-v1). swaylock is
// started daemonised (-f), so it returns once the screen is locked and the
// lock outlives the shell; a lock never depends on the shell staying up.
class ScreenLocker : public QObject
{
    Q_OBJECT

public:
    explicit ScreenLocker(QObject *parent = nullptr);

    // Whether swaylock is installed.
    bool isAvailable() const;
    // The locker; swaylock on PATH by default. For tests.
    void setProgram(const QString &program) { m_program = program; }

    // Locks the session and calls done once it is locked, or once locking
    // failed (logged), so a caller waiting to suspend is never held up.
    // While a lock is starting, further requests wait for that one.
    void lock(LockKind kind, std::function<void()> done = {});

private:
    void finish();

    QString m_program;
    QProcess *m_process = nullptr;
    QList<std::function<void()>> m_waiting;
};

// Runs swayidle for the session, with the screen saver's wait from the
// settings, restarting it when they change or idle is inhibited, as
// swayidle cannot change its timeouts while running.
class IdleMonitor : public QObject
{
    Q_OBJECT

public:
    IdleMonitor(Settings *settings, const QString &shell, QObject *parent = nullptr);
    ~IdleMonitor() override;

    // The idle tool; swayidle on PATH by default. For tests.
    void setProgram(const QString &program) { m_program = program; }

    // Starts swayidle. Returns false if it is not installed.
    bool start();

    // While inhibited (org.freedesktop.ScreenSaver), the screen saver never
    // starts; locking before sleep still happens.
    void setInhibited(bool inhibited);
    bool isInhibited() const { return m_inhibited; }

    // What swayidle runs with now, or would.
    QStringList arguments() const;
    // The running swayidle process, if any. For tests.
    QProcess *process() const { return m_process; }

private:
    void update();
    void stop();

    Settings *m_settings;
    QString m_shell;
    QString m_program;
    bool m_started = false;
    bool m_inhibited = false;
    QProcess *m_process = nullptr;
    QStringList m_running;
};

} // namespace Daytona
