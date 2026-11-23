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

bool ShellService::requestToggleStartMenu()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        std::fprintf(stderr, "daytona-shell: no D-Bus session bus\n");
        return false;
    }
    const QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(ServiceName),
        QString::fromLatin1(ObjectPath), QString::fromLatin1(ServiceName),
        QStringLiteral("ToggleStartMenu"));
    const QDBusMessage reply = bus.call(call, QDBus::Block, 2000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        std::fprintf(
            stderr, "daytona-shell: no running shell: %s\n", qPrintable(reply.errorMessage()));
        return false;
    }
    return true;
}

void ShellService::ToggleStartMenu()
{
    qCDebug(lcService) << "Start menu toggle requested over D-Bus";
    Q_EMIT toggleStartMenuRequested();
}

} // namespace Daytona
