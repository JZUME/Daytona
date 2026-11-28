// SPDX-License-Identifier: GPL-3.0-or-later

#include "notificationqueue.h"

#include <utility>

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
    connect(&m_expiry, &QTimer::timeout, this, &NotificationQueue::timedOut);
}

int NotificationQueue::effectiveTimeoutMs(const Notification &notification) const
{
    if (notification.staysOpen())
        return 0;
    if (notification.timeoutMs < 0)
        return DefaultTimeoutMs;
    return qMin(notification.timeoutMs, MaxTimeoutMs);
}

void NotificationQueue::setPaused(bool paused)
{
    if (paused == m_paused)
        return;
    m_paused = paused;
    if (paused) {
        m_remainingMs = m_expiry.isActive() ? qMax(0, m_expiry.remainingTime()) : -1;
        m_expiry.stop();
    } else if (m_remainingMs >= 0) {
        m_expiry.start(std::exchange(m_remainingMs, -1));
    }
}

void NotificationQueue::runTimer(int milliseconds)
{
    if (m_paused) {
        m_expiry.stop();
        m_remainingMs = milliseconds;
    } else {
        m_expiry.start(milliseconds);
    }
}

void NotificationQueue::startTimer()
{
    m_turnOver = false;
    m_remainingMs = -1;
    const int timeout = effectiveTimeoutMs(*m_current);
    runTimer(timeout > 0 ? timeout : m_turnMs);
}

void NotificationQueue::timedOut()
{
    if (!m_current)
        return;
    if (!m_current->staysOpen()) {
        close(m_current->id, CloseReason::Expired);
        return;
    }
    // A staying balloon has had its turn: it gives way if another is
    // waiting, otherwise it stays and gives way to the next to arrive.
    m_turnOver = true;
    if (!m_queue.isEmpty())
        yieldCurrent();
}

void NotificationQueue::yieldCurrent()
{
    m_queue << *std::exchange(m_current, std::nullopt);
    showNext();
}

uint NotificationQueue::notify(Notification notification, uint replacesId)
{
    if (replacesId != 0) {
        if (m_current && m_current->id == replacesId) {
            notification.id = replacesId;
            m_current = notification;
            startTimer(); // a fresh timeout
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
    else if (m_turnOver)
        yieldCurrent();
    return notification.id;
}

bool NotificationQueue::close(uint id, CloseReason reason)
{
    if (m_current && m_current->id == id) {
        m_expiry.stop();
        m_remainingMs = -1;
        m_turnOver = false;
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
    startTimer();
    Q_EMIT currentChanged();
}

} // namespace Daytona
