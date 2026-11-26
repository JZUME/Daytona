// SPDX-License-Identifier: GPL-3.0-or-later

#include "startbutton.h"

#include <QSignalSpy>
#include <QTest>

using Daytona::StartButton;

class TestStartButton : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void looksLikeNt()
    {
        StartButton button;
        QCOMPARE(button.text(), QStringLiteral("Start"));
        QVERIFY(button.font().bold());
        QVERIFY(button.isCheckable());
        QCOMPARE(button.sizeHint().height(), 22);
        // Sized to its contents, not a 75 pixel dialog button.
        QVERIFY(button.sizeHint().width() < 75);
        QVERIFY(
            button.sizeHint().width() > 16 + button.fontMetrics().horizontalAdvance(button.text()));
    }

    void logoLoads()
    {
        StartButton button;
        QVERIFY2(
            !button.icon().pixmap(16).isNull(), "needs the Qt SVG icon plugin (qt6-svg-plugins)");
        // Daytona's own, for systems without a logo.
        QVERIFY2(!StartButton::daytonaLogo().pixmap(16).isNull(),
            "needs the Qt SVG icon plugin (qt6-svg-plugins)");
    }

    void systemLogoCanBeTurnedOff()
    {
        StartButton button;
        const QImage daytona = StartButton::daytonaLogo().pixmap(16).toImage();
        button.setUseSystemLogo(false);
        QCOMPARE(button.icon().pixmap(16).toImage(), daytona);
        button.setUseSystemLogo(true);
        QVERIFY(!button.icon().pixmap(16).isNull());
    }

    void toggleRequestsTheMenu()
    {
        StartButton button;
        QSignalSpy spy(&button, &StartButton::startMenuRequested);
        button.toggle();
        QVERIFY(button.isChecked());
        button.click();
        QVERIFY(!button.isChecked());
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
    }
};

QTEST_MAIN(TestStartButton)
#include "tst_startbutton.moc"
