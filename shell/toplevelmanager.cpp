// SPDX-License-Identifier: GPL-3.0-or-later

#include "toplevelmanager.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QtGui/qguiapplication_platform.h>

Q_LOGGING_CATEGORY(lcToplevels, "daytona.toplevels", QtInfoMsg)

namespace Daytona {

// One window's handle: forwards its events into the list.
class ToplevelHandle : public QtWayland::zwlr_foreign_toplevel_handle_v1
{
public:
    ToplevelHandle(
        struct ::zwlr_foreign_toplevel_handle_v1 *object, quint32 id, ToplevelManager *manager)
        : QtWayland::zwlr_foreign_toplevel_handle_v1(object)
        , m_id(id)
        , m_manager(manager)
    {
        m_manager->m_list.create(id);
    }

    ~ToplevelHandle() override { destroy(); }

protected:
    void zwlr_foreign_toplevel_handle_v1_title(const QString &title) override
    {
        m_manager->m_list.setTitle(m_id, title);
    }
    void zwlr_foreign_toplevel_handle_v1_app_id(const QString &appId) override
    {
        m_manager->m_list.setAppId(m_id, appId);
    }
    void zwlr_foreign_toplevel_handle_v1_state(wl_array *state) override
    {
        m_manager->m_list.setState(
            m_id, QByteArray(static_cast<const char *>(state->data), int(state->size)));
    }
    void zwlr_foreign_toplevel_handle_v1_done() override { m_manager->m_list.done(m_id); }
    void zwlr_foreign_toplevel_handle_v1_closed() override { m_manager->handleClosed(m_id); }

private:
    quint32 m_id;
    ToplevelManager *m_manager;
};

ToplevelManager::ToplevelManager(QObject *parent)
    : QWaylandClientExtensionTemplate<ToplevelManager>(3)
{
    setParent(parent);
    connect(&m_list, &ToplevelList::added, this, [this](quint32 id) {
        const Toplevel *t = m_list.find(id);
        qCDebug(lcToplevels) << "added" << id << t->appId << t->title;
    });
    connect(&m_list, &ToplevelList::changed, this, [this](quint32 id) {
        const Toplevel *t = m_list.find(id);
        qCDebug(lcToplevels) << "changed" << id << t->title << "activated" << t->activated
                             << "minimized" << t->minimized << "maximized" << t->maximized;
    });
    connect(&m_list, &ToplevelList::removed, this,
        [](quint32 id) { qCDebug(lcToplevels) << "removed" << id; });
}

ToplevelManager::~ToplevelManager()
{
    qDeleteAll(m_handles);
    if (isActive())
        stop();
}

void ToplevelManager::zwlr_foreign_toplevel_manager_v1_toplevel(
    struct ::zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    const quint32 id = ++m_nextId;
    m_handles.insert(id, new ToplevelHandle(toplevel, id, this));
}

void ToplevelManager::zwlr_foreign_toplevel_manager_v1_finished()
{
    qCWarning(lcToplevels) << "The compositor stopped sending window information";
}

void ToplevelManager::handleClosed(quint32 id)
{
    m_list.close(id);
    delete m_handles.take(id);
}

void ToplevelManager::activate(quint32 id)
{
    ToplevelHandle *handle = m_handles.value(id);
    auto *waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!handle || !waylandApp || !waylandApp->seat())
        return;
    handle->activate(waylandApp->seat());
}

void ToplevelManager::setMinimized(quint32 id, bool minimized)
{
    if (ToplevelHandle *handle = m_handles.value(id))
        minimized ? handle->set_minimized() : handle->unset_minimized();
}

void ToplevelManager::setMaximized(quint32 id, bool maximized)
{
    if (ToplevelHandle *handle = m_handles.value(id))
        maximized ? handle->set_maximized() : handle->unset_maximized();
}

void ToplevelManager::close(quint32 id)
{
    if (ToplevelHandle *handle = m_handles.value(id))
        handle->close();
}

} // namespace Daytona
