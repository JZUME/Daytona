// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QIcon>
#include <QPushButton>

namespace Daytona {

// The NT Start button: the operating system's logo (systemLogo(), or
// Daytona's own when it has none) and bold "Start", raised normally and
// pressed while the Start menu is open.
class StartButton : public QPushButton
{
    Q_OBJECT

public:
    explicit StartButton(QWidget *parent = nullptr);

    QSize sizeHint() const override;

    // Daytona's own logo, used when the system has none.
    static QIcon daytonaLogo();

    // Shows the operating system's logo (the default) or Daytona's own.
    void setUseSystemLogo(bool use);

Q_SIGNALS:
    // Emitted when the button is pressed or released, by the user or by
    // toggle() (e.g. from the Start menu key).
    void startMenuRequested(bool open);

private:
    QIcon m_systemLogo;
};

} // namespace Daytona
