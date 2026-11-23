// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

namespace Daytona {

// A transparent full-screen layer surface placed under an open shell menu.
// Clicking anywhere outside the menu lands here and closes the menu, the
// way a click outside a Windows menu dismisses it without reaching the
// window underneath.
class ClickCatcher : public QWidget
{
    Q_OBJECT

public:
    explicit ClickCatcher(QWidget *parent = nullptr);

    // Shows the catcher on the screen of the given widget.
    void cover(const QWidget *onScreenOf);

Q_SIGNALS:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_attached = false;
};

} // namespace Daytona
