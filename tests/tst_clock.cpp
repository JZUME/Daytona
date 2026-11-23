// SPDX-License-Identifier: GPL-3.0-or-later

#include "clock.h"
#include "notificationarea.h"

#include <QTest>

using Daytona::Clock;

class TestClock : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void msUntilNextMinute_data()
    {
        QTest::addColumn<QTime>("now");
        QTest::addColumn<int>("expected");
        QTest::newRow("exactly on the minute") << QTime(10, 0, 0, 0) << 60000;
        QTest::newRow("one ms in") << QTime(10, 0, 0, 1) << 59999;
        QTest::newRow("half way") << QTime(10, 0, 30, 0) << 30000;
        QTest::newRow("last ms") << QTime(10, 0, 59, 999) << 1;
        QTest::newRow("before midnight") << QTime(23, 59, 59, 500) << 500;
    }

    void msUntilNextMinute()
    {
        QFETCH(QTime, now);
        QFETCH(int, expected);
        const int ms = Clock::msUntilNextMinute(now);
        QCOMPARE(ms, expected);
        QVERIFY(ms > 0 && ms <= 60000);
    }

    void textFollowsLocale()
    {
        const QDateTime when(QDate(1996, 7, 31), QTime(14, 5));
        QCOMPARE(Clock::timeText(when, QLocale(QLocale::English, QLocale::UnitedStates)),
            QStringLiteral("2:05 PM"));
        QCOMPARE(Clock::timeText(when, QLocale(QLocale::German, QLocale::Germany)),
            QStringLiteral("14:05"));
        QVERIFY(Clock::dateText(when, QLocale(QLocale::English, QLocale::UnitedStates))
                .contains(QStringLiteral("July")));
        QVERIFY(Clock::dateText(when, QLocale(QLocale::English, QLocale::UnitedStates))
                .contains(QStringLiteral("1996")));
    }

    void noSeconds_data()
    {
        QTest::addColumn<QLocale>("locale");
        QTest::newRow("C") << QLocale::c();
        QTest::newRow("en_US") << QLocale(QLocale::English, QLocale::UnitedStates);
        QTest::newRow("en_GB") << QLocale(QLocale::English, QLocale::UnitedKingdom);
        QTest::newRow("de_DE") << QLocale(QLocale::German, QLocale::Germany);
        QTest::newRow("ja_JP") << QLocale(QLocale::Japanese, QLocale::Japan);
    }

    void noSeconds()
    {
        QFETCH(QLocale, locale);
        const QString format = Clock::timeFormat(locale);
        QVERIFY2(!format.contains(QLatin1Char('s')), qPrintable(format));
        QVERIFY2(format.contains(QLatin1Char('m')), qPrintable(format));
        const QDateTime when(QDate(1996, 7, 31), QTime(14, 5, 37));
        QVERIFY(!Clock::timeText(when, locale).contains(QStringLiteral("37")));
    }

    void showsTimeAndDate()
    {
        Clock clock;
        QVERIFY(!clock.text().isEmpty());
        QVERIFY(!clock.toolTip().isEmpty());
        QVERIFY(clock.sizeHint().width() > clock.fontMetrics().horizontalAdvance(clock.text()));
    }

    void clockIsRightmostInNotificationArea()
    {
        Daytona::NotificationArea area;
        QVERIFY(area.clock());
        QCOMPARE(area.clock()->parentWidget(), &area);
    }
};

QTEST_MAIN(TestClock)
#include "tst_clock.moc"
