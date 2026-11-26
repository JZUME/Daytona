// SPDX-License-Identifier: GPL-3.0-or-later

#include "inputlocaleicon.h"

#include "inputlocales.h"
#include "volumepopup.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

namespace Daytona {

namespace {
    // As TrayIcon: 16x16 with a 2 pixel gap.
    constexpr int IconSize = 16;
    constexpr int Gap = 2;

    QFont indicatorFont(QFont font)
    {
        font.setPixelSize(10);
        return font;
    }
} // namespace

InputLocaleIcon::InputLocaleIcon(InputLocales *locales, QWidget *parent)
    : QWidget(parent)
    , m_locales(locales)
{
    setAttribute(Qt::WA_AlwaysShowToolTips);
    connect(locales, &InputLocales::changed, this, &InputLocaleIcon::stateChanged);
    stateChanged();
}

QSize InputLocaleIcon::sizeHint() const
{
    return { IconSize + Gap, IconSize };
}

void InputLocaleIcon::paintIndicator(QPainter *painter, const QRect &rect, const QString &text,
    const QPalette &palette, const QFont &font)
{
    // NT drew it in the selection colours, so it follows the scheme.
    painter->fillRect(rect, palette.color(QPalette::Active, QPalette::Highlight));
    painter->setPen(palette.color(QPalette::Active, QPalette::HighlightedText));
    painter->setFont(indicatorFont(font));
    painter->drawText(rect, Qt::AlignCenter, text);
}

void InputLocaleIcon::stateChanged()
{
    const QList<InputLocale> &locales = m_locales->locales();
    setVisible(locales.size() > 1);
    const LayoutInfo info = m_registry.find(m_locales->current());
    m_text = LayoutRegistry::indicatorText(info);
    setToolTip(info.description);
    setAccessibleName(tr("Input locale: %1").arg(info.description));
    update();
}

void InputLocaleIcon::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    paintIndicator(&painter, QRect(Gap / 2, (height() - IconSize) / 2, IconSize, IconSize), m_text,
        palette(), font());
}

QMenu *InputLocaleIcon::localeMenu()
{
    delete m_menu;
    m_menu = new QMenu(this);
    auto *group = new QActionGroup(m_menu);
    const QList<InputLocale> &locales = m_locales->locales();
    for (int i = 0; i < locales.size(); ++i) {
        // The one in use is marked; NT's menu used the check column for it.
        QAction *action = m_menu->addAction(m_registry.find(locales.at(i)).description);
        action->setCheckable(true);
        action->setChecked(i == m_locales->currentIndex());
        group->addAction(action);
        connect(action, &QAction::triggered, m_locales, [this, i] { m_locales->select(i); });
    }
    return m_menu;
}

void InputLocaleIcon::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        showMenu(localeMenu());
}

void InputLocaleIcon::contextMenuEvent(QContextMenuEvent *)
{
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(tr("&Properties"), this, &InputLocaleIcon::propertiesRequested);
    showMenu(menu);
}

void InputLocaleIcon::showMenu(QMenu *menu)
{
    popUpBesideTrayIcon(menu, this);
}

} // namespace Daytona
