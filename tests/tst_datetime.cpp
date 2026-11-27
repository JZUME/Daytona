// SPDX-License-Identifier: GPL-3.0-or-later

#include "clock.h"
#include "controlpanel.h"
#include "datetimeproperties.h"
#include "timedate.h"

#include <QCalendarWidget>
#include <QCheckBox>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeEdit>

using namespace Daytona;

// A stand-in for systemd-timedated, on its own connection to the test's
// private session bus.
class FakeTimeDate : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.timedate1")
    Q_PROPERTY(QString Timezone MEMBER timezone)
    Q_PROPERTY(bool NTP MEMBER ntp)
    Q_PROPERTY(bool CanNTP MEMBER canNtp)

public:
    QString timezone = QStringLiteral("Europe/London");
    bool ntp = true;
    bool canNtp = true;
    QString refuse; // a method to refuse, as polkit does when cancelled
    QStringList calls;
    qlonglong usec = 0;

    void propertiesChanged(const QVariantMap &changed)
    {
        QDBusMessage signal = QDBusMessage::createSignal(
            QStringLiteral("/org/freedesktop/timedate1"),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
        signal << QStringLiteral("org.freedesktop.timedate1") << changed << QStringList();
        QDBusConnection(QStringLiteral("fake-timedated")).send(signal);
    }

public Q_SLOTS:
    void SetTimezone(const QString &zone, bool interactive)
    {
        calls << QStringLiteral("SetTimezone %1 %2").arg(zone).arg(interactive);
        if (refused(QStringLiteral("SetTimezone")))
            return;
        timezone = zone;
        propertiesChanged({ { QStringLiteral("Timezone"), zone } });
    }
    void SetNTP(bool on, bool interactive)
    {
        calls << QStringLiteral("SetNTP %1 %2").arg(on).arg(interactive);
        if (refused(QStringLiteral("SetNTP")))
            return;
        ntp = on;
    }
    void SetTime(qlonglong usecUtc, bool relative, bool interactive)
    {
        calls << QStringLiteral("SetTime %1 %2").arg(relative).arg(interactive);
        if (refused(QStringLiteral("SetTime")))
            return;
        usec = usecUtc;
    }

private:
    bool refused(const QString &method)
    {
        if (refuse != method)
            return false;
        sendErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
            QStringLiteral("Access denied"));
        return true;
    }
};

class TestDateTime : public QObject
{
    Q_OBJECT

private:
    FakeTimeDate *m_fake = nullptr;
    QStringList m_errors;

    static QDBusConnection bus() { return QDBusConnection::sessionBus(); }

    void waitForCalls(int count) { QTRY_COMPARE_WITH_TIMEOUT(m_fake->calls.size(), count, 5000); }

