// SPDX-License-Identifier: GPL-3.0-or-later

#include "datetimeproperties.h"

#include "timedate.h"

#include <QCalendarWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QTimeEdit>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>
#include <qdrawutil.h>

namespace Daytona {

namespace {

    constexpr int ZoneRole = Qt::UserRole;
    constexpr int FirstYear = 1970;
    constexpr int LastYear = 2099;

    // A point on the clock face: a fraction of the radius out from the
    // centre, at an angle in degrees clockwise from twelve o'clock.
    QPointF onFace(const QPointF &centre, qreal radius, qreal degrees)
    {
        const qreal radians = qDegreesToRadians(degrees);
        return centre + QPointF(radius * qSin(radians), -radius * qCos(radians));
    }

    // A hand: a long, thin diamond from just behind the centre.
    void drawHand(QPainter &p, const QPointF &centre, qreal length, qreal width, qreal degrees)
    {
        const QPolygonF hand { onFace(centre, -length * 0.15, degrees),
            onFace(centre, width, degrees + 90), onFace(centre, length, degrees),
            onFace(centre, width, degrees - 90) };
        p.drawPolygon(hand);
    }

    // A zone's standard offset from GMT in the year of a moment: the smaller
    // of its winter and summer offsets. (tzdata gives Ireland a negative
    // daylight saving time, so Qt's standard offset there is its summer one.)
    int standardOffset(const QString &zone, const QDateTime &when)
    {
        const QTimeZone tz(zone.toUtf8());
        if (!tz.isValid())
            return 0;
        const int year = when.date().year();
        return qMin(tz.offsetFromUtc(QDateTime(QDate(year, 1, 1), QTime(12, 0), QTimeZone::UTC)),
            tz.offsetFromUtc(QDateTime(QDate(year, 7, 1), QTime(12, 0), QTimeZone::UTC)));
    }

} // namespace

AnalogueClock::AnalogueClock(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(100, 100);
    setAccessibleName(tr("Clock"));
}

void AnalogueClock::setTime(const QTime &time)
{
    if (time == m_time)
        return;
    m_time = time;
    update();
}

void AnalogueClock::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const int side = qMin(width(), height());
    const QPointF centre = QRectF(rect()).center();
    const qreal radius = side / 2.0 - 2;

    // Minute dots, and raised hour marks, as NT drew them.
    const int mark = qMax(3, qRound(radius / 12));
    for (int i = 0; i < 60; ++i) {
        const QPointF at = onFace(centre, radius - mark, i * 6);
        if (i % 5 == 0) {
            const QRect square(
                qRound(at.x() - mark / 2.0), qRound(at.y() - mark / 2.0), mark, mark);
            qDrawShadePanel(&p, square, palette(), false, 1, &palette().brush(QPalette::Button));
        } else {
            p.fillRect(QRectF(at.x() - 0.5, at.y() - 0.5, 1, 1), palette().color(QPalette::Dark));
        }
    }
    if (!m_time.isValid())
        return;

    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(palette().color(QPalette::Shadow), 1));
    p.setBrush(palette().color(QPalette::Highlight));
    const qreal hours = (m_time.hour() % 12) + m_time.minute() / 60.0;
    drawHand(p, centre, radius * 0.5, radius * 0.08, hours * 30);
    const qreal minutes = m_time.minute() + m_time.second() / 60.0;
    drawHand(p, centre, radius * 0.75, radius * 0.07, minutes * 6);
    p.setPen(QPen(palette().color(QPalette::WindowText), 1));
    p.drawLine(centre, onFace(centre, radius * 0.8, m_time.second() * 6));
}

