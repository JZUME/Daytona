// SPDX-License-Identifier: GPL-3.0-or-later

#include "volumepopup.h"

#include "taskbar.h"

#include <QCheckBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace Daytona {

VolumePopup::VolumePopup(QWidget *parent)
    : QFrame(parent, Qt::Popup)
{
    setFrameStyle(QFrame::WinPanel | QFrame::Raised);
    setAccessibleName(tr("Volume"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);

    auto *label = new QLabel(tr("Volume"), this);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    m_slider = new QSlider(Qt::Vertical, this);
    m_slider->setRange(0, 100);
    m_slider->setSingleStep(2);
    m_slider->setPageStep(10);
    m_slider->setTickPosition(QSlider::TicksBothSides);
    m_slider->setTickInterval(10);
    m_slider->setFixedHeight(90);
    m_slider->setAccessibleName(tr("Volume"));
    label->setBuddy(m_slider);
    layout->addWidget(m_slider, 0, Qt::AlignHCenter);

    m_mute = new QCheckBox(tr("&Mute"), this);
    layout->addWidget(m_mute, 0, Qt::AlignHCenter);

    connect(m_slider, &QSlider::valueChanged, this, &VolumePopup::volumeRequested);
    connect(m_mute, &QCheckBox::toggled, this, &VolumePopup::muteRequested);
    setFocusProxy(m_slider);
}

void VolumePopup::setState(int percent, bool muted)
{
    if (!m_slider->isSliderDown()) {
        const QSignalBlocker blocker(m_slider);
        m_slider->setValue(percent);
    }
    const QSignalBlocker blocker(m_mute);
    m_mute->setChecked(muted);
}

QPoint volumePopupPosition(
    TaskbarEdge edge, const QRect &taskbar, const QRect &icon, const QSize &popup)
{
    const QPoint centre = icon.center();
    // Along the taskbar: centred on the icon, but not past either end of the
    // taskbar (which spans the screen). The start wins if the pop-up is
    // longer than the taskbar.
    const int x = qMax(
        taskbar.left(), qMin(centre.x() - popup.width() / 2, taskbar.right() + 1 - popup.width()));
    const int y = qMax(taskbar.top(),
        qMin(centre.y() - popup.height() / 2, taskbar.bottom() + 1 - popup.height()));
    switch (edge) {
    case TaskbarEdge::Bottom:
        return { x, taskbar.top() - popup.height() };
    case TaskbarEdge::Top:
        return { x, taskbar.bottom() + 1 };
    case TaskbarEdge::Left:
        return { taskbar.right() + 1, y };
    case TaskbarEdge::Right:
        return { taskbar.left() - popup.width(), y };
    }
    return {};
}

void popUpBesideTrayIcon(QWidget *popup, QWidget *icon)
{
    QWidget *bar = icon->window();
    const auto *taskbar = qobject_cast<const Taskbar *>(bar);
    const TaskbarEdge edge = taskbar ? taskbar->edge() : TaskbarEdge::Bottom;
    const QRect rect(icon->mapTo(bar, QPoint()), icon->size());
    popup->adjustSize();
    popup->move(bar->mapToGlobal(volumePopupPosition(edge, bar->rect(), rect, popup->sizeHint())));
    popup->show();
}

} // namespace Daytona
