// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "notificationqueue.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QObject>
#include <QVariantMap>

class QDBusArgument;

namespace Daytona {

// Reads the image-data hint, (iiibiiay): bounded and checked, as it comes
// from other programs. Null if absent or malformed.
QImage parseNotificationImage(const QDBusArgument &argument);

// org.freedesktop.Notifications, feeding a NotificationQueue.
class NotificationService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    static constexpr const char *ServiceName = "org.freedesktop.Notifications";
    static constexpr const char *ObjectPath = "/org/freedesktop/Notifications";

    NotificationService(
        NotificationQueue *queue, QDBusConnection connection, QObject *parent = nullptr);

    // Takes the name. Returns false if another notification server has it.
    bool registerOnBus();

public Q_SLOTS:
    Q_SCRIPTABLE uint Notify(const QString &appName, uint replacesId, const QString &appIcon,
        const QString &summary, const QString &body, const QStringList &actions,
        const QVariantMap &hints, int expireTimeout);
    Q_SCRIPTABLE void CloseNotification(uint id);
    Q_SCRIPTABLE QStringList GetCapabilities();
    Q_SCRIPTABLE QString GetServerInformation(
        QString &vendor, QString &version, QString &specVersion);

Q_SIGNALS:
    Q_SCRIPTABLE void NotificationClosed(uint id, uint reason);
    Q_SCRIPTABLE void ActionInvoked(uint id, const QString &actionKey);
    // Not on the bus: the sound a notification asks for, as a freedesktop
    // sound name or a file (one of the two is empty). New notifications
    // without a sound hint play message-new-instant; suppress-sound plays
    // nothing.
    void soundRequested(const QString &name, const QString &file);

private:
    NotificationQueue *m_queue;
    QDBusConnection m_connection;
};

} // namespace Daytona