DateTimePropertiesDialog::DateTimePropertiesDialog(TimeDate *timeDate, QWidget *parent)
    : QDialog(parent)
    , m_timeDate(timeDate)
{
    setWindowTitle(tr("Date/Time Properties"));
    showError
        = [this](const QString &message) { QMessageBox::critical(this, windowTitle(), message); };
    auto *tabs = new QTabWidget(this);

    // Date & Time: the month and year above a month calendar.
    auto *datePage = new QWidget;
    m_dateGroup = new QGroupBox(tr("Date"), datePage);
    m_month = new QComboBox(m_dateGroup);
    for (int month = 1; month <= 12; ++month)
        m_month->addItem(QLocale::system().standaloneMonthName(month), month);
    m_year = new QSpinBox(m_dateGroup);
    m_year->setRange(FirstYear, LastYear);
    m_calendar = new QCalendarWidget(m_dateGroup);
    m_calendar->setNavigationBarVisible(false);
    m_calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    m_calendar->setHorizontalHeaderFormat(QCalendarWidget::SingleLetterDayNames);
    m_calendar->setDateRange(QDate(FirstYear, 1, 1), QDate(LastYear, 12, 31));
    // NT's calendar drew every day alike; Qt's shows weekends in red.
    m_calendar->setWeekdayTextFormat(Qt::Saturday, QTextCharFormat());
    m_calendar->setWeekdayTextFormat(Qt::Sunday, QTextCharFormat());
    auto *monthYear = new QHBoxLayout;
    monthYear->addWidget(m_month, 1);
    monthYear->addWidget(m_year);
    auto *dateLayout = new QVBoxLayout(m_dateGroup);
    dateLayout->addLayout(monthYear);
    dateLayout->addWidget(m_calendar, 1);

    connect(m_month, &QComboBox::currentIndexChanged, this, [this] {
        if (m_updating)
            return;
        const QDate shown = m_calendar->selectedDate();
        const int month = m_month->currentData().toInt();
        const int day = qMin(shown.day(), QDate(shown.year(), month, 1).daysInMonth());
        dateEdited(QDate(shown.year(), month, day));
    });
    connect(m_year, &QSpinBox::valueChanged, this, [this](int year) {
        if (m_updating)
            return;
        const QDate shown = m_calendar->selectedDate();
        const int day = qMin(shown.day(), QDate(year, shown.month(), 1).daysInMonth());
        dateEdited(QDate(year, shown.month(), day));
    });
    connect(m_calendar, &QCalendarWidget::selectionChanged, this, [this] {
        if (!m_updating)
            dateEdited(m_calendar->selectedDate());
    });

    // The clock face over the time.
    m_timeGroup = new QGroupBox(tr("Time"), datePage);
    m_clock = new AnalogueClock(m_timeGroup);
    m_timeEdit = new QTimeEdit(m_timeGroup);
    m_timeEdit->setDisplayFormat(timeFormat(QLocale::system()));
    connect(m_timeEdit, &QTimeEdit::timeChanged, this, [this](QTime time) {
        m_clock->setTime(time);
        if (m_updating)
            return;
        m_timeEdited = true;
        changed();
    });
    auto *timeLayout = new QVBoxLayout(m_timeGroup);
    timeLayout->addWidget(m_clock, 1);
    timeLayout->addWidget(m_timeEdit);

    // Windows 2000 and XP's Internet time, as one check box: network time
    // through timedated (systemd-timesyncd or another NTP service).
    m_ntp = new QCheckBox(tr("A&utomatically synchronise with an Internet time server"), datePage);
    connect(m_ntp, &QCheckBox::toggled, this, [this] {
        updateEnabled();
        if (m_updating)
            return;
        m_ntpChanged = true;
        changed();
    });
    m_currentZone = new QLabel(datePage);

    auto *groups = new QHBoxLayout;
    groups->addWidget(m_dateGroup, 3);
    groups->addWidget(m_timeGroup, 2);
    auto *dateTimeLayout = new QVBoxLayout(datePage);
    dateTimeLayout->addLayout(groups, 1);
    dateTimeLayout->addWidget(m_ntp);
    dateTimeLayout->addWidget(m_currentZone);
    tabs->addTab(datePage, tr("Date && Time"));

    // Time Zone: a list in place of NT's world map.
    auto *zonePage = new QWidget;
    m_zones = new QListWidget(zonePage);
    m_zones->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_zones, &QListWidget::currentItemChanged, this, [this] {
        if (m_updating)
            return;
        m_zoneChanged = true;
        changed();
    });
    auto *zoneLayout = new QVBoxLayout(zonePage);
    zoneLayout->addWidget(m_zones);
    tabs->addTab(zonePage, tr("Time Zone"));
    // The chosen zone in view whenever the tab is shown.
    // (Once laid out: the list has no size before it is first shown.)
    connect(tabs, &QTabWidget::currentChanged, this, [this, zonePage](int) {
        QTimer::singleShot(0, this, [this, zonePage] {
            if (QListWidgetItem *item = m_zones->currentItem(); item && zonePage->isVisible())
                m_zones->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        });
    });

    m_ok = new QPushButton(tr("OK"), this);
    m_ok->setDefault(true);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    m_apply = new QPushButton(tr("&Apply"), this);
    for (QPushButton *b : { m_ok, cancel, m_apply })
        b->setMinimumSize(75, 23);
    connect(m_ok, &QPushButton::clicked, this, &DateTimePropertiesDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &DateTimePropertiesDialog::reject);
    connect(m_apply, &QPushButton::clicked, this, &DateTimePropertiesDialog::apply);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(m_ok);
    buttons->addWidget(cancel);
    buttons->addWidget(m_apply);

    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(6, 6, 6, 6);
    main->addWidget(tabs);
    main->addLayout(buttons);

    // The clock runs while nothing has been changed by hand.
    m_ticker.setInterval(1000);
    connect(&m_ticker, &QTimer::timeout, this, &DateTimePropertiesDialog::tick);
    if (m_timeDate)
        connect(m_timeDate, &TimeDate::changed, this, &DateTimePropertiesDialog::syncFromService);
}