    // A dialog on a started client that has read the fake's properties.
    struct Fixture
    {
        TimeDate timeDate { QDBusConnection::sessionBus() };
        DateTimePropertiesDialog dialog { &timeDate };
    };
    void start(Fixture &f)
    {
        QSignalSpy changed(&f.timeDate, &TimeDate::changed);
        f.timeDate.start();
        QVERIFY(changed.wait(5000));
        QVERIFY(f.timeDate.isAvailable());
        f.dialog.showError = [this](const QString &message) { m_errors << message; };
        f.dialog.prepare();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QDBusConnection service = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, QStringLiteral("fake-timedated"));
        QVERIFY(service.isConnected());
        m_fake = new FakeTimeDate;
        QVERIFY(service.registerObject(QStringLiteral("/org/freedesktop/timedate1"), m_fake,
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties));
        QVERIFY(service.registerService(QStringLiteral("org.freedesktop.timedate1")));
    }

    void init()
    {
        m_fake->timezone = QStringLiteral("Europe/London");
        m_fake->ntp = true;
        m_fake->canNtp = true;
        m_fake->refuse.clear();
        m_fake->calls.clear();
        m_errors.clear();
    }

    void availableZones()
    {
        QTemporaryDir dir;
        QFile tab(dir.filePath(QStringLiteral("zone1970.tab")));
        QVERIFY(tab.open(QIODevice::WriteOnly));
        tab.write("# comment\n"
                  "GB,GG,IM,JE\t+513030-0000731\tEurope/London\n"
                  "FR,MC\t+4852+00220\tEurope/Paris\n"
                  "US\t+404251-0740023\tAmerica/New_York\tEastern (most areas)\n");
        tab.close();
        QCOMPARE(TimeDate::availableZones(dir.path()),
            QStringList({ QStringLiteral("America/New_York"), QStringLiteral("Europe/London"),
                QStringLiteral("Europe/Paris"), QStringLiteral("UTC") }));
        // Without tzdata, UTC alone.
        QCOMPARE(TimeDate::availableZones(dir.filePath(QStringLiteral("missing"))),
            QStringList { QStringLiteral("UTC") });
    }

    void zoneText()
    {
        const QDateTime january(QDate(2026, 1, 15), QTime(12, 0), QTimeZone::UTC);
        QCOMPARE(DateTimePropertiesDialog::zoneText(QStringLiteral("UTC"), january),
            QStringLiteral("(GMT) UTC"));
        QCOMPARE(DateTimePropertiesDialog::zoneText(QStringLiteral("America/New_York"), january),
            QStringLiteral("(GMT-05:00) America/New York"));
        QCOMPARE(DateTimePropertiesDialog::zoneText(QStringLiteral("Asia/Kolkata"), january),
            QStringLiteral("(GMT+05:30) Asia/Kolkata"));
        // Whatever the season, and for Ireland's negative daylight saving.
        const QDateTime july(QDate(2026, 7, 15), QTime(12, 0), QTimeZone::UTC);
        QCOMPARE(DateTimePropertiesDialog::zoneText(QStringLiteral("Europe/Dublin"), july),
            QStringLiteral("(GMT) Europe/Dublin"));
        QCOMPARE(DateTimePropertiesDialog::zoneText(QStringLiteral("Australia/Sydney"), january),
            QStringLiteral("(GMT+10:00) Australia/Sydney"));
    }

    void timeFormatHasSecondsNoZone()
    {
        for (const QLocale &locale :
            { QLocale::c(), QLocale(QLocale::English, QLocale::UnitedStates),
                QLocale(QLocale::German, QLocale::Germany) }) {
            const QString format = DateTimePropertiesDialog::timeFormat(locale);
            QVERIFY2(format.contains(QLatin1Char('s')), qPrintable(format));
            QVERIFY2(!format.contains(QLatin1Char('t')), qPrintable(format));
        }
    }

    void controlPanelListsDateTime()
    {
        const QList<ControlPanelApplet> applets = controlPanelApplets({});
        const auto it = std::find_if(applets.begin(), applets.end(),
            [](const auto &a) { return a.builtin == ControlPanelApplet::Builtin::DateTime; });
        QVERIFY(it != applets.end());
        QCOMPARE(it->name, QStringLiteral("Date/Time"));
    }

    void clockDoubleClick()
    {
        Clock clock;
        QSignalSpy requested(&clock, &Clock::propertiesRequested);
        QTest::mouseDClick(&clock, Qt::LeftButton);
        QCOMPARE(requested.size(), 1);
    }

    void preparesFromService()
    {
        Fixture f;
        start(f);
        QCOMPARE(f.dialog.selectedZone(), QStringLiteral("Europe/London"));
        QVERIFY(f.dialog.ntpBox()->isChecked());
        QVERIFY(f.dialog.ntpBox()->isEnabled());
        // Network time keeps the clock: the date and time are not editable.
        QVERIFY(!f.dialog.calendar()->isEnabled());
        QVERIFY(!f.dialog.timeEdit()->isEnabled());
        QVERIFY(!f.dialog.applyButton()->isEnabled());
        QCOMPARE(f.dialog.calendar()->selectedDate(), QDate::currentDate());
    }

    void noNtpService()
    {
        m_fake->canNtp = false;
        m_fake->ntp = false;
        Fixture f;
        start(f);
        QVERIFY(!f.dialog.ntpBox()->isEnabled());
        QVERIFY(!f.dialog.ntpBox()->isChecked());
        QVERIFY(f.dialog.timeEdit()->isEnabled());
    }

    void changeTimeZone()
    {
        Fixture f;
        start(f);
        QSignalSpy zoneChanged(&f.timeDate, &TimeDate::timeZoneChanged);
        QSignalSpy applied(&f.dialog, &DateTimePropertiesDialog::applied);
        f.dialog.selectZone(QStringLiteral("Europe/Paris"));
        QVERIFY(f.dialog.applyButton()->isEnabled());
        f.dialog.apply();
        waitForCalls(1);
        // Interactive, so the polkit agent can ask for a password.
        QCOMPARE(m_fake->calls, QStringList { QStringLiteral("SetTimezone Europe/Paris 1") });
        QTRY_COMPARE(applied.size(), 1);
        QTRY_COMPARE(zoneChanged.size(), 1);
        QCOMPARE(zoneChanged.first().first().toString(), QStringLiteral("Europe/Paris"));
        QCOMPARE(f.timeDate.timeZone(), QStringLiteral("Europe/Paris"));
        QVERIFY(!f.dialog.applyButton()->isEnabled());
        QVERIFY(m_errors.isEmpty());
    }

    void zoneChangedElsewhere()
    {
        Fixture f;
        start(f);
        QSignalSpy zoneChanged(&f.timeDate, &TimeDate::timeZoneChanged);
        m_fake->timezone = QStringLiteral("Asia/Tokyo");
        m_fake->propertiesChanged({ { QStringLiteral("Timezone"), m_fake->timezone } });
        QVERIFY(zoneChanged.wait(5000));
        // The dialog follows, as nothing was changed in it.
        QTRY_COMPARE(f.dialog.selectedZone(), QStringLiteral("Asia/Tokyo"));
    }

    void setTimeManually()
    {
        Fixture f;
        start(f);
        // Turning network time off allows the clock to be set.
        f.dialog.ntpBox()->setChecked(false);
        QVERIFY(f.dialog.timeEdit()->isEnabled());
        const QDate date(2027, 3, 14);
        f.dialog.calendar()->setSelectedDate(date);
        f.dialog.timeEdit()->setTime(QTime(9, 26, 53));
        QVERIFY(f.dialog.timeEdited());
        f.dialog.apply();
        waitForCalls(2);
        QCOMPARE(m_fake->calls,
            QStringList({ QStringLiteral("SetNTP 0 1"), QStringLiteral("SetTime 0 1") }));
        const QDateTime expected(
            date, QTime(9, 26, 53), QTimeZone(QByteArrayLiteral("Europe/London")));
        QCOMPARE(m_fake->usec, expected.toMSecsSinceEpoch() * 1000);
        QVERIFY(!m_fake->ntp);
        QTRY_VERIFY(!f.dialog.timeEdited());
    }

    void refusedStopsAndShows()
    {
        Fixture f;
        start(f);
        m_fake->refuse = QStringLiteral("SetTimezone");
        QSignalSpy applied(&f.dialog, &DateTimePropertiesDialog::applied);
        f.dialog.selectZone(QStringLiteral("Asia/Tokyo"));
        f.dialog.ntpBox()->setChecked(false);
        f.dialog.timeEdit()->setTime(QTime(1, 2, 3));
        f.dialog.apply();
        // SetNTP, then SetTimezone is refused and SetTime is not sent.
        QTRY_COMPARE(m_errors.size(), 1);
        QCOMPARE(m_fake->calls.size(), 2);
        QVERIFY(m_errors.first().contains(QStringLiteral("time zone")));
        QCOMPARE(applied.size(), 0);
        QVERIFY(f.dialog.applyButton()->isEnabled());
    }

    void okClosesOnceApplied()
    {
        Fixture f;
        start(f);
        f.dialog.show();
        f.dialog.selectZone(QStringLiteral("America/New_York"));
        f.dialog.accept();
        QTRY_VERIFY(!f.dialog.isVisible());
        QCOMPARE(f.dialog.result(), int(QDialog::Accepted));
        QCOMPARE(m_fake->timezone, QStringLiteral("America/New_York"));
    }

    void reloadLocalTimeZone()
    {
        // TZ is put back as it was.
        qputenv("TZ", "Asia/Tokyo");
        TimeDate::reloadLocalTimeZone();
        QCOMPARE(qgetenv("TZ"), QByteArray("Asia/Tokyo"));
        qunsetenv("TZ");
        TimeDate::reloadLocalTimeZone();
        QVERIFY(!qEnvironmentVariableIsSet("TZ"));
    }
};

QTEST_MAIN(TestDateTime)
#include "tst_datetime.moc"
