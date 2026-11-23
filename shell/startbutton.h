// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPushButton>

namespace Daytona {

// The NT Start button: the Daytona logo and bold "Start", raised normally
// and pressed while the Start menu is open.
class StartButton : public QPushButton
{
    Q_OBJECT

public:
    explicit StartButton(QWidget *parent = nullptr);

    QSize sizeHint() const override;

Q_SIGNALS:
    // Emitted when the button is pressed or released, by the user or by
    // toggle() (e.g. from the Start menu key).
    void startMenuRequested(bool open);
};

} // namespace Daytona