QString DateTimePropertiesDialog::zoneText(const QString &zone, const QDateTime &when)
{
    const int offset = standardOffset(zone, when);
    QString gmt = QStringLiteral("GMT");
    if (offset != 0) {
        const int minutes = qAbs(offset) / 60;
        gmt += QStringLiteral("%1%2:%3")
                   .arg(offset < 0 ? QLatin1Char('-') : QLatin1Char('+'))
                   .arg(minutes / 60, 2, 10, QLatin1Char('0'))
                   .arg(minutes % 60, 2, 10, QLatin1Char('0'));
    }
    QString name = zone;
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    return QStringLiteral("(%1) %2").arg(gmt, name);
}

QString DateTimePropertiesDialog::timeFormat(const QLocale &locale)
{
    // Qt's "t" is the time zone, which the dialog shows on its own line.
    static const QRegularExpression zone(QStringLiteral("\\s*t+"));
    QString format = locale.timeFormat(QLocale::LongFormat);
    format.remove(zone);
    return format.trimmed();
}

void DateTimePropertiesDialog::prepare()
{
    m_updating = true;
    m_timeEdited = m_zoneChanged = m_ntpChanged = false;
    m_busy = false;
    m_ok->setEnabled(true);

    const QDateTime now = QDateTime::currentDateTime();
    showDate(now.date());
    m_timeEdit->setTime(now.time());

    // Zones in order of their offset from GMT, as NT listed them.
    m_zones->clear();
    QStringList zones = TimeDate::availableZones();
    const QString current = m_timeDate ? m_timeDate->timeZone() : QString();
    if (!current.isEmpty() && !zones.contains(current))
        zones << current;
    QList<QPair<int, QString>> ordered;
    for (const QString &zone : zones) {
        ordered.append({ standardOffset(zone, now), zone });
    }
    std::sort(ordered.begin(), ordered.end());
    for (const auto &[offset, zone] : ordered) {
        auto *item = new QListWidgetItem(zoneText(zone, now), m_zones);
        item->setData(ZoneRole, zone);
    }
    m_updating = false;

    syncFromService();
    m_apply->setEnabled(false);
    m_ticker.start();
}

void DateTimePropertiesDialog::syncFromService()
{
    // What the system has, except where it has been changed here.
    m_updating = true;
    const bool canNtp = m_timeDate && m_timeDate->canNtp();
    m_ntp->setEnabled(canNtp);
    if (!m_ntpChanged)
        m_ntp->setChecked(canNtp && m_timeDate->ntp());
    if (!m_zoneChanged && m_timeDate)
        selectZone(m_timeDate->timeZone());
    m_updating = false;
    updateEnabled();
    updateCurrentZone();
}

void DateTimePropertiesDialog::selectZone(const QString &zone)
{
    for (int i = 0; i < m_zones->count(); ++i) {
        if (m_zones->item(i)->data(ZoneRole).toString() == zone) {
            m_zones->setCurrentRow(i);
            m_zones->scrollToItem(m_zones->item(i), QAbstractItemView::PositionAtCenter);
            return;
        }
    }
}

QString DateTimePropertiesDialog::selectedZone() const
{
    const QListWidgetItem *item = m_zones->currentItem();
    return item ? item->data(ZoneRole).toString() : QString();
}

void DateTimePropertiesDialog::updateCurrentZone()
{
    const QString zone = m_timeDate ? m_timeDate->timeZone() : QString();
    const QTimeZone tz = zone.isEmpty() ? QTimeZone::systemTimeZone() : QTimeZone(zone.toUtf8());
    const QString name
        = tz.isValid() ? tz.displayName(QDateTime::currentDateTime(), QTimeZone::LongName) : zone;
    m_currentZone->setText(tr("Current time zone: %1").arg(name));
}

