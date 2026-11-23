// SPDX-License-Identifier: GPL-3.0-or-later

#include "clock.h"
#include "notificationarea.h"
#include "settings.h"
#include "startbutton.h"
#include "taskbar.h"
#include "taskbarplacement.h"
#include "taskbarproperties.h"
#include "tasklist.h"

#include <QApplication>
#include <QCheckBox>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

Q_DECLARE_METATYPE(Daytona::TaskbarEdge)

class TestTaskbarPlacement : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nearest_data()
    {
        QTest::addColumn<QPoint>("point");
        QTest::addColumn<TaskbarEdge>("edge");
        // 1024x768 screen.
        QTest::newRow("bottom middle") << QPoint(512, 760) << TaskbarEdge::Bottom;
        QTest::newRow("just below centre") << QPoint(512, 400) << TaskbarEdge::Bottom;
        QTest::newRow("top middle") << QPoint(512, 5) << TaskbarEdge::Top;
        QTest::newRow("left middle") << QPoint(5, 384) << TaskbarEdge::Left;
        QTest::newRow("right middle") << QPoint(1020, 384) << TaskbarEdge::Right;
        // Corners follow the diagonals of the (non-square) screen.
        QTest::newRow("near top-left, wide side") << QPoint(100, 30) << TaskbarEdge::Top;
        QTest::newRow("near top-left, tall side") << QPoint(30, 100) << TaskbarEdge::Left;
        QTest::newRow("bottom-right corner, above diagonal")
            << QPoint(1000, 700) << TaskbarEdge::Right;
        QTest::newRow("off screen below") << QPoint(500, 2000) << TaskbarEdge::Bottom;
        QTest::newRow("off screen left") << QPoint(-500, 384) << TaskbarEdge::Left;
        QTest::newRow("centre tie goes to bottom") << QPoint(512, 384) << TaskbarEdge::Bottom;
    }

    void nearest()
    {
        QFETCH(QPoint, point);
        QFETCH(TaskbarEdge, edge);
        QCOMPARE(nearestEdge(point, QSize(1024, 768)), edge);
    }

    void rects()
    {
        const QSize screen(1024, 768);
        QCOMPARE(taskbarRect(TaskbarEdge::Bottom, screen, 28), QRect(0, 740, 1024, 28));
        QCOMPARE(taskbarRect(TaskbarEdge::Top, screen, 28), QRect(0, 0, 1024, 28));
        QCOMPARE(taskbarRect(TaskbarEdge::Left, screen, 70), QRect(0, 0, 70, 768));
        QCOMPARE(taskbarRect(TaskbarEdge::Right, screen, 70), QRect(954, 0, 70, 768));
        // A drag that left a bottom taskbar 700 pixels upwards.
        QCOMPARE(
            taskbarToScreen(TaskbarEdge::Bottom, screen, 28, QPoint(20, -720)), QPoint(20, 20));
        QCOMPARE(taskbarToScreen(TaskbarEdge::Right, screen, 70, QPoint(-900, 10)), QPoint(54, 10));
    }

    void edgeAndLockPersist()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("daytona.ini"));
        {
            Settings settings(path);
            Taskbar taskbar;
            taskbar.setSettings(&settings);
            QCOMPARE(taskbar.edge(), TaskbarEdge::Bottom);
            QVERIFY(taskbar.isLocked()); // locked by default
            QAction *lock = taskbar.contextMenu()->actions().first();
            QCOMPARE(lock->text(), QStringLiteral("&Lock the Taskbar"));
            QVERIFY(lock->isCheckable() && lock->isChecked());
            QAction *properties = taskbar.contextMenu()->actions().last();
            QVERIFY(properties->isEnabled());
            QSignalSpy requested(&taskbar, &Taskbar::propertiesRequested);
            properties->trigger();
            QCOMPARE(requested.count(), 1);
            lock->trigger();
            QVERIFY(!taskbar.isLocked());
            settings.setTaskbarEdge(TaskbarEdge::Left);
        }
        Settings settings(path);
        Taskbar taskbar;
        taskbar.setSettings(&settings);
        QVERIFY(!taskbar.isLocked());
        QCOMPARE(taskbar.edge(), TaskbarEdge::Left);
    }

    void verticalLayout()
    {
        Taskbar taskbar;
        QSignalSpy changed(&taskbar, &Taskbar::edgeChanged);
        taskbar.setEdge(TaskbarEdge::Right);
        QCOMPARE(changed.count(), 1);
        QVERIFY(taskbar.taskList()->isVertical());
        // NT's narrowest vertical taskbar fits the Start button.
        QVERIFY(taskbar.thickness() >= taskbar.startButton()->sizeHint().width());
        QCOMPARE(taskbar.width(), taskbar.thickness());
        taskbar.resize(taskbar.thickness(), 600);
        taskbar.show();
        QCoreApplication::processEvents();
        // Start button at the top, notification area at the bottom.
        QVERIFY(taskbar.startButton()->y() < taskbar.taskList()->y());
        QVERIFY(taskbar.taskList()->geometry().bottom() < 600 - 22);
        // The window buttons take the space between them.
        QVERIFY(taskbar.taskList()->height() > 400);
        QVERIFY(taskbar.notificationArea()->geometry().bottom() >= 600 - 4);
        taskbar.setEdge(TaskbarEdge::Top);
        QVERIFY(!taskbar.taskList()->isVertical());
        QCOMPARE(taskbar.height(), Taskbar::RowHeight);
    }

    void thicknessSnapping()
    {
        QCOMPARE(TaskbarThickness::forRows(1), 28);
        QCOMPARE(TaskbarThickness::forRows(2), 53);
        QCOMPARE(TaskbarThickness::forRows(3), 78);
        // Half of a 768 pixel screen holds 15 rows.
        QCOMPARE(maxTaskbarRows(768), 15);
        QCOMPARE(snapTaskbarRows(28, 768), 1);
        QCOMPARE(snapTaskbarRows(39, 768), 1); // under half a row more
        QCOMPARE(snapTaskbarRows(41, 768), 2); // over half a row more
        QCOMPARE(snapTaskbarRows(78, 768), 3);
        QCOMPARE(snapTaskbarRows(0, 768), 1);
        QCOMPARE(snapTaskbarRows(760, 768), 15);
        QCOMPARE(clampTaskbarWidth(30, 70, 1024), 70);
        QCOMPARE(clampTaskbarWidth(200, 70, 1024), 200);
        QCOMPARE(clampTaskbarWidth(900, 70, 1024), 512);
        const QSize screen(1024, 768);
        QCOMPARE(draggedThickness(TaskbarEdge::Bottom, screen, QPoint(5, 700)), 68);
        QCOMPARE(draggedThickness(TaskbarEdge::Top, screen, QPoint(5, 60)), 60);
        QCOMPARE(draggedThickness(TaskbarEdge::Left, screen, QPoint(150, 5)), 150);
        QCOMPARE(draggedThickness(TaskbarEdge::Right, screen, QPoint(900, 5)), 124);
    }

    void resizeRows()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        Taskbar taskbar;
        taskbar.setSettings(&settings);
        taskbar.resize(1024, taskbar.thickness());
        taskbar.show();
        // Locked: no resize handle and no resizing.
        QVERIFY(taskbar.cursor().shape() != Qt::SizeVerCursor);
        dragInnerEdge(&taskbar, -50);
        QCOMPARE(taskbar.rows(), 1);

        taskbar.setLocked(false);
        QVERIFY(taskbar.isResizeHandle(QPoint(500, 1)));
        QVERIFY(!taskbar.isResizeHandle(QPoint(500, 14)));
        dragInnerEdge(&taskbar, -50); // 50 pixels up: two more rows
        QCOMPARE(taskbar.rows(), 3);
        QCOMPARE(taskbar.height(), 78);
        QCOMPARE(taskbar.taskList()->rows(), 3);
        QCOMPARE(settings.taskbarRows(), 3); // saved on release

        // Vertical: any width from the minimum to half the screen.
        taskbar.setEdge(TaskbarEdge::Left);
        QCOMPARE(taskbar.width(), taskbar.minimumVerticalWidth());
        taskbar.setVerticalWidth(150);
        QCOMPARE(taskbar.width(), 150);
        taskbar.setVerticalWidth(10);
        QCOMPARE(taskbar.width(), taskbar.minimumVerticalWidth());
    }

    void autoHide()
    {
        Taskbar taskbar;
        // Away from the offscreen platform's pointer, which stays at 0,0.
        taskbar.move(200, 300);
        taskbar.resize(1024, Taskbar::RowHeight);
        taskbar.show();
        QVERIFY(!taskbar.isAutoHide());
        taskbar.setAutoHide(true);
        QVERIFY(taskbar.isCollapsed());
        QCOMPARE(taskbar.height(), 2); // a 2 pixel strip on its edge
        QVERIFY(taskbar.startButton()->isHidden());

        taskbar.expand(); // the pointer reached the strip
        QVERIFY(!taskbar.isCollapsed());
        QCOMPARE(taskbar.height(), Taskbar::RowHeight);
        QVERIFY(!taskbar.startButton()->isHidden());

        // The pointer leaves and nothing keeps it shown: it hides again
        // after a short delay.
        taskbar.collapseLater();
        QTRY_VERIFY_WITH_TIMEOUT(taskbar.isCollapsed(), 2000);

        // While a menu is open it stays.
        bool menuOpen = true;
        taskbar.setKeepShown([&menuOpen] { return menuOpen; });
        taskbar.expand();
        taskbar.collapseLater();
        QTest::qWait(1200);
        QVERIFY(!taskbar.isCollapsed());
        menuOpen = false;
        QTRY_VERIFY_WITH_TIMEOUT(taskbar.isCollapsed(), 2000);

        taskbar.setAutoHide(false);
        QVERIFY(!taskbar.isCollapsed());
        QCOMPARE(taskbar.height(), Taskbar::RowHeight);
    }

    void optionsFollowSettings()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        Taskbar taskbar;
        taskbar.setSettings(&settings);
        QVERIFY(taskbar.isAlwaysOnTop());
        QVERIFY(!taskbar.notificationArea()->clock()->isHidden());
        settings.setShowClock(false);
        QVERIFY(taskbar.notificationArea()->clock()->isHidden());
        settings.setTaskbarAlwaysOnTop(false);
        QVERIFY(!taskbar.isAlwaysOnTop());
        settings.setTaskbarAutoHide(true);
        QVERIFY(taskbar.isAutoHide());
    }

    void propertiesDialog()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        TaskbarPropertiesDialog dialog(&settings);
        QCOMPARE(dialog.options(), TaskbarOptions::fromSettings(settings));
        QVERIFY(!dialog.applyButton()->isEnabled());
        auto *autoHide = dialog.findChildren<QCheckBox *>().at(1);
        QCOMPARE(autoHide->text(), QStringLiteral("A&uto hide"));
        autoHide->setChecked(true);
        QVERIFY(dialog.applyButton()->isEnabled());
        QVERIFY(!settings.taskbarAutoHide()); // not until Apply
        dialog.apply();
        QVERIFY(settings.taskbarAutoHide());
        QVERIFY(!dialog.applyButton()->isEnabled());

        // Cancel leaves the settings as they were.
        dialog.findChildren<QCheckBox *>().at(3)->setChecked(false); // Show Clock
        dialog.reject();
        QVERIFY(settings.showClock());
        // OK applies.
        dialog.prepare();
        dialog.findChildren<QCheckBox *>().at(4)->setChecked(false); // Lock
        dialog.accept();
        QVERIFY(!settings.taskbarLocked());
        QCOMPARE(dialog.findChildren<QCheckBox *>().size(), 5);
    }

    void lockedTaskbarDoesNotMove()
    {
        Taskbar taskbar;
        taskbar.resize(1024, Taskbar::RowHeight);
        taskbar.show();
        QSignalSpy changed(&taskbar, &Taskbar::edgeChanged);
        // Press on empty space and drag far up: nothing happens while locked.
        QTest::mousePress(&taskbar, Qt::LeftButton, {}, QPoint(700, 14));
        QMouseEvent move(QEvent::MouseMove, QPointF(700, -700), QPointF(700, -700), Qt::NoButton,
            Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&taskbar, &move);
        QTest::mouseRelease(&taskbar, Qt::LeftButton, {}, QPoint(700, -700));
        QCOMPARE(changed.count(), 0);
        QCOMPARE(taskbar.edge(), TaskbarEdge::Bottom);
    }

private:
    // Presses the inner edge of a bottom taskbar and drags by dy pixels.
    static void dragInnerEdge(Taskbar *taskbar, int dy)
    {
        QTest::mousePress(taskbar, Qt::LeftButton, {}, QPoint(500, 1));
        QMouseEvent move(QEvent::MouseMove, QPointF(500, 1 + dy), QPointF(500, 1 + dy),
            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(taskbar, &move);
        QTest::mouseRelease(taskbar, Qt::LeftButton, {}, QPoint(500, 1 + dy));
    }
};

QTEST_MAIN(TestTaskbarPlacement)
#include "tst_taskbarplacement.moc"
