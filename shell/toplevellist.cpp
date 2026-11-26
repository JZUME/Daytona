// SPDX-License-Identifier: GPL-3.0-or-later

#include "toplevellist.h"

#include <cstring>

namespace Daytona {

ToplevelList::ToplevelList(QObject *parent)
    : QObject(parent)
{
}

void ToplevelList::create(quint32 id)
{
    Toplevel toplevel;
    toplevel.id = id;
    m_pending.insert(id, toplevel);
}

void ToplevelList::setTitle(quint32 id, const QString &title)
{
    auto it = m_pending.find(id);
    if (it != m_pending.end())
        it->title = title.left(MaxTextLength);
}

void ToplevelList::setAppId(quint32 id, const QString &appId)
{
    auto it = m_pending.find(id);
    if (it != m_pending.end())
        it->appId = appId.left(MaxTextLength);
}

void ToplevelList::setState(quint32 id, const QByteArray &states)
{
    auto it = m_pending.find(id);
    if (it == m_pending.end())
        return;
    it->activated = it->minimized = it->maximized = it->fullscreen = false;
    const int count = states.size() / int(sizeof(quint32));
    for (int i = 0; i < count; ++i) {
        quint32 value;
        std::memcpy(&value, states.constData() + i * sizeof(quint32), sizeof(value));
        switch (value) {
        case Maximized:
            it->maximized = true;
            break;
        case Minimized:
            it->minimized = true;
            break;
        case Activated:
            it->activated = true;
            break;
        case Fullscreen:
            it->fullscreen = true;
            break;
        default:
            break; // states added in later protocol versions
        }
    }
}

void ToplevelList::setParentToplevel(quint32 id, quint32 parentId)
{
    auto it = m_pending.find(id);
    if (it != m_pending.end())
        it->parent = parentId == id ? 0 : parentId;
}

void ToplevelList::outputEnter(quint32 id, const QString &output)
{
    auto it = m_pending.find(id);
    if (it != m_pending.end() && !output.isEmpty() && !it->outputs.contains(output))
        it->outputs.append(output);
}

void ToplevelList::outputLeave(quint32 id, const QString &output)
{
    auto it = m_pending.find(id);
    if (it != m_pending.end())
        it->outputs.removeAll(output);
}

void ToplevelList::done(quint32 id)
{
    auto pending = m_pending.constFind(id);
    if (pending == m_pending.constEnd())
        return;
    auto current = m_current.find(id);
    if (current == m_current.end()) {
        m_current.insert(id, *pending);
        m_order.append(id);
        Q_EMIT added(id);
    } else if (!(*current == *pending)) {
        *current = *pending;
        Q_EMIT changed(id);
    }
}

void ToplevelList::close(quint32 id)
{
    m_pending.remove(id);
    if (m_current.remove(id)) {
        m_order.removeOne(id);
        Q_EMIT removed(id);
    }
    // A closed window cannot stay a parent. wlroots clears the parent of
    // such windows itself before the close, so this normally finds nothing.
    for (Toplevel &pending : m_pending) {
        if (pending.parent == id)
            pending.parent = 0;
    }
    for (quint32 childId : std::as_const(m_order)) {
        Toplevel &child = m_current[childId];
        if (child.parent == id) {
            child.parent = 0;
            Q_EMIT changed(childId);
        }
    }
}

QList<Toplevel> ToplevelList::toplevels() const
{
    QList<Toplevel> result;
    result.reserve(m_order.size());
    for (quint32 id : m_order)
        result.append(m_current.value(id));
    return result;
}

const Toplevel *ToplevelList::find(quint32 id) const
{
    auto it = m_current.constFind(id);
    return it == m_current.constEnd() ? nullptr : &*it;
}

QList<quint32> ToplevelList::owners() const
{
    QList<quint32> result;
    for (quint32 id : m_order) {
        if (m_current.value(id).parent == 0)
            result.append(id);
    }
    return result;
}

quint32 ToplevelList::rootOwner(quint32 id) const
{
    if (!m_current.contains(id))
        return 0;
    // Bounded, as the parent links come from the compositor and a loop
    // must not hang the shell.
    for (qsizetype steps = 0; steps < m_order.size(); ++steps) {
        const quint32 parent = m_current.value(id).parent;
        if (parent == 0 || !m_current.contains(parent))
            break;
        id = parent;
    }
    return id;
}

quint32 ToplevelList::activeOwner() const
{
    for (quint32 id : m_order) {
        if (m_current.value(id).activated)
            return rootOwner(id);
    }
    return 0;
}

QString ToplevelList::activeOutput() const
{
    for (quint32 id : m_order) {
        const Toplevel &toplevel = m_current[id];
        if (toplevel.activated)
            return toplevel.outputs.value(0);
    }
    return {};
}

} // namespace Daytona
