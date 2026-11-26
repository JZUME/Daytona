// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

namespace Daytona {

// One open window as the compositor reports it.
struct Toplevel
{
    quint32 id = 0;
    QString title;
    QString appId;
    bool activated = false;
    bool minimized = false;
    bool maximized = false;
    bool fullscreen = false;
    // The window that owns this one (a dialog's main window), or 0 for an
    // owner window. NT gives only owner windows a taskbar button.
    quint32 parent = 0;

    bool operator==(const Toplevel &other) const
    {
        return id == other.id && title == other.title && appId == other.appId
            && activated == other.activated && minimized == other.minimized
            && maximized == other.maximized && fullscreen == other.fullscreen
            && parent == other.parent;
    }
};

// The shell's view of open windows, fed by foreign-toplevel protocol events.
//
// The compositor is the source of truth: this only mirrors what it reports.
// Changes are staged per window and applied together when the compositor
// sends "done", as the protocol requires. A window becomes visible at its
// first "done" and keeps its place in launch order until it closes. When a
// window closes, windows it was the parent of lose their parent.
class ToplevelList : public QObject
{
    Q_OBJECT

public:
    // Values from zwlr_foreign_toplevel_handle_v1.state.
    enum ProtocolState : quint32
    {
        Maximized = 0,
        Minimized = 1,
        Activated = 2,
        Fullscreen = 3
    };

    // Window titles and app ids come from other applications; cap them so a
    // misbehaving client cannot make the shell hold huge strings.
    static constexpr int MaxTextLength = 512;

    explicit ToplevelList(QObject *parent = nullptr);

    // Protocol events.
    void create(quint32 id);
    void setTitle(quint32 id, const QString &title);
    void setAppId(quint32 id, const QString &appId);
    // states is the raw wl_array contents: native-endian uint32 values.
    void setState(quint32 id, const QByteArray &states);
    // parentId is 0 when the window has no parent.
    void setParentToplevel(quint32 id, quint32 parentId);
    void done(quint32 id);
    void close(quint32 id);

    // Windows that have been announced, in launch order.
    QList<Toplevel> toplevels() const;
    const Toplevel *find(quint32 id) const;

    // Announced windows without a parent, in launch order: the ones that
    // get a taskbar button.
    QList<quint32> owners() const;
    // The top of id's chain of parents (id itself for an owner window), or
    // 0 if id is not announced.
    quint32 rootOwner(quint32 id) const;
    // The root owner of the active window, or 0 if no window is active. NT
    // shows this owner's button pressed while one of its dialogs is active.
    quint32 activeOwner() const;

Q_SIGNALS:
    void added(quint32 id);
    void changed(quint32 id);
    void removed(quint32 id);

private:
    QHash<quint32, Toplevel> m_pending; // staged, not yet "done"
    QHash<quint32, Toplevel> m_current; // announced windows
    QList<quint32> m_order; // announced windows, launch order
};

} // namespace Daytona
