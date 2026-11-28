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
    bool critical = false; // urgency hint 2

    bool hasDefaultAction() const;
    // Asked never to expire (timeout 0), or critical: the specification
    // says neither closes by itself.
    bool staysOpen() const { return timeoutMs == 0 || critical; }
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

    // NT balloons stayed about 10 seconds; the server default (-1).
    static constexpr int DefaultTimeoutMs = 10000;
    // Applications cannot keep an expiring balloon up longer than this; a
    // balloon that must stay asks never to expire instead.
    static constexpr int MaxTimeoutMs = 60000;
    // A balloon that stays open is shown for at least this long (Windows
    // 2000's minimum balloon time) before a waiting one may take its place.
    static constexpr int DefaultTurnMs = DefaultTimeoutMs;
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
    // How long the balloon stays before it expires; 0 if it never does.
    int effectiveTimeoutMs(const Notification &notification) const;

    // While paused (the screen locked or the screen saver showing), the
    // shown balloon's time does not run, as Windows 2000 did not count time
    // nobody could see a balloon.
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

    // DefaultTurnMs, shorter in tests.
    void setTurnMs(int milliseconds) { m_turnMs = milliseconds; }

Q_SIGNALS:
    // The balloon should show this notification (or nothing, if empty).
    void currentChanged();
    void closed(uint id, Daytona::NotificationQueue::CloseReason reason);
    void actionInvoked(uint id, const QString &actionKey);

private:
    void showNext();
    // Starts the shown balloon's time: its timeout, or its turn if it
    // stays open.
    void startTimer();
    void runTimer(int milliseconds);
    void timedOut();
    // The shown balloon stays open and its turn is over: it goes to the back
    // of the queue, still open, and the next one shows.
    void yieldCurrent();

    std::optional<Notification> m_current;
    QList<Notification> m_queue;
    uint m_nextId = 1;
    QTimer m_expiry;
    int m_turnMs = DefaultTurnMs;
    bool m_paused = false;
    int m_remainingMs = -1; // while paused, the time left; -1 none
    bool m_turnOver = false; // the shown staying balloon may yield
};

} // namespace Daytona
