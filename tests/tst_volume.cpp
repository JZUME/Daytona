// SPDX-License-Identifier: GPL-3.0-or-later

#include "notificationarea.h"
#include "volumecontrol.h"
#include "volumeicon.h"
#include "volumepopup.h"

#include <QApplication>
#include <QCheckBox>
#include <QSignalSpy>
#include <QSlider>
#include <QTest>

using Daytona::TaskbarEdge;
using Daytona::VolumeControl;
using Daytona::VolumePopup;

class TestVolume : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void percentConversion_data()
    {
        QTest::addColumn<quint32>("volume");
        QTest::addColumn<int>("percent");
        // PA_VOLUME_NORM is 0x10000.
        QTest::newRow("silent") << quint32(0) << 0;
        QTest::newRow("full") << quint32(0x10000) << 100;
        QTest::newRow("half") << quint32(0x8000) << 50;
        QTest::newRow("amplified") << quint32(0x18000) << 150;
    }

    void percentConversion()
    {
        QFETCH(quint32, volume);
        QFETCH(int, percent);
        QCOMPARE(VolumeControl::percentFromVolume(volume), percent);
        if (percent <= 100)
            QCOMPARE(VolumeControl::volumeFromPercent(percent), volume);
    }

    void percentRoundTrips()
    {
        for (int percent = 0; percent <= 100; ++percent)
            QCOMPARE(VolumeControl::percentFromVolume(VolumeControl::volumeFromPercent(percent)),
                percent);
        // Out of range requests are clamped.
        QCOMPARE(VolumeControl::volumeFromPercent(-5), 0u);
        QCOMPARE(VolumeControl::volumeFromPercent(150), 0x10000u);
    }

    void popupPosition_data()
    {
        QTest::addColumn<TaskbarEdge>("edge");
        QTest::addColumn<QRect>("taskbar");
        QTest::addColumn<QRect>("icon");
        QTest::addColumn<QPoint>("expected");
        // The pop-up is 60x140. A 1024 wide bottom taskbar, the icon near the right end: centred
        // on it would pass the screen edge, so the pop-up stops there.
        QTest::newRow("bottom, near the end") << TaskbarEdge::Bottom << QRect(0, 0, 1024, 28)
                                              << QRect(990, 6, 18, 16) << QPoint(964, -140);
        QTest::newRow("bottom, room to centre") << TaskbarEdge::Bottom << QRect(0, 0, 1024, 28)
                                                << QRect(491, 6, 18, 16) << QPoint(469, -140);
        QTest::newRow("top") << TaskbarEdge::Top << QRect(0, 0, 1024, 28) << QRect(990, 6, 18, 16)
                             << QPoint(964, 28);
        // A vertical taskbar: beside it, kept within its height.
        QTest::newRow("left") << TaskbarEdge::Left << QRect(0, 0, 60, 768) << QRect(20, 720, 18, 16)
                              << QPoint(60, 628);
        QTest::newRow("right") << TaskbarEdge::Right << QRect(0, 0, 60, 768)
                               << QRect(20, 720, 18, 16) << QPoint(-60, 628);
    }

    void popupPosition()
    {
        QFETCH(TaskbarEdge, edge);
        QFETCH(QRect, taskbar);
        QFETCH(QRect, icon);
        QFETCH(QPoint, expected);
        QCOMPARE(Daytona::volumePopupPosition(edge, taskbar, icon, QSize(60, 140)), expected);
    }

    void popupReportsRequests()
    {
        VolumePopup popup;
        QSignalSpy volume(&popup, &VolumePopup::volumeRequested);
        QSignalSpy mute(&popup, &VolumePopup::muteRequested);

        // Reported state is shown without being requested back.
        popup.setState(40, true);
        QCOMPARE(popup.slider()->value(), 40);
        QVERIFY(popup.muteBox()->isChecked());
        QCOMPARE(volume.count(), 0);
        QCOMPARE(mute.count(), 0);

        // The user's changes are requested.
        popup.slider()->setValue(70);
        QCOMPARE(volume.count(), 1);
        QCOMPARE(volume.takeFirst().at(0).toInt(), 70);
        popup.muteBox()->click();
        QCOMPARE(mute.count(), 1);
        QCOMPARE(mute.takeFirst().at(0).toBool(), false);

        // Keyboard: Page Up raises by a page.
        popup.slider()->setValue(50);
        volume.clear();
        QTest::keyClick(popup.slider(), Qt::Key_PageUp);
        QCOMPARE(popup.slider()->value(), 60);
        QCOMPARE(volume.count(), 1);
    }

    void popupKeepsDraggedValue()
    {
        VolumePopup popup;
        popup.setState(30, false);
        popup.slider()->setSliderDown(true);
        popup.setState(80, true);
        QCOMPARE(popup.slider()->value(), 30);
        // The mute state still follows.
        QVERIFY(popup.muteBox()->isChecked());
        popup.slider()->setSliderDown(false);
    }

    void iconBesideClock()
    {
        // Without a sound server the icon is hidden, but it sits between the
        // tray icons and the clock.
        VolumeControl control;
        Daytona::NotificationArea area;
        area.setVolumeControl(&control);
        QVERIFY(area.volumeIcon());
        QVERIFY(area.volumeIcon()->isHidden());
        const QList<QWidget *> children = area.findChildren<QWidget *>(Qt::FindDirectChildrenOnly);
        QVERIFY(children.contains(area.volumeIcon()));
    }

    void clickOpensPopupDoubleClickDoesNot()
    {
        VolumeControl control;
        Daytona::VolumeIcon icon(&control);
        icon.resize(icon.sizeHint());
        QTest::mouseClick(&icon, Qt::LeftButton);
        QTRY_VERIFY(icon.popup()->isVisible());
        icon.popup()->hide();

        // Press, release, double-click press, release: only the mixer, never
        // the pop-up. No mixer can be found, so none is started.
        qputenv("PATH", "");
        QVERIFY(Daytona::VolumeIcon::mixerProgram().isEmpty());
        QTest::mouseClick(&icon, Qt::LeftButton);
        QTest::mouseDClick(&icon, Qt::LeftButton);
        QTest::mouseRelease(&icon, Qt::LeftButton);
        QTest::qWait(QApplication::doubleClickInterval() + 100);
        QVERIFY(!icon.popup()->isVisible());
    }

    void iconNames()
    {
        QCOMPARE(Daytona::VolumeIcon::iconName(false), QStringLiteral("audio-volume-high"));
        QCOMPARE(Daytona::VolumeIcon::iconName(true), QStringLiteral("audio-volume-muted"));
    }
};

QTEST_MAIN(TestVolume)
#include "tst_volume.moc"
