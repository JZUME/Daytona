// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QTimer>
#include <QWidget>

namespace Daytona {

class VolumeControl;
class VolumePopup;

// The speaker in the notification area, as NT's sndvol32 put there. It
// shows whether the default output is muted. Clicking it opens the volume
// pop-up; double-clicking it, or Open Volume Control on its menu, opens the
// installed mixer. It is hidden while there is no sound output.
class VolumeIcon : public QWidget
{
    Q_OBJECT

public:
    explicit VolumeIcon(VolumeControl *control, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    VolumePopup *popup() const { return m_popup; }

    // The first installed mixer of those Daytona knows (pavucontrol-qt,
    // then pavucontrol), or an empty string.
    static QString mixerProgram();

    // The icon name for the mute state.
    static QString iconName(bool muted);

public Q_SLOTS:
    void showPopup();
    void openMixer();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void stateChanged();

    VolumeControl *m_control;
    VolumePopup *m_popup;
    // A click waits out the double-click time before opening the pop-up, so
    // a double-click opens only the mixer, as in NT.
    QTimer m_clickTimer;
    // The release that ends a double-click is not another click.
    bool m_doubleClicked = false;
};

} // namespace Daytona
