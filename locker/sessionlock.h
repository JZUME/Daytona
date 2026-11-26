// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "qwayland-ext-session-lock-v1.h"

#include <QObject>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>

class QWidget;

namespace Daytona {

// ext-session-lock-v1's global.
class SessionLockManager : public QWaylandClientExtensionTemplate<SessionLockManager>,
                           public QtWayland::ext_session_lock_manager_v1
{
public:
    SessionLockManager();
    ~SessionLockManager() override;
};

// The lock: the compositor blanks every output and sends input only to
// this client's lock surfaces until unlock(). If this process ends without
// unlocking, the session stays locked (labwc lets a new locker take over).
class SessionLock : public QObject, public QtWayland::ext_session_lock_v1
{
    Q_OBJECT

public:
    explicit SessionLock(SessionLockManager *manager);
    ~SessionLock() override;

    bool isLocked() const { return m_locked; }

    // Makes widget, a top-level not yet shown, this lock's surface on its
    // screen. Returns false if its screen has no output.
    bool attach(QWidget *widget);

    // Unlocks the session and waits until the compositor has seen it.
    void unlock();

Q_SIGNALS:
    // Every output is covered: the session is locked.
    void locked();
    // The compositor refused the lock (or ended it); the session is not
    // locked by this client.
    void finished();

protected:
    void ext_session_lock_v1_locked() override;
    void ext_session_lock_v1_finished() override;

private:
    bool m_locked = false;
};

} // namespace Daytona
