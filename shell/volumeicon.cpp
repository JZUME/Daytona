// SPDX-License-Identifier: GPL-3.0-or-later

#include "volumeicon.h"

#include "taskbar.h"
#include "volumecontrol.h"
#include "volumepopup.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>

namespace Daytona {

namespace {
    // As TrayIcon: 16x16 with a 2 pixel gap.
    constexpr int IconSize = 16;
    constexpr int Gap = 2;
} // namespace

VolumeIcon::VolumeIcon(VolumeControl *control, QWidget *parent)
    : QWidget(parent)
    , m_control(control)
    , m_popup(new VolumePopup(this))
{
    setAttribute(Qt::WA_AlwaysShowToolTips);
    setToolTip(tr("Volume"));
    m_clickTimer.setSingleShot(true);
    connect(&m_clickTimer, &QTimer::timeout, this, &VolumeIcon::showPopup);
    connect(m_popup, &VolumePopup::volumeRequested, control, &VolumeControl::setVolume);
    connect(m_popup, &VolumePopup::muteRequested, control, &VolumeControl::setMuted);
    connect(control, &VolumeControl::changed, this, &VolumeIcon::stateChanged);
    stateChanged();
}

QSize VolumeIcon::sizeHint() const
{
    return { IconSize + Gap, IconSize };
}

QString VolumeIcon::mixerProgram()
{
    for (const char *name : { "pavucontrol-qt", "pavucontrol" }) {
        const QString path = QStandardPaths::findExecutable(QString::fromLatin1(name));
        if (!path.isEmpty())
            return path;
    }
    return {};
}

QString VolumeIcon::iconName(bool muted)
{
    return muted ? QStringLiteral("audio-volume-muted") : QStringLiteral("audio-volume-high");
}

void VolumeIcon::stateChanged()
{
    const bool available = m_control->isAvailable();
    setVisible(available);
    if (!available) {
        m_popup->hide();
        return;
    }
    const bool muted = m_control->isMuted();
    // The icon's shape shows the mute state; the name says it too.
    setAccessibleName(muted ? tr("Volume (muted)") : tr("Volume"));
    m_popup->setState(m_control->volume(), muted);
    update();
}

void VolumeIcon::paintEvent(QPaintEvent *)
{
    const QIcon icon = QIcon::fromTheme(iconName(m_control->isMuted()));
    QPainter painter(this);
    const QRect target(Gap / 2, (height() - IconSize) / 2, IconSize, IconSize);
    icon.paint(&painter, target);
}

void VolumeIcon::showPopup()
{
    QWidget *bar = window();
    const auto *taskbar = qobject_cast<const Taskbar *>(bar);
    const TaskbarEdge edge = taskbar ? taskbar->edge() : TaskbarEdge::Bottom;
    m_popup->adjustSize();
    const QRect icon(mapTo(bar, QPoint()), size());
    const QPoint local = volumePopupPosition(edge, bar->rect(), icon, m_popup->size());
    m_popup->move(bar->mapToGlobal(local));
    m_popup->show();
    m_popup->setFocus(Qt::PopupFocusReason);
}

void VolumeIcon::openMixer()
{
    const QString program = mixerProgram();
    if (!program.isEmpty())
        QProcess::startDetached(program, {});
}

void VolumeIcon::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_doubleClicked) {
        m_doubleClicked = false;
        return;
    }
    if (rect().contains(event->position().toPoint()) && !m_clickTimer.isActive())
        m_clickTimer.start(QApplication::doubleClickInterval());
}

void VolumeIcon::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_clickTimer.stop();
    m_doubleClicked = true;
    openMixer();
}

void VolumeIcon::contextMenuEvent(QContextMenuEvent *event)
{
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QAction *open = menu->addAction(tr("&Open Volume Control"), this, &VolumeIcon::openMixer);
    open->setEnabled(!mixerProgram().isEmpty());
    // The double-click action, in bold as NT shows a default item.
    QFont bold = open->font();
    bold.setBold(true);
    open->setFont(bold);
    menu->addAction(tr("&Adjust Volume"), this, &VolumeIcon::showPopup);
    menu->popup(event->globalPos());
}

} // namespace Daytona
