// SPDX-License-Identifier: GPL-3.0-or-later

#include "notificationqueue.h"

namespace Daytona {

bool Notification::hasDefaultAction() const
{
    for (int i = 0; i + 1 < actions.size(); i += 2) {
        if (actions[i] == QLatin1String("default"))
            return true;
    }
    return false;
}

NotificationQueue::NotificationQueue(QObject *parent)
    : QObject(parent)
{
    m_expiry.setSingleShot(true);
    connect(&m_expiry, &QTimer::timeout, this, [this] {
        if (m_current)
            close(m_current->id, CloseReason::Expired);
    });
}

int NotificationQueue::effectiveTimeoutMs(const Notification &notification) const
{
    if (notification.timeoutMs <= 0)
        return DefaultTimeoutMs;
    return qMin(notification.timeoutMs, MaxTimeoutMs);
}

uint NotificationQueue::notify(Notification notification, uint replacesId)
{
    if (replacesId != 0) {
        if (m_current && m_current->id == replacesId) {
            notification.id = replacesId;
            m_current = notification;
            m_expiry.start(effectiveTimeoutMs(notification)); // a fresh timeout
            Q_EMIT currentChanged();
            return replacesId;
        }
        for (Notification &queued : m_queue) {
            if (queued.id == replacesId) {
                notification.id = replacesId;
                queued = notification;
                return replacesId;
            }
        }
    }
    notification.id = m_nextId++;
    if (m_nextId == 0) // ids are never 0
        m_nextId = 1;
    if (m_queue.size() >= MaxQueued) {
        // Too many waiting: the oldest waiting one is dropped.
        const uint dropped = m_queue.takeFirst().id;
        Q_EMIT closed(dropped, CloseReason::Undefined);
    }
    m_queue << notification;
    if (!m_current)
        showNext();
    return notification.id;
}

bool NotificationQueue::close(uint id, CloseReason reason)
{
    if (m_current && m_current->id == id) {
        m_expiry.stop();
        m_current.reset();
        Q_EMIT closed(id, reason);
        showNext();
        return true;
    }
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue[i].id == id) {
            m_queue.removeAt(i);
            Q_EMIT closed(id, reason);
            return true;
        }
    }
    return false;
}

void NotificationQueue::activateCurrent()
{
    if (!m_current)
        return;
    const uint id = m_current->id;
    if (m_current->hasDefaultAction())
        Q_EMIT actionInvoked(id, QStringLiteral("default"));
    close(id, CloseReason::Dismissed);
}

std::optional<Notification> NotificationQueue::current() const
{
    return m_current;
}

void NotificationQueue::showNext()
{
    if (m_queue.isEmpty()) {
        Q_EMIT currentChanged();
        return;
    }
    m_current = m_queue.takeFirst();
    m_expiry.start(effectiveTimeoutMs(*m_current));
    Q_EMIT currentChanged();
}

} // namespace Daytona
