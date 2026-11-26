// SPDX-License-Identifier: GPL-3.0-or-later

#include "daytonastyle.h"
#include "desktop.h"
#include "settings.h"

#include <QApplication>
#include <QMenu>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>

using namespace Daytona;

Q_DECLARE_METATYPE(Daytona::WallpaperMode)

class TestDesktop : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        auto *style = new Style;
        style->setColourFile(m_dir.filePath(QStringLiteral("colors.ini")));
        QApplication::setStyle(style);
        // A 3x2 wallpaper with distinct corners.
        QImage image(3, 2, QImage::Format_RGB32);
        image.fill(Qt::red);
        image.setPixelColor(0, 0, Qt::blue);
        image.setPixelColor(2, 1, Qt::green);
        QVERIFY(image.save(wallpaperPath(), "BMP"));
    }

    void placement_data()
    {
        QTest::addColumn<QSize>("image");
        QTest::addColumn<WallpaperMode>("mode");
        QTest::addColumn<QRect>("expected");
        // On a 100x50 desktop.
        QTest::newRow("centre") << QSize(10, 10) << WallpaperMode::Centre << QRect(45, 20, 10, 10);
        // Larger than the desktop: cropped evenly, never scaled.
        QTest::newRow("centre, large")
            << QSize(200, 150) << WallpaperMode::Centre << QRect(-50, -50, 200, 150);
        QTest::newRow("stretch") << QSize(10, 10) << WallpaperMode::Stretch << QRect(0, 0, 100, 50);
        QTest::newRow("fit, tall") << QSize(10, 10) << WallpaperMode::Fit << QRect(25, 0, 50, 50);
        QTest::newRow("fit, wide")
            << QSize(400, 100) << WallpaperMode::Fit << QRect(0, 12, 100, 25);
        QTest::newRow("fill, tall")
            << QSize(10, 10) << WallpaperMode::Fill << QRect(0, -25, 100, 100);
        QTest::newRow("fill, wide")
            << QSize(400, 100) << WallpaperMode::Fill << QRect(-50, 0, 200, 50);
    }

    void placement()
    {
        QFETCH(QSize, image);
        QFETCH(WallpaperMode, mode);
        QFETCH(QRect, expected);
        QCOMPARE(wallpaperRect({ 100, 50 }, image, mode), expected);
    }

    void colourOnly()
    {
        Settings settings(m_dir.filePath(QStringLiteral("a.ini")));
        Desktop desktop(&settings);
        desktop.resize(20, 10);
        const QImage shot = desktop.grab().toImage();
        QCOMPARE(shot.pixelColor(0, 0), QColor(0, 128, 128)); // Windows Standard desktop
        QCOMPARE(shot.pixelColor(19, 9), QColor(0, 128, 128));
    }

    void centredWallpaper()
    {
        Settings settings(m_dir.filePath(QStringLiteral("b.ini")));
        settings.setWallpaperPath(wallpaperPath());
        settings.setWallpaperMode(WallpaperMode::Centre);
        Desktop desktop(&settings);
        desktop.resize(21, 10);
        if (desktop.wallpaper().isNull()) {
            QSignalSpy loaded(&desktop, &Desktop::wallpaperLoaded);
            QVERIFY(loaded.wait(3000));
        }
        const QImage shot = desktop.grab().toImage();
        QCOMPARE(shot.pixelColor(0, 0), QColor(0, 128, 128));
        QCOMPARE(shot.pixelColor(9, 4), QColor(Qt::blue)); // (21-3)/2, (10-2)/2
        QCOMPARE(shot.pixelColor(11, 5), QColor(Qt::green));
        QCOMPARE(shot.pixelColor(12, 5), QColor(0, 128, 128));
    }

    void scaledWallpaper()
    {
        Settings settings(m_dir.filePath(QStringLiteral("e.ini")));
        settings.setWallpaperPath(wallpaperPath());
        settings.setWallpaperMode(WallpaperMode::Fit);
        Desktop desktop(&settings);
        desktop.resize(30, 40);
        if (desktop.wallpaper().isNull()) {
            QSignalSpy loaded(&desktop, &Desktop::wallpaperLoaded);
            QVERIFY(loaded.wait(3000));
        }
        // Fit: 3x2 becomes 30x20, with the desktop colour above and below.
        QImage shot = desktop.grab().toImage();
        QCOMPARE(shot.pixelColor(15, 2), QColor(0, 128, 128));
        QCOMPARE(shot.pixelColor(1, 11), QColor(Qt::blue));
        QCOMPARE(shot.pixelColor(28, 28), QColor(Qt::green));
        QCOMPARE(shot.pixelColor(15, 37), QColor(0, 128, 128));

        // Stretch covers it all, corner to corner.
        settings.setWallpaperMode(WallpaperMode::Stretch);
        shot = desktop.grab().toImage();
        QCOMPARE(shot.pixelColor(0, 0), QColor(Qt::blue));
        QCOMPARE(shot.pixelColor(29, 39), QColor(Qt::green));
    }

    void tiledWallpaperFollowsSettings()
    {
        Settings settings(m_dir.filePath(QStringLiteral("c.ini")));
        Desktop desktop(&settings);
        desktop.resize(7, 4);
        QSignalSpy loaded(&desktop, &Desktop::wallpaperLoaded);
        settings.setWallpaperMode(WallpaperMode::Tile);
        settings.setWallpaperPath(wallpaperPath());
        QVERIFY(loaded.wait(3000));
        const QImage shot = desktop.grab().toImage();
        for (const QPoint &p : { QPoint(0, 0), QPoint(3, 0), QPoint(6, 2), QPoint(0, 2) })
            QCOMPARE(shot.pixelColor(p), QColor(Qt::blue));
        QCOMPARE(shot.pixelColor(5, 1), QColor(Qt::green));

        // Clearing the wallpaper goes back to the plain colour.
        settings.setWallpaperPath(QString());
        QCOMPARE(desktop.grab().toImage().pixelColor(0, 0), QColor(0, 128, 128));
    }

    void missingWallpaperIsJustColour()
    {
        Settings settings(m_dir.filePath(QStringLiteral("d.ini")));
        settings.setWallpaperPath(m_dir.filePath(QStringLiteral("missing.bmp")));
        Desktop desktop(&settings);
        desktop.resize(4, 4);
        QSignalSpy loaded(&desktop, &Desktop::wallpaperLoaded);
        QVERIFY(loaded.wait(3000));
        QCOMPARE(desktop.grab().toImage().pixelColor(1, 1), QColor(0, 128, 128));
    }

    void onePerScreen()
    {
        Settings settings(m_dir.filePath(QStringLiteral("m.ini")));
        DesktopManager manager(&settings);
        manager.start();
        QCOMPARE(manager.desktops().size(), QGuiApplication::screens().size());
        for (QScreen *screen : QGuiApplication::screens()) {
            QVERIFY(manager.desktop(screen));
            QCOMPARE(manager.desktop(screen)->windowHandle()->screen(), screen);
        }
        QSignalSpy requested(&manager, &DesktopManager::propertiesRequested);
        manager.desktops().first()->contextMenu()->actions().first()->trigger();
        QCOMPARE(requested.count(), 1);
    }

    void propertiesMenu()
    {
        Desktop desktop(nullptr);
        const QList<QAction *> actions = desktop.contextMenu()->actions();
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions[0]->text(), QStringLiteral("P&roperties"));
        QVERIFY(actions[0]->isEnabled());
        QSignalSpy requested(&desktop, &Desktop::propertiesRequested);
        actions[0]->trigger();
        QCOMPARE(requested.count(), 1);
    }

private:
    QString wallpaperPath() const { return m_dir.filePath(QStringLiteral("wall.bmp")); }
    QTemporaryDir m_dir;
};

QTEST_MAIN(TestDesktop)
#include "tst_desktop.moc"
