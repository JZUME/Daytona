// SPDX-License-Identifier: GPL-3.0-or-later

#include "clock.h"

#include <QRegularExpression>

namespace Daytona {

namespace {
    constexpr int MsPerMinute = 60 * 1000;
    constexpr int HorizontalPadding = 6; // either side of the text, inside the area
} // namespace

Clock::Clock(QWidget *parent)
    : QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    setAccessibleName(tr("Clock"));
    m_timer.setSingleShot(true);
    // A single-shot timer re-armed for the next whole minute each time it
    // fires: the shell wakes once a minute, never more. If the system clock
    // jumps or the machine resumes from suspend, the next tick lands late or
    // early once and then re-aligns.
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &Clock::update);
    update();
}

int Clock::msUntilNextMinute(const QTime &now)
{
    const int intoMinute = now.second() * 1000 + now.msec();
    return MsPerMinute - intoMinute;
}

QString Clock::timeFormat(const QLocale &locale)
{
    // NT's clock shows hours and minutes only. Most locales' short format
    // already does; some (such as the C locale) include seconds, so remove
    // them along with their separator.
    static const QRegularExpression seconds(QStringLiteral("[^hHmapAP\\s]?s{1,2}"));
    QString format = locale.timeFormat(QLocale::ShortFormat);
    format.remove(seconds);
    return format.trimmed();
}

QString Clock::timeText(const QDateTime &when, const QLocale &locale)
{
    return locale.toString(when.time(), timeFormat(locale));
}

QString Clock::dateText(const QDateTime &when, const QLocale &locale)
{
    return locale.toString(when.date(), QLocale::LongFormat);
}

QSize Clock::sizeHint() const
{
    // Wide enough for the widest time of day, so the taskbar does not shift
    // as the minutes change.
    const QLocale locale = QLocale::system();
    int widest = 0;
    for (const QTime sample : { QTime(23, 58), QTime(12, 58), QTime(10, 58) })
        widest = qMax(
            widest, fontMetrics().horizontalAdvance(locale.toString(sample, QLocale::ShortFormat)));
    return { widest + 2 * HorizontalPadding, QLabel::sizeHint().height() };
}

void Clock::update()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QLocale locale = QLocale::system();
    setText(timeText(now, locale));
    setToolTip(dateText(now, locale));
    m_timer.start(msUntilNextMinute(now.time()));
}

} // namespace Daytona
