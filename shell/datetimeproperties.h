// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QDialog>
#include <QTime>
#include <QTimer>
#include <functional>

class QCalendarWidget;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTimeEdit;

namespace Daytona {

class TimeDate;

// NT's analogue clock face: hour marks round the edge and three hands.
class AnalogueClock : public QWidget
{
    Q_OBJECT

public:
    explicit AnalogueClock(QWidget *parent = nullptr);

    QTime time() const { return m_time; }
    void setTime(const QTime &time);

    QSize sizeHint() const override { return { 120, 120 }; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTime m_time;
};

// NT's Date/Time Properties: the Date & Time tab (a month calendar and the
// clock, plus Windows 2000's network time check box) and the Time Zone tab
// (a plain list in place of NT's world map). Changes go to systemd-timedated,
// which asks for an administrator's password through the polkit agent.
class DateTimePropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DateTimePropertiesDialog(TimeDate *timeDate, QWidget *parent = nullptr);

    // Reads the current date, time, time zone and network time setting.
    void prepare();

    // "(GMT+01:00) Europe/Paris": a zone's standard (winter) offset in the
    // year of a moment, then its name.
    static QString zoneText(const QString &zone, const QDateTime &when);
    // The locale's long time format with seconds but without a time zone.
    static QString timeFormat(const QLocale &locale);

    QComboBox *monthBox() const { return m_month; }
    QSpinBox *yearBox() const { return m_year; }
    QCalendarWidget *calendar() const { return m_calendar; }
    QTimeEdit *timeEdit() const { return m_timeEdit; }
    AnalogueClock *clockFace() const { return m_clock; }
    QCheckBox *ntpBox() const { return m_ntp; }
    QListWidget *zoneList() const { return m_zones; }
    QLabel *currentZoneLabel() const { return m_currentZone; }
    QPushButton *applyButton() const { return m_apply; }
    QString selectedZone() const;
    void selectZone(const QString &zone);
    // The date and time shown, when changed by hand.
    bool timeEdited() const { return m_timeEdited; }
    QDateTime editedDateTime() const;

    // How errors are shown; a message box by default (replaced in tests).
    std::function<void(const QString &message)> showError;

public Q_SLOTS:
    void accept() override;
    // Sends the changes to timedated, one after another, stopping at (and
    // showing) the first it refuses. OK closes the dialog once they are done.
    void apply();

Q_SIGNALS:
    // The date, time or time zone was changed.
    void applied();

protected:
    void hideEvent(QHideEvent *event) override;

private:
    void tick();
    void showDate(const QDate &date);
    void dateEdited(const QDate &date);
    void updateEnabled();
    void updateCurrentZone();
    void syncFromService();
    void changed();
    void runSteps(QList<std::function<void(const std::function<void(const QString &)> &)>> steps,
        const std::function<void(bool)> &finished);
    void applyThen(const std::function<void(bool)> &finished);

    TimeDate *m_timeDate;
    QComboBox *m_month;
    QSpinBox *m_year;
    QCalendarWidget *m_calendar;
    QGroupBox *m_dateGroup;
    QGroupBox *m_timeGroup;
    AnalogueClock *m_clock;
    QTimeEdit *m_timeEdit;
    QCheckBox *m_ntp;
    QLabel *m_currentZone;
    QListWidget *m_zones;
    QPushButton *m_ok;
    QPushButton *m_apply;
    QTimer m_ticker;
    bool m_updating = false;
    bool m_timeEdited = false;
    bool m_zoneChanged = false;
    bool m_ntpChanged = false;
    bool m_busy = false;
};

} // namespace Daytona
