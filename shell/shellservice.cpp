// SPDX-License-Identifier: GPL-3.0-or-later

#include "shellservice.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QLoggingCategory>
#include <cstdio>

Q_LOGGING_CATEGORY(lcService, "daytona.service", QtInfoMsg)

namespace Daytona {

ShellService::ShellService(QObject *parent)
    : QObject(parent)
{
}

ShellService::Registration ShellService::registerOnSessionBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return Registration::NoSessionBus;
    if (!bus.registerService(QString::fromLatin1(ServiceName))) {
        if (bus.interface()
            && bus.interface()->isServiceRegistered(QString::fromLatin1(ServiceName)))
            return Registration::AlreadyRunning;
        qCWarning(lcService) << "Could not register" << ServiceName << bus.lastError().message();
        return Registration::NoSessionBus;
    }
    bus.registerObject(
        QString::fromLatin1(ObjectPath), this, QDBusConnection::ExportScriptableSlots);
    return Registration::Registered;
}

namespace {

    constexpr const char *PrintScreenFailed = "org.daytona.Shell.Error.PrintScreenFailed";

    bool callShell(const char *method, int timeoutMs, const QVariantList &arguments = {})
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            std::fprintf(stderr, "daytona-shell: no D-Bus session bus\n");
            return false;
        }
        QDBusMessage call
            = QDBusMessage::createMethodCall(QString::fromLatin1(ShellService::ServiceName),
                QString::fromLatin1(ShellService::ObjectPath),
                QString::fromLatin1(ShellService::ServiceName), QString::fromLatin1(method));
        call.setArguments(arguments);
        const QDBusMessage reply = bus.call(call, QDBus::Block, timeoutMs);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            if (reply.errorName() == QLatin1String(PrintScreenFailed))
                std::fprintf(stderr, "daytona-shell: %s\n", qPrintable(reply.errorMessage()));
            else
                std::fprintf(stderr, "daytona-shell: no running shell: %s\n",
                    qPrintable(reply.errorMessage()));
            return false;
        }
        return true;
    }

} // namespace

bool ShellService::requestToggleStartMenu()
{
    return callShell("ToggleStartMenu", 2000);
}

bool ShellService::requestLock()
{
    // Long enough for the locker to start on a slow machine; logind allows a
    // sleep to be delayed for 5 seconds by default anyway.
    return callShell("Lock", 10000);
}

bool ShellService::requestScreenSaver()
{
    return callShell("StartScreenSaver", 2000);
}

bool ShellService::requestPrintScreen(bool activeWindow, const QString &file)
{
    return callShell("PrintScreen", 10000, { activeWindow, file });
}

void ShellService::PrintScreen(bool activeWindow, const QString &file)
{
    qCDebug(lcService) << "Print Screen requested over D-Bus" << activeWindow << file;
    std::function<void(bool)> done = [](bool) { };
    if (calledFromDBus()) {
        setDelayedReply(true);
        done = [bus = connection(), call = message()](bool ok) mutable {
            bus.send(ok ? call.createReply()
                        : call.createErrorReply(QString::fromLatin1(PrintScreenFailed),
                              QStringLiteral("the screen could not be captured")));
        };
    }
    Q_EMIT printScreenRequested(activeWindow, file, done);
}

void ShellService::Lock()
{
    qCDebug(lcService) << "Lock requested over D-Bus";
    std::function<void()> done = [] { };
    if (calledFromDBus()) {
        setDelayedReply(true);
        done = [bus = connection(), reply = message().createReply()]() mutable { bus.send(reply); };
    }
    Q_EMIT lockRequested(done);
}

void ShellService::StartScreenSaver()
{
    qCDebug(lcService) << "Screen saver requested over D-Bus";
    Q_EMIT screenSaverRequested();
}

void ShellService::ToggleStartMenu()
{
    qCDebug(lcService) << "Start menu toggle requested over D-Bus";
    Q_EMIT toggleStartMenuRequested();
}

} // namespace Daytona
