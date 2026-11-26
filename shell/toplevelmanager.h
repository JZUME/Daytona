// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "qwayland-wlr-foreign-toplevel-management-unstable-v1.h"
#include "toplevellist.h"

#include <QHash>
#include <QtWaylandClient/QWaylandClientExtension>

namespace Daytona {

class ToplevelHandle;

// Binds zwlr_foreign_toplevel_manager_v1 and keeps a ToplevelList up to
// date. Requests go to the compositor; the list changes only when the
// compositor reports the result.
class ToplevelManager : public QWaylandClientExtensionTemplate<ToplevelManager>,
                        public QtWayland::zwlr_foreign_toplevel_manager_v1
{
    Q_OBJECT

public:
    explicit ToplevelManager(QObject *parent = nullptr);
    ~ToplevelManager() override;

    const ToplevelList &list() const { return m_list; }
    ToplevelList &list() { return m_list; }

    void activate(quint32 id);
    void setMinimized(quint32 id, bool minimized);
    void setMaximized(quint32 id, bool maximized);
    void close(quint32 id);

protected:
    void zwlr_foreign_toplevel_manager_v1_toplevel(
        struct ::zwlr_foreign_toplevel_handle_v1 *toplevel) override;
    void zwlr_foreign_toplevel_manager_v1_finished() override;

private:
    friend class ToplevelHandle;
    void handleClosed(quint32 id);
    // 0 for null or a handle this manager does not know.
    quint32 idForHandle(struct ::zwlr_foreign_toplevel_handle_v1 *object) const;

    ToplevelList m_list;
    QHash<quint32, ToplevelHandle *> m_handles;
    quint32 m_nextId = 0;
};

} // namespace Daytona
