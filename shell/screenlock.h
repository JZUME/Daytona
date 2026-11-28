// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QStringList>
#include <functional>

class QProcess;

namespace Daytona {

class Settings;

enum class LockKind
{
    // Lock Workstation: the desktop colour with NT's Workstation Locked
    // dialog.
    Workstation,
    // A password-protected screen saver: a black screen until a key is
    // pressed or the pointer moves, as Blank Screen looks, then Workstation
    // Locked.
    ScreenSaver
};

// daytona-lock's arguments for a lock of this kind.
QStringList lockArguments(LockKind kind);

// daytona-lock next to this program: installed in libexec/daytona, or in
// the build tree's locker/.
QString defaultLockProgram();

// swayidle's arguments: the screen saver after timeoutSeconds idle (none if
// 0), and a lock before sleep and when logind asks (loginctl lock-session).
// shell is daytona-shell's path; swayidle runs the commands with sh -c.
QStringList swayidleArguments(int timeoutSeconds, const QString &shell);

// Locks the session with daytona-lock (ext-session-lock-v1), which prints
// "locked" once the screen is locked and exits once it is unlocked. If it
// crashes while locked, the compositor keeps the session locked and the
// locker is started again, to take the lock over. The locker outlives the
// shell: a lock never depends on the shell staying up.
class ScreenLocker : public QObject
{
    Q_OBJECT

public:
    explicit ScreenLocker(QObject *parent = nullptr);
    // Leaves a running locker running.
    ~ScreenLocker() override;

    // Whether daytona-lock is installed.
    bool isAvailable() const;
    // The locker; defaultLockProgram() by default. For tests.
    void setProgram(const QString &program) { m_program = program; }

    // Locks the session and calls done once it is locked, or once locking
    // failed (logged), so a caller waiting to suspend is never held up.
    // While a lock is starting, further requests wait for that one; while
    // locked, they are done at once.
    void lock(LockKind kind, std::function<void()> done = {});

    // Whether this shell's locker has the session locked.
    bool isLocked() const { return m_locked; }

Q_SIGNALS:
    void lockedChanged(bool locked);

private:
    void start();
    void finish();

    QString m_program;
    QProcess *m_process = nullptr;
    LockKind m_kind = LockKind::Workstation;
    bool m_locked = false;
    QList<qint64> m_restarts;
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
