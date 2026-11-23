// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

namespace Daytona {

// The shell's session D-Bus service, org.daytona.Shell. Owning the name
// keeps the shell single-instance; other processes (such as labwc's Start
// menu key binding) call its methods.
class ShellService : public QObject
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

public Q_SLOTS:
    Q_SCRIPTABLE void ToggleStartMenu();

Q_SIGNALS:
    void toggleStartMenuRequested();
};

} // namespace Daytona
