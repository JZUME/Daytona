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

} // namespace Daytona
