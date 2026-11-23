// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <optional>

namespace Daytona {

struct Notification
{
    uint id = 0;
    QString appName;
    QString appIcon; // icon name, absolute path or file:// URL
    QImage image; // from the image-data hint, if any
    QString summary;
    QString body; // shown as plain text
    QStringList actions; // key, label, key, label, ...
    int timeoutMs = -1; // as sent: -1 server default, 0 never

    bool hasDefaultAction() const;
};

// The notifications waiting to be shown as balloons, one at a time, with
// the freedesktop rules for ids, replacement, timeouts and close reasons.
class NotificationQueue : public QObject
{
    Q_OBJECT

public:
    // Reasons for NotificationClosed, from the specification.
    enum class CloseReason : uint
    {
        Expired = 1,
        Dismissed = 2,
        ClosedByCall = 3,
        Undefined = 4
    };
    Q_ENUM(CloseReason)

    // NT balloons stayed about 10 seconds. Notifications asking for the
    // server default (-1) or never to expire (0) use this too: a balloon
    // that never closed would hide the ones queued behind it.
    static constexpr int DefaultTimeoutMs = 10000;
    // Applications cannot keep a balloon up longer than this.
    static constexpr int MaxTimeoutMs = 60000;
    static constexpr int MaxQueued = 50;

    explicit NotificationQueue(QObject *parent = nullptr);

    // Adds or replaces a notification and returns its id. replacesId
    // updates the shown or queued notification with that id in place; an
    // unknown replacesId is treated as a new notification.
    uint notify(Notification notification, uint replacesId = 0);

    // Closes a shown or queued notification. Returns false if unknown.
    bool close(uint id, CloseReason reason);

    // The balloon's body was clicked: invokes "default" if present, then
    // closes the balloon as dismissed.
    void activateCurrent();

    std::optional<Notification> current() const;
    QList<Notification> queued() const { return m_queue; }
    int effectiveTimeoutMs(const Notification &notification) const;

Q_SIGNALS:
    // The balloon should show this notification (or nothing, if empty).
    void currentChanged();
    void closed(uint id, Daytona::NotificationQueue::CloseReason reason);
    void actionInvoked(uint id, const QString &actionKey);

private:
    void showNext();

    std::optional<Notification> m_current;
    QList<Notification> m_queue;
    uint m_nextId = 1;
    QTimer m_expiry;
};

} // namespace Daytona
