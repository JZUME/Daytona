// SPDX-License-Identifier: GPL-3.0-or-later

#include "daytonastyle.h"
#include "desktop.h"
#include "settings.h"

#include <QApplication>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

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

    void centredPosition()
    {
        QCOMPARE(centredWallpaperPosition({ 100, 50 }, { 10, 10 }), QPoint(45, 20));
        // Larger than the desktop: cropped evenly, never scaled.
        QCOMPARE(centredWallpaperPosition({ 100, 50 }, { 200, 150 }), QPoint(-50, -50));
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
