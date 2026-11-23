// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QLabel>
#include <QLocale>
#include <QTimer>

namespace Daytona {

// The taskbar clock: the time in the locale's short format, updated exactly
// on each minute, with the long date as its tooltip.
class Clock : public QLabel
{
    Q_OBJECT

public:
    explicit Clock(QWidget *parent = nullptr);

    // Milliseconds from now until the next whole minute, in (0, 60000].
    static int msUntilNextMinute(const QTime &now);

    // The locale's short time format without seconds.
    static QString timeFormat(const QLocale &locale);
    static QString timeText(const QDateTime &when, const QLocale &locale);
    static QString dateText(const QDateTime &when, const QLocale &locale);

    QSize sizeHint() const override;

private:
    void update();

    QTimer m_timer;
};

} // namespace Daytona