void DateTimePropertiesDialog::showDate(const QDate &date)
{
    const bool wasUpdating = m_updating;
    m_updating = true;
    m_month->setCurrentIndex(date.month() - 1);
    m_year->setValue(date.year());
    m_calendar->setCurrentPage(date.year(), date.month());
    m_calendar->setSelectedDate(date);
    m_updating = wasUpdating;
}

void DateTimePropertiesDialog::dateEdited(const QDate &date)
{
    showDate(date);
    m_timeEdited = true;
    changed();
}

void DateTimePropertiesDialog::tick()
{
    if (m_timeEdited)
        return;
    const QDateTime now = QDateTime::currentDateTime();
    m_updating = true;
    if (m_calendar->selectedDate() != now.date())
        showDate(now.date());
    m_timeEdit->setTime(now.time());
    m_updating = false;
}

void DateTimePropertiesDialog::updateEnabled()
{
    // timedated will not set the clock while network time keeps it.
    const bool manual = !m_ntp->isChecked();
    m_dateGroup->setEnabled(manual);
    m_timeGroup->setEnabled(manual);
}

void DateTimePropertiesDialog::changed()
{
    if (!m_busy)
        m_apply->setEnabled(true);
}

QDateTime DateTimePropertiesDialog::editedDateTime() const
{
    // In the zone that will be the system's, so the clock shows what was set.
    const QString zone = selectedZone();
    const QTimeZone tz = zone.isEmpty() ? QTimeZone::systemTimeZone() : QTimeZone(zone.toUtf8());
    return QDateTime(m_calendar->selectedDate(), m_timeEdit->time(), tz);
}

void DateTimePropertiesDialog::runSteps(
    QList<std::function<void(const std::function<void(const QString &)> &)>> steps,
    const std::function<void(bool)> &finished)
{
    if (steps.isEmpty()) {
        finished(true);
        return;
    }
    const auto step = steps.takeFirst();
    step([this, steps, finished](const QString &error) {
        if (!error.isEmpty()) {
            showError(error);
            finished(false);
            return;
        }
        runSteps(steps, finished);
    });
}

void DateTimePropertiesDialog::apply()
{
    applyThen({});
}

void DateTimePropertiesDialog::accept()
{
    applyThen([this](bool ok) {
        if (ok)
            QDialog::accept();
    });
}

void DateTimePropertiesDialog::applyThen(const std::function<void(bool)> &then)
{
    if (m_busy)
        return;
    using Step = std::function<void(const std::function<void(const QString &)> &)>;
    QList<Step> steps;
    const auto failure = [](const QString &what) {
        return [what](const std::function<void(const QString &)> &next) {
            return [what, next](const QString &error) {
                next(error.isEmpty() ? QString() : what + QStringLiteral("\n\n") + error);
            };
        };
    };
    TimeDate *timeDate = m_timeDate;
    if (timeDate && m_ntpChanged) {
        const bool on = m_ntp->isChecked();
        steps << [timeDate, on, failure](const auto &next) {
            timeDate->setNtp(on, failure(tr("Cannot change the time synchronisation."))(next));
        };
    }
    if (timeDate && m_zoneChanged && !selectedZone().isEmpty()) {
        const QString zone = selectedZone();
        steps << [timeDate, zone, failure](const auto &next) {
            timeDate->setTimeZone(zone, failure(tr("Cannot change the time zone."))(next));
        };
    }
    if (timeDate && m_timeEdited && !m_ntp->isChecked()) {
        const qint64 when = editedDateTime().toMSecsSinceEpoch();
        steps << [timeDate, when, failure](const auto &next) {
            timeDate->setTime(when, failure(tr("Cannot change the date and time."))(next));
        };
    }
    if (steps.isEmpty()) {
        m_apply->setEnabled(false);
        if (then)
            then(true);
        return;
    }

    m_busy = true;
    m_ok->setEnabled(false);
    m_apply->setEnabled(false);
    runSteps(steps, [this, then](bool ok) {
        m_busy = false;
        m_ok->setEnabled(true);
        if (ok) {
            m_timeEdited = m_zoneChanged = m_ntpChanged = false;
            syncFromService();
            tick();
            Q_EMIT applied();
        } else {
            m_apply->setEnabled(true);
        }
        if (then)
            then(ok);
    });
}

void DateTimePropertiesDialog::hideEvent(QHideEvent *event)
{
    m_ticker.stop();
    QDialog::hideEvent(event);
}

} // namespace Daytona
