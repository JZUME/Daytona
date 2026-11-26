// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings.h"

#include <QFrame>

class QCheckBox;
class QSlider;

namespace Daytona {

// NT's volume pop-up, shown by clicking the speaker in the notification
// area: "Volume", a vertical trackbar and a Mute check box. It only reports
// what the user asks for; the caller applies it and reports the result back
// through setState.
class VolumePopup : public QFrame
{
    Q_OBJECT

public:
    explicit VolumePopup(QWidget *parent = nullptr);

    // The current volume (0 to 100) and mute state. While the user drags the
    // trackbar, the volume is left alone so the thumb does not jump back.
    void setState(int percent, bool muted);

    QSlider *slider() const { return m_slider; }
    QCheckBox *muteBox() const { return m_mute; }

Q_SIGNALS:
    void volumeRequested(int percent);
    void muteRequested(bool muted);

private:
    QSlider *m_slider;
    QCheckBox *m_mute;
};

// Where the pop-up goes for a speaker icon on a taskbar at edge, all in the
// taskbar's coordinates: beside the taskbar towards the middle of the
// screen, centred on the icon and kept within the taskbar's length.
QPoint volumePopupPosition(
    TaskbarEdge edge, const QRect &taskbar, const QRect &icon, const QSize &popup);

// Shows popup (a menu or pop-up window) beside the taskbar at icon, a widget
// in the notification area, as NT's tray menus and the volume pop-up open.
// Qt's own popup() is not used: it keeps a menu on the screen as Qt sees it,
// where a layer-shell taskbar is at 0,0, so the place above the taskbar looks
// off the screen, though it is right for the compositor.
void popUpBesideTrayIcon(QWidget *popup, QWidget *icon);

} // namespace Daytona
