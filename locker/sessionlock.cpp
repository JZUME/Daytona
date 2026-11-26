// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessionlock.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPointer>
#include <QWidget>
#include <QWindow>
#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include <QtWaylandClient/private/qwaylandshellintegration_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <memory>
#include <wayland-client-core.h>

Q_LOGGING_CATEGORY(lcSessionLock, "daytona.lock.wayland", QtInfoMsg)

// Qt has no public API for a window with a role other than xdg-shell's, so
// a lock surface is a Qt Wayland shell integration, set on each window (as
// LayerShellQt does for layer surfaces). This uses Qt's private Wayland
// client API, which is tied to the Qt version built against.

namespace Daytona {

namespace {

    class LockSurface : public QtWaylandClient::QWaylandShellSurface,
                        public QtWayland::ext_session_lock_surface_v1
    {
    public:
        LockSurface(SessionLock *lock, ::wl_output *output, QtWaylandClient::QWaylandWindow *window)
            : QWaylandShellSurface(window)
        {
            // Qt commits the new surface before it has a buffer, which is a
            // protocol error for a lock surface, so the role is given after
            // that. (Qt 6.9 can be told not to, but Debian 13 has 6.8.)
            QPointer<SessionLock> guard(lock);
            QMetaObject::invokeMethod(
                this,
                [this, guard, output] {
                    if (guard && guard->isInitialized())
                        init(ext_session_lock_v1_get_lock_surface(
                            guard->object(), wlSurface(), output));
                },
                Qt::QueuedConnection);
        }

        ~LockSurface() override
        {
            if (isInitialized())
                destroy();
        }

        // Nothing is drawn until the compositor gives the size.
        bool isExposed() const override { return m_configured; }

        void applyConfigure() override { window()->resizeFromApplyConfigure(m_size); }

    protected:
        void ext_session_lock_surface_v1_configure(
            uint32_t serial, uint32_t width, uint32_t height) override
        {
            ack_configure(serial);
            m_size = QSize(int(width), int(height));
            if (!m_configured) {
                m_configured = true;
                applyConfigure();
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
                window()->updateExposure();
#else
                window()->sendRecursiveExposeEvent();
#endif
            } else {
                // Later sizes are applied when the window is not painting.
                window()->applyConfigureWhenPossible();
            }
        }

    private:
        QSize m_size;
        bool m_configured = false;
    };

    class LockShellIntegration : public QtWaylandClient::QWaylandShellIntegration
    {
    public:
        explicit LockShellIntegration(SessionLock *lock)
            : m_lock(lock)
        {
        }

        bool initialize(QtWaylandClient::QWaylandDisplay *) override { return true; }

        QtWaylandClient::QWaylandShellSurface *createShellSurface(
            QtWaylandClient::QWaylandWindow *window) override
        {
            ::wl_output *output
                = window->waylandScreen() ? window->waylandScreen()->output() : nullptr;
            if (!m_lock->isInitialized() || !output)
                return nullptr;
            return new LockSurface(m_lock, output, window);
        }

    private:
        SessionLock *m_lock;
    };

    // One per process: there is only one lock.
    std::unique_ptr<LockShellIntegration> s_integration;

} // namespace

SessionLockManager::SessionLockManager()
    : QWaylandClientExtensionTemplate(1)
{
    initialize();
}

SessionLockManager::~SessionLockManager()
{
    if (isInitialized())
        destroy();
}

SessionLock::SessionLock(SessionLockManager *manager)
    : QtWayland::ext_session_lock_v1(manager->lock())
{
    s_integration = std::make_unique<LockShellIntegration>(this);
}

SessionLock::~SessionLock()
{
    // Without unlock_and_destroy, the session stays locked.
    if (isInitialized())
        destroy();
}

bool SessionLock::attach(QWidget *widget)
{
    widget->winId();
    auto *window = dynamic_cast<QtWaylandClient::QWaylandWindow *>(
        widget->windowHandle() ? widget->windowHandle()->handle() : nullptr);
    if (!window) {
        qCWarning(lcSessionLock) << "Not a Wayland window";
        return false;
    }
    if (!window->waylandScreen() || !window->waylandScreen()->output()) {
        qCInfo(lcSessionLock) << "No output for screen" << widget->screen();
        return false;
    }
    window->setShellIntegration(s_integration.get());
    return true;
}

void SessionLock::unlock()
{
    if (!isInitialized())
        return;
    if (m_locked)
        unlock_and_destroy();
    else
        destroy();
    // Exiting straight after could leave the request unread.
    auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (wayland && wayland->display())
        wl_display_roundtrip(wayland->display());
}

void SessionLock::ext_session_lock_v1_locked()
{
    m_locked = true;
    Q_EMIT locked();
}

void SessionLock::ext_session_lock_v1_finished()
{
    // Not ours to unlock; it must only be destroyed.
    if (isInitialized())
        destroy();
    const bool wasLocked = std::exchange(m_locked, false);
    if (wasLocked)
        qCWarning(lcSessionLock) << "The compositor ended the lock";
    Q_EMIT finished();
}

} // namespace Daytona
