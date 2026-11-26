// SPDX-License-Identifier: GPL-3.0-or-later

#include "startbutton.h"

#include "systemlogo.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcStartButton, "daytona.startbutton", QtInfoMsg)

namespace Daytona {

namespace {
    constexpr int ButtonHeight = 22;
    constexpr int LogoSize = 16;
    constexpr int LogoTextGap = 3;
    constexpr int SidePadding = 4; // inside the 2 pixel bevel, each side
} // namespace

StartButton::StartButton(QWidget *parent)
    : QPushButton(parent)
{
    setCheckable(true);
    setAutoDefault(false);
    // labwc gives the taskbar the keyboard only when it is clicked, so the
    // taskbar cannot be operated from the keyboard the way NT's could; take
    // no focus, so a click does not leave a stray focus rectangle.
    setFocusPolicy(Qt::NoFocus);
    setText(tr("Start"));
    setAccessibleName(tr("Start"));

    QFont bold = font();
    bold.setBold(true);
    setFont(bold);

    // The operating system's own logo, as NT's Start button showed NT's.
    m_systemLogo = systemLogo();
    setUseSystemLogo(true);
    setIconSize(QSize(LogoSize, LogoSize));
    setFixedHeight(ButtonHeight);

    connect(this, &QPushButton::toggled, this, &StartButton::startMenuRequested);
}

void StartButton::setUseSystemLogo(bool use)
{
    // Daytona's when the system has no logo or it is turned off.
    QIcon logo = use ? m_systemLogo : QIcon();
    if (logo.isNull()) {
        logo = daytonaLogo();
        if (logo.pixmap(LogoSize).isNull())
            qCWarning(lcStartButton)
                << "Start logo could not be loaded; is qt6-svg-plugins installed?";
    }
    setIcon(logo);
}

QIcon StartButton::daytonaLogo()
{
    return QIcon(QStringLiteral(":/daytona/start-logo.svg"));
}

QSize StartButton::sizeHint() const
{
    // NT sizes the Start button to its contents rather than to a standard
    // button width.
    const int textWidth = fontMetrics().horizontalAdvance(text());
    return { 2 + SidePadding + LogoSize + LogoTextGap + textWidth + SidePadding + 2, ButtonHeight };
}

} // namespace Daytona
