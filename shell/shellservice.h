// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusContext>
#include <QObject>
#include <functional>

namespace Daytona {

// The shell's session D-Bus service, org.daytona.Shell. Owning the name
// keeps the shell single-instance; other processes (such as labwc's Start
// menu key binding) call its methods.
class ShellService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.daytona.Shell")

public:
    static constexpr const char *ServiceName = "org.daytona.Shell";
    static constexpr const char *ObjectPath = "/org/daytona/Shell";

    enum class Registration
    {
        Registered,
        AlreadyRunning,
        NoSessionBus
    };

    explicit ShellService(QObject *parent = nullptr);

    Registration registerOnSessionBus();

    // Asks a running shell to toggle its Start menu. Returns false (with a
    // message on stderr) if no shell is running.
    static bool requestToggleStartMenu();
    // Asks a running shell to lock the session, returning once it is locked
    // (swayidle waits for this before the computer sleeps).
    static bool requestLock();
    // Asks a running shell to start the screen saver (swayidle's timeout).
    static bool requestScreenSaver();

public Q_SLOTS:
    Q_SCRIPTABLE void ToggleStartMenu();
    // Replies once the session is locked.
    Q_SCRIPTABLE void Lock();
    Q_SCRIPTABLE void StartScreenSaver();

Q_SIGNALS:
    void toggleStartMenuRequested();
    // done must be called once the session is locked (or locking failed).
    void lockRequested(const std::function<void()> &done);
    void screenSaverRequested();
};

} // namespace Daytona
