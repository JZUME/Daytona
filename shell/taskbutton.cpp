// SPDX-License-Identifier: GPL-3.0-or-later

#include "taskbutton.h"

#include "toplevellist.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QStyle>

namespace Daytona {

namespace {
    constexpr int ButtonHeight = 22;
    constexpr int IconSize = 16;
    // Bevel, padding and the gap between icon and text, in total.
    constexpr int NonTextWidth = 2 + 3 + IconSize + 4 + 3 + 2;
} // namespace

TaskButton::TaskButton(quint32 id, QWidget *parent)
    : QToolButton(parent)
    , m_id(id)
{
    setCheckable(true);
    setAutoRaise(false);
    setFocusPolicy(Qt::NoFocus); // see StartButton

    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setIconSize(QSize(IconSize, IconSize));
    setFixedHeight(ButtonHeight);
}

QIcon TaskButton::iconForAppId(const QString &appId)
{
    // App ids are untrusted; they are only ever used as icon theme names.
    if (!appId.isEmpty()) {
        if (QIcon::hasThemeIcon(appId))
            return QIcon::fromTheme(appId);
        const QString lastPart = appId.section(QLatin1Char('.'), -1).toLower();
        if (!lastPart.isEmpty() && QIcon::hasThemeIcon(lastPart))
            return QIcon::fromTheme(lastPart);
    }
    if (QIcon::hasThemeIcon(QStringLiteral("application-x-executable")))
        return QIcon::fromTheme(QStringLiteral("application-x-executable"));
    return QApplication::style()->standardIcon(QStyle::SP_TitleBarMenuButton);
}

void TaskButton::update(const Toplevel &toplevel)
{
    if (toplevel.appId != m_appId || icon().isNull()) {
        m_appId = toplevel.appId;
        setIcon(iconForAppId(m_appId));
    }
    // NT: the active window's button is latched down with bold text.
    const bool active = toplevel.activated && !toplevel.minimized;
    setChecked(active);
    QFont f = font();
    if (f.bold() != active) {
        f.setBold(active);
        setFont(f);
    }
    m_title = toplevel.title;
    setAccessibleName(m_title);
    updateText();
}

void TaskButton::contextMenuEvent(QContextMenuEvent *event)
{
    Q_EMIT windowMenuRequested(event->globalPos());
    event->accept();
}

void TaskButton::nextCheckState()
{
    // The checked state follows the compositor's "activated" state only.
}

void TaskButton::resizeEvent(QResizeEvent *event)
{
    QToolButton::resizeEvent(event);
    updateText();
}

void TaskButton::updateText()
{
    const int available = std::max(0, width() - NonTextWidth);
    const QString shown = fontMetrics().elidedText(m_title, Qt::ElideRight, available);
    setText(shown);
    setToolTip(shown == m_title ? QString() : m_title);
}

} // namespace Daytona
