// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayproperties.h"
#include "monitorlayout.h"
#include "outputmanager.h"
#include "settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

// Stands in for the compositor: records requests and answers them at once,
// applying them when it accepts. DP-1 has screenModes; with screenList set,
// HDMI-1 is a second, smaller screen.
class FakeScaler : public ScreenSettings
{
public:
    QStringList screens() const override { return screenList; }
    QString description(const QString &screen) const override
    {
        return QStringLiteral("Maker Model (%1)").arg(screen);
    }
    bool isEnabled(const QString &screen) const override { return !disabled.contains(screen); }
    QPoint position(const QString &screen) const override { return positions.value(screen); }
    double scale(const QString &screen) const override { return scales.value(screen); }
    QList<ScreenMode> modes(const QString &screen) const override
    {
        if (screen == QStringLiteral("DP-1"))
            return screenModes;
        if (screen == QStringLiteral("HDMI-1"))
            return { mode(1280, 1024, 60000, true), mode(1024, 768, 60000) };
        return {};
    }
    ScreenMode currentMode(const QString &screen) const override
    {
        if (disabled.contains(screen))
            return {};
        if (screen == QStringLiteral("DP-1"))
            return current;
        if (screen == QStringLiteral("HDMI-1"))
            return hdmiCurrent;
        return {};
    }
    QString identity(const QString &screen) const override
    {
        return QStringLiteral("Maker Model ") + screen;
    }
    void change(const QList<ScreenChange> &changes, const std::function<void(bool)> &done) override
    {
        batches.append(changes);
        for (const ScreenChange &one : changes) {
            if (one.scale > 0)
                requests.append({ one.screen, one.scale });
            if (one.mode.isValid())
                modeRequests.append(modeKey(one.mode));
            if (!accept)
                continue;
            if (one.scale > 0)
                scales.insert(one.screen, one.scale);
            if (one.mode.isValid() && one.screen == QStringLiteral("DP-1"))
                current = one.mode;
            if (one.mode.isValid() && one.screen == QStringLiteral("HDMI-1"))
                hdmiCurrent = one.mode;
            if (one.position)
                positions.insert(one.screen, *one.position);
            if (one.enabled && *one.enabled)
                disabled.remove(one.screen);
            else if (one.enabled)
                disabled.insert(one.screen);
        }
        if (done)
            done(accept);
    }

    static ScreenMode mode(int w, int h, int refresh, bool preferred = false)
    {
        ScreenMode m;
        m.size = QSize(w, h);
        m.refresh = refresh;
        m.preferred = preferred;
        return m;
    }

    // Two screens: DP-1 (1920x1080) with HDMI-1 (1280x1024) on its right.
    void addSecondScreen()
    {
        screenList = { QStringLiteral("DP-1"), QStringLiteral("HDMI-1") };
        scales.insert(QStringLiteral("HDMI-1"), 1.0);
        positions.insert(QStringLiteral("DP-1"), QPoint(0, 0));
        positions.insert(QStringLiteral("HDMI-1"), QPoint(1920, 0));
    }

    QStringList screenList;
    QHash<QString, double> scales { { QStringLiteral("DP-1"), 1.0 } };
    QHash<QString, QPoint> positions;
    QSet<QString> disabled;
    QList<ScreenMode> screenModes { mode(1920, 1080, 60000, true), mode(1920, 1080, 59940),
        mode(1920, 1080, 144000), mode(1024, 768, 75000), mode(1024, 768, 60000),
        mode(800, 600, 60000) };
    ScreenMode current = mode(1920, 1080, 60000, true);
    ScreenMode hdmiCurrent = mode(1280, 1024, 60000, true);
    QList<QPair<QString, double>> requests; // scale requests
    QStringList modeRequests;
    QList<QList<ScreenChange>> batches; // every request, as sent
    bool accept = true;
};

} // namespace

class TestDisplayProperties : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Keep the theme tool and settings inside a temporary home.
        QVERIFY(m_home.isValid());
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_home.filePath(QStringLiteral("config"))));
        qputenv("XDG_DATA_HOME", QFile::encodeName(m_home.filePath(QStringLiteral("data"))));
        qputenv("LABWC_PID", "");
    }

    void candidates()
    {
        QTemporaryDir a, b;
        for (const char *name : { "b.png", "A.bmp", "notes.txt", "c.JPG" })
            writeImage(a.filePath(QLatin1String(name)));
        writeImage(b.filePath(QStringLiteral("z.png")));
        const QStringList files = wallpaperCandidates({ a.path(), b.path(), a.path() });
        QCOMPARE(files,
            QStringList({ a.filePath(QStringLiteral("A.bmp")), a.filePath(QStringLiteral("b.png")),
                // Any case, as Windows-era files are often upper case.
                a.filePath(QStringLiteral("c.JPG")), b.filePath(QStringLiteral("z.png")) }));
        QCOMPARE(wallpaperCandidates({ a.path() }, 1).size(), 1);
        QVERIFY(wallpaperCandidates({ a.filePath(QStringLiteral("missing")) }).isEmpty());
    }

    void backgroundTab()
    {
        QTemporaryDir dir;
        const QString wall = dir.filePath(QStringLiteral("wall.png"));
        writeImage(wall);
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        settings.setWallpaperPath(wall);
        settings.setWallpaperMode(WallpaperMode::Tile);
        DisplayPropertiesDialog dialog(&settings);
        // The current wallpaper is listed and selected, after (None).
        QCOMPARE(dialog.wallpaperList()->item(0)->text(), QStringLiteral("(None)"));
        QCOMPARE(dialog.selectedWallpaper(), wall);
        QCOMPARE(dialog.selectedMode(), WallpaperMode::Tile);
        QVERIFY(!dialog.applyButton()->isEnabled());

        // The later Windows modes are offered and selected from settings too.
        for (WallpaperMode mode :
            { WallpaperMode::Stretch, WallpaperMode::Fit, WallpaperMode::Fill }) {
            settings.setWallpaperMode(mode);
            QCOMPARE(DisplayPropertiesDialog(&settings).selectedMode(), mode);
        }
        settings.setWallpaperMode(WallpaperMode::Tile);

        dialog.wallpaperList()->setCurrentRow(0); // (None)
        QVERIFY(dialog.applyButton()->isEnabled());
        dialog.apply();
        QVERIFY(settings.wallpaperPath().isEmpty());
        QVERIFY(!dialog.applyButton()->isEnabled());
    }

    void appearanceTab()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        DisplayPropertiesDialog dialog(&settings);
        QCOMPARE(dialog.schemeBox()->currentText(), QStringLiteral("Windows Standard"));
        QCOMPARE(dialog.candidateColours().value(QStringLiteral("ActiveTitle")), QColor(0, 0, 128));
        dialog.schemeBox()->setCurrentText(QStringLiteral("High Contrast Black"));
        QCOMPARE(
            dialog.candidateColours().value(QStringLiteral("ActiveTitle")), QColor(128, 0, 128));

        // Loading a .theme file lists and selects it.
        const QString theme = QStringLiteral(DAYTONA_TEST_DATA "/themes/Sample.theme");
        dialog.loadThemeFile(theme);
        QCOMPARE(dialog.schemeBox()->currentText(), QStringLiteral("Sample"));
        QCOMPARE(dialog.candidateColours().value(QStringLiteral("ActiveTitle")), QColor(128, 0, 0));
        const int count = dialog.schemeBox()->count();
        dialog.loadThemeFile(theme); // not twice
        QCOMPARE(dialog.schemeBox()->count(), count);
    }

    void systemLogoSetting()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        DisplayPropertiesDialog dialog(&settings);
        QVERIFY(dialog.systemLogoBox()->isChecked());
        QVERIFY(!dialog.applyButton()->isEnabled());
        dialog.systemLogoBox()->setChecked(false);
        QVERIFY(dialog.applyButton()->isEnabled());
        dialog.apply();
        QCOMPARE(settings.systemStartLogo(), false);
        dialog.prepare();
        QVERIFY(!dialog.systemLogoBox()->isChecked());
    }

    void screenSaverTab()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        DisplayPropertiesDialog dialog(&settings);
        QCOMPARE(dialog.screenSaverBox()->currentText(), QStringLiteral("Blank Screen"));
        QCOMPARE(dialog.screenSaverWaitBox()->value(), 15);
        QVERIFY(!dialog.screenSaverPasswordBox()->isChecked());
        QVERIFY(dialog.screenSaverPreviewButton()->isEnabled());

        dialog.screenSaverWaitBox()->setValue(3);
        dialog.screenSaverPasswordBox()->setChecked(true);
        QVERIFY(dialog.applyButton()->isEnabled());
        QSignalSpy preview(&dialog, &DisplayPropertiesDialog::screenSaverPreviewRequested);
        dialog.screenSaverPreviewButton()->click();
        QCOMPARE(preview.count(), 1);
        dialog.apply();
        QCOMPARE(settings.screenSaverWait(), 3);
        QCOMPARE(settings.screenSaverPassword(), true);
        QCOMPARE(settings.screenSaver(), ScreenSaver::Blank);

        // (None): nothing to preview, protect or wait for.
        dialog.screenSaverBox()->setCurrentIndex(0);
        QVERIFY(!dialog.screenSaverPreviewButton()->isEnabled());
        QVERIFY(!dialog.screenSaverPasswordBox()->isEnabled());
        QVERIFY(!dialog.screenSaverWaitBox()->isEnabled());
        dialog.apply();
        QCOMPARE(settings.screenSaver(), ScreenSaver::None);
        dialog.prepare();
        QCOMPARE(dialog.screenSaverBox()->currentText(), QStringLiteral("(None)"));
        QCOMPARE(dialog.screenSaverWaitBox()->value(), 3);
    }

    void fontSizeChoices()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        dialog.prepare();
        QVERIFY(dialog.fontSizeBox()->isEnabled());
        QCOMPARE(dialog.fontSizeBox()->count(), 2);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Small Fonts"));
        QCOMPARE(dialog.selectedScale(), 1.0);
        QVERIFY(!dialog.applyButton()->isEnabled());

        // Large Fonts is 150% (NT's 125% is a custom size).
        dialog.selectScale(1.5);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Large Fonts"));
        QVERIFY(dialog.applyButton()->isEnabled());
        // A custom size gets one entry of its own, reused for the next one.
        dialog.selectScale(1.25);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Other (125%)"));
        dialog.selectScale(2.0);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Other (200%)"));
        QCOMPARE(dialog.fontSizeBox()->count(), 3);
        QCOMPARE(dialog.selectedScale(), 2.0);

        // Opening the dialog again shows the screen's actual scale.
        scaler.scales.insert(QStringLiteral("DP-1"), 1.75);
        dialog.prepare();
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Other (175%)"));
        QVERIFY(!dialog.applyButton()->isEnabled());
    }

    void fontSizeKeptWhenConfirmed()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        int asked = 0;
        dialog.setScaleConfirmation([&asked](const std::function<void(bool)> &done) {
            ++asked;
            done(true);
        });
        dialog.prepare();

        dialog.selectScale(1.25);
        dialog.apply();
        QCOMPARE(scaler.requests.size(), 1);
        QCOMPARE(scaler.requests.first(), qMakePair(QStringLiteral("DP-1"), 1.25));
        QCOMPARE(asked, 1);
        // Saved for that monitor, not for the connector.
        QCOMPARE(settings.outputScale(QStringLiteral("Maker Model DP-1")), 1.25);
        QCOMPARE(settings.outputScale(QStringLiteral("DP-1")), 0.0);
        QVERIFY(!dialog.applyButton()->isEnabled());

        // Applying again without a change asks nothing.
        dialog.apply();
        QCOMPARE(scaler.requests.size(), 1);
    }

    void fontSizeRevertsWhenNotConfirmed()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        dialog.setScaleConfirmation([](const std::function<void(bool)> &done) { done(false); });
        dialog.prepare();

        dialog.selectScale(2.0);
        dialog.apply();
        // Set, then set back.
        QCOMPARE(scaler.requests.size(), 2);
        QCOMPARE(scaler.requests.at(0).second, 2.0);
        QCOMPARE(scaler.requests.at(1).second, 1.0);
        QCOMPARE(scaler.scale(QStringLiteral("DP-1")), 1.0);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Small Fonts"));
        QCOMPARE(settings.outputScale(QStringLiteral("Maker Model DP-1")), 0.0);
        QVERIFY(!dialog.applyButton()->isEnabled());
    }

    void fontSizeRefusedByCompositor()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        scaler.accept = false;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        bool asked = false;
        dialog.setScaleConfirmation([&asked](const std::function<void(bool)> &done) {
            asked = true;
            done(true);
        });
        dialog.prepare();

        dialog.selectScale(1.25);
        dialog.apply();
        QCOMPARE(scaler.requests.size(), 1);
        QVERIFY(!asked);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Small Fonts"));
        QCOMPARE(settings.outputScale(QStringLiteral("Maker Model DP-1")), 0.0);
        // The warning is not modal; close it.
        const auto boxes = dialog.findChildren<QMessageBox *>();
        QCOMPARE(boxes.size(), 1);
        QCOMPARE(
            boxes.first()->text(), QStringLiteral("The display settings could not be changed."));
        boxes.first()->close();
    }

    void fontSizeUnavailableWithoutCompositorSupport()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("UNKNOWN-1")); // not reported
        dialog.prepare();
        QVERIFY(!dialog.fontSizeBox()->isEnabled());
        QVERIFY(!dialog.customFontSizeButton()->isEnabled());
        QVERIFY(!dialog.desktopAreaSlider()->isEnabled());
        QVERIFY(!dialog.refreshBox()->isEnabled());
        QCOMPARE(dialog.desktopAreaLabel()->text(), QStringLiteral("Not available"));

        DisplayPropertiesDialog noScaler(&settings);
        QVERIFY(!noScaler.fontSizeBox()->isEnabled());
        noScaler.selectScale(1.25);
        noScaler.apply(); // nothing to apply it with; must not crash
    }

    void desktopArea()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        dialog.prepare();
        // Less to more, one notch per size; the current one selected.
        QVERIFY(dialog.desktopAreaSlider()->isEnabled());
        QCOMPARE(dialog.desktopAreaSlider()->maximum(), 2);
        QCOMPARE(dialog.desktopAreaSlider()->value(), 2);
        QCOMPARE(dialog.desktopAreaLabel()->text(), QStringLiteral("1920 by 1080 pixels"));
        // 59.94 and 60 Hz would both read "60", so they keep their decimals.
        QStringList rates;
        for (int i = 0; i < dialog.refreshBox()->count(); ++i)
            rates << dialog.refreshBox()->itemText(i);
        QCOMPARE(rates,
            QStringList({ QStringLiteral("59.94 Hertz"), QStringLiteral("60.00 Hertz"),
                QStringLiteral("144 Hertz") }));
        QCOMPARE(dialog.refreshBox()->currentText(), QStringLiteral("60.00 Hertz"));
        QVERIFY(!dialog.applyButton()->isEnabled());

        // A smaller size: its own rates, keeping 60 Hz.
        dialog.desktopAreaSlider()->setValue(1);
        QCOMPARE(dialog.desktopAreaLabel()->text(), QStringLiteral("1024 by 768 pixels"));
        QCOMPARE(dialog.refreshBox()->count(), 2);
        QCOMPARE(dialog.refreshBox()->currentText(), QStringLiteral("60 Hertz"));
        QVERIFY(dialog.applyButton()->isEnabled());
        dialog.desktopAreaSlider()->setValue(0);
        QCOMPARE(modeKey(dialog.selectedScreenMode()), QStringLiteral("800x600@60000"));
        // Back up: the size's preferred mode wins over a rate it lacks.
        dialog.refreshBox()->setCurrentIndex(0);
        dialog.desktopAreaSlider()->setValue(2);
        QCOMPARE(modeKey(dialog.selectedScreenMode()), QStringLiteral("1920x1080@60000"));
    }

    void modeKeptWhenConfirmed()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        int asked = 0;
        dialog.setScaleConfirmation([&asked](const std::function<void(bool)> &done) {
            ++asked;
            done(true);
        });
        dialog.prepare();

        // Mode and font size together: one change, one question.
        dialog.desktopAreaSlider()->setValue(1);
        dialog.refreshBox()->setCurrentIndex(dialog.refreshBox()->findData(75000));
        dialog.selectScale(1.5);
        dialog.apply();
        QCOMPARE(scaler.modeRequests, QStringList({ QStringLiteral("1024x768@75000") }));
        QCOMPARE(scaler.requests.size(), 1);
        QCOMPARE(asked, 1);
        QCOMPARE(modeKey(settings.outputMode(QStringLiteral("Maker Model DP-1"))),
            QStringLiteral("1024x768@75000"));
        QCOMPARE(settings.outputScale(QStringLiteral("Maker Model DP-1")), 1.5);

        // Only what changed is sent and saved.
        dialog.refreshBox()->setCurrentIndex(dialog.refreshBox()->findData(60000));
        dialog.apply();
        QCOMPARE(scaler.modeRequests.size(), 2);
        QCOMPARE(scaler.requests.size(), 1);
        QCOMPARE(modeKey(settings.outputMode(QStringLiteral("Maker Model DP-1"))),
            QStringLiteral("1024x768@60000"));
    }

    void modeRevertsWhenNotConfirmed()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        dialog.setScaleConfirmation([](const std::function<void(bool)> &done) { done(false); });
        dialog.prepare();

        dialog.desktopAreaSlider()->setValue(0);
        dialog.apply();
        QCOMPARE(scaler.modeRequests,
            QStringList({ QStringLiteral("800x600@60000"), QStringLiteral("1920x1080@60000") }));
        QCOMPARE(scaler.requests.size(), 0); // the font size was not touched
        QCOMPARE(modeKey(scaler.current), QStringLiteral("1920x1080@60000"));
        QCOMPARE(dialog.desktopAreaLabel()->text(), QStringLiteral("1920 by 1080 pixels"));
        QVERIFY(!settings.outputMode(QStringLiteral("Maker Model DP-1")).isValid());
        QVERIFY(!dialog.applyButton()->isEnabled());
    }

    void snapMonitorEdgeToEdge()
    {
        const QRect main(0, 0, 1920, 1080);
        // Dropped a little away from the right edge: touches it, tops lined up.
        QCOMPARE(snapMonitor(QRect(1950, 20, 1280, 1024), { main }, 40), QPoint(1920, 0));
        // Further down, it keeps its height but shares part of the edge.
        QCOMPARE(snapMonitor(QRect(1950, 300, 1280, 1024), { main }, 40), QPoint(1920, 300));
        // Bottoms line up when close.
        QCOMPARE(snapMonitor(QRect(1950, 70, 1280, 1024), { main }, 40), QPoint(1920, 56));
        // Dropped on top of it, towards the left: goes to its left.
        QCOMPARE(snapMonitor(QRect(-900, 0, 1280, 1024), { main }, 40), QPoint(-1280, 0));
        // Below it, left edges lined up.
        QCOMPARE(snapMonitor(QRect(10, 1500, 1280, 1024), { main }, 40), QPoint(0, 1080));
        // Far below and to the side: still kept touching, sharing an edge.
        const QPoint far = snapMonitor(QRect(5000, 5000, 1280, 1024), { main }, 40);
        const QRect placed(far, QSize(1280, 1024));
        QVERIFY(!placed.intersects(main));
        QVERIFY(placed.adjusted(-1, -1, 1, 1).intersects(main));
        // Never on top of another.
        const QRect right(1920, 0, 1280, 1024);
        const QPoint third = snapMonitor(QRect(1900, 10, 800, 600), { main, right }, 40);
        QVERIFY(!QRect(third, QSize(800, 600)).intersects(main));
        QVERIFY(!QRect(third, QSize(800, 600)).intersects(right));
        // Nothing to touch: stays put.
        QCOMPARE(snapMonitor(QRect(30, 40, 800, 600), {}, 40), QPoint(30, 40));
    }

    void resizeAndNormaliseMonitors()
    {
        const QList<QRect> row
            = { QRect(0, 0, 1920, 1080), QRect(1920, 0, 1280, 1024), QRect(3200, 0, 800, 600) };
        // The first gets smaller: the others follow its right edge.
        const QList<QRect> smaller = resizeMonitor(row, 0, QSize(1024, 768));
        QCOMPARE(smaller.at(0), QRect(0, 0, 1024, 768));
        QCOMPARE(smaller.at(1).topLeft(), QPoint(1024, 0));
        QCOMPARE(smaller.at(2).topLeft(), QPoint(2304, 0));
        // Stacked: the lower one follows the bottom edge.
        const QList<QRect> stacked = resizeMonitor(
            { QRect(0, 0, 1920, 1080), QRect(0, 1080, 1280, 1024) }, 0, QSize(1920, 1200));
        QCOMPARE(stacked.at(1).topLeft(), QPoint(0, 1200));
        // Normalised: the top left at 0,0.
        const QList<QRect> moved
            = normaliseMonitors({ QRect(0, 0, 1920, 1080), QRect(-1280, 56, 1280, 1024) });
        QCOMPARE(moved.at(0).topLeft(), QPoint(1280, 0));
        QCOMPARE(moved.at(1).topLeft(), QPoint(0, 56));
    }

    void monitorDiagramDrag()
    {
        MonitorArrangement arrangement;
        arrangement.resize(360, 140);
        arrangement.setMonitors({ { QStringLiteral("1"), QRect(0, 0, 1920, 1080), true },
            { QStringLiteral("2"), QRect(1920, 0, 1280, 1024), true },
            { QStringLiteral("3"), QRect(0, 0, 1024, 768), false } });
        QSignalSpy selected(&arrangement, &MonitorArrangement::selectedChanged);
        QSignalSpy moved(&arrangement, &MonitorArrangement::monitorMoved);
        // Drawn side by side, to scale; the switched-off one below them.
        const QRect one = arrangement.monitorRect(0);
        const QRect two = arrangement.monitorRect(1);
        QVERIFY(one.right() < two.left());
        QVERIFY(one.width() > two.width());
        QVERIFY(arrangement.monitorRect(2).top() > one.bottom());

        // A click selects without moving.
        QTest::mouseClick(&arrangement, Qt::LeftButton, {}, two.center());
        QCOMPARE(arrangement.selected(), 1);
        QCOMPARE(selected.size(), 1);
        QCOMPARE(moved.size(), 0);

        // Dragged over to the left of the first: it lands edge to edge there.
        const QPoint start = two.center();
        const QPoint end(one.left() - two.width() / 2 + 4, two.center().y() + 2);
        QTest::mousePress(&arrangement, Qt::LeftButton, {}, start);
        QTest::mouseMove(&arrangement, (start + end) / 2);
        QTest::mouseMove(&arrangement, end);
        QTest::mouseRelease(&arrangement, Qt::LeftButton, {}, end);
        QCOMPARE(moved.size(), 1);
        QCOMPARE(moved.first().at(0).toInt(), 1);
        QCOMPARE(moved.first().at(1).toPoint(), QPoint(-1280, 0));

        // A switched-off monitor can be selected but not dragged.
        const QRect three = arrangement.monitorRect(2);
        QTest::mousePress(&arrangement, Qt::LeftButton, {}, three.center());
        QTest::mouseMove(&arrangement, three.center() + QPoint(40, -60));
        QTest::mouseRelease(&arrangement, Qt::LeftButton, {}, three.center() + QPoint(40, -60));
        QCOMPARE(arrangement.selected(), 2);
        QCOMPARE(moved.size(), 1);
    }

    void oneScreenHasNoArrangement()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        dialog.prepare();
        // NT's own Settings tab.
        QVERIFY(dialog.monitorArrangement()->isHidden()
            || dialog.monitorArrangement()->parentWidget()->isHidden());
        QVERIFY(dialog.primaryBox()->isHidden());
        QVERIFY(dialog.extendBox()->isHidden());
    }

    void twoScreensArrangement()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        scaler.addSecondScreen();
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        int asked = 0;
        dialog.setScaleConfirmation([&asked](const std::function<void(bool)> &done) {
            ++asked;
            done(true);
        });
        dialog.prepare();

        QVERIFY(!dialog.monitorArrangement()->parentWidget()->isHidden());
        QVERIFY(!dialog.primaryBox()->isHidden());
        QCOMPARE(dialog.displayBox()->count(), 2);
        QCOMPARE(dialog.displayBox()->itemText(0), QStringLiteral("1. Maker Model (DP-1)"));
        QCOMPARE(dialog.displayBox()->itemText(1), QStringLiteral("2. Maker Model (HDMI-1)"));
        const QList<MonitorArrangement::Monitor> drawn = dialog.monitorArrangement()->monitors();
        QCOMPARE(drawn.size(), 2);
        QCOMPARE(drawn.at(0).geometry, QRect(0, 0, 1920, 1080));
        QCOMPARE(drawn.at(1).geometry, QRect(1920, 0, 1280, 1024));
        // The first is primary (the test has no screen of either name), so it
        // stays extended.
        QCOMPARE(dialog.displayBox()->currentIndex(), 0);
        QVERIFY(dialog.primaryBox()->isChecked());
        QVERIFY(!dialog.primaryBox()->isEnabled());
        QVERIFY(dialog.extendBox()->isChecked());
        QVERIFY(!dialog.extendBox()->isEnabled());

        // Choosing the second shows its own settings.
        dialog.displayBox()->setCurrentIndex(1);
        QCOMPARE(dialog.monitorArrangement()->selected(), 1);
        QCOMPARE(dialog.desktopAreaLabel()->text(), QStringLiteral("1280 by 1024 pixels"));
        QVERIFY(!dialog.primaryBox()->isChecked());
        QVERIFY(dialog.primaryBox()->isEnabled());
        QVERIFY(dialog.extendBox()->isEnabled());
        QVERIFY(!dialog.applyButton()->isEnabled());

        // Dragged to the left of the first: both move, so the desktop starts
        // at 0,0, and are saved once kept.
        Q_EMIT dialog.monitorArrangement()->monitorMoved(1, QPoint(-1280, 0));
        QVERIFY(dialog.applyButton()->isEnabled());
        dialog.apply();
        QCOMPARE(asked, 1);
        QCOMPARE(scaler.batches.size(), 1);
        QCOMPARE(scaler.positions.value(QStringLiteral("HDMI-1")), QPoint(0, 0));
        QCOMPARE(scaler.positions.value(QStringLiteral("DP-1")), QPoint(1280, 0));
        QCOMPARE(settings.outputPosition(QStringLiteral("Maker Model HDMI-1")), QPoint(0, 0));
        QCOMPARE(settings.outputPosition(QStringLiteral("Maker Model DP-1")), QPoint(1280, 0));
        QVERIFY(scaler.modeRequests.isEmpty());
        QVERIFY(scaler.requests.isEmpty());

        // A smaller Desktop Area on the left one (HDMI-1, still selected)
        // pulls the right one along.
        dialog.desktopAreaSlider()->setValue(0);
        QCOMPARE(dialog.monitorArrangement()->monitors().at(1).geometry, QRect(0, 0, 1024, 768));
        QCOMPARE(dialog.monitorArrangement()->monitors().at(0).geometry.topLeft(), QPoint(1024, 0));
    }

    void arrangementRevertsWhenNotConfirmed()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        scaler.addSecondScreen();
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        dialog.setScaleConfirmation([](const std::function<void(bool)> &done) { done(false); });
        dialog.prepare();

        Q_EMIT dialog.monitorArrangement()->monitorMoved(1, QPoint(0, 1080));
        dialog.apply();
        QCOMPARE(scaler.batches.size(), 2);
        QCOMPARE(scaler.positions.value(QStringLiteral("HDMI-1")), QPoint(1920, 0));
        QCOMPARE(scaler.positions.value(QStringLiteral("DP-1")), QPoint(0, 0));
        QCOMPARE(dialog.monitorArrangement()->monitors().at(1).geometry.topLeft(), QPoint(1920, 0));
        QVERIFY(!settings.outputPosition(QStringLiteral("Maker Model HDMI-1")));
        QVERIFY(!dialog.applyButton()->isEnabled());
    }

    void extendTurnsScreenOff()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        scaler.addSecondScreen();
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("HDMI-1"));
        dialog.setScaleConfirmation([](const std::function<void(bool)> &done) { done(true); });
        dialog.prepare();
        QCOMPARE(dialog.displayBox()->currentIndex(), 1);

        dialog.extendBox()->setChecked(false);
        // Its settings cannot be changed while it is off.
        QVERIFY(!dialog.desktopAreaSlider()->isEnabled());
        QVERIFY(!dialog.fontSizeBox()->isEnabled());
        QVERIFY(!dialog.primaryBox()->isEnabled());
        QVERIFY(!dialog.monitorArrangement()->monitors().at(1).enabled);
        dialog.apply();
        QCOMPARE(scaler.batches.size(), 1);
        QCOMPARE(scaler.batches.first().size(), 1);
        QCOMPARE(scaler.batches.first().first().screen, QStringLiteral("HDMI-1"));
        QCOMPARE(scaler.batches.first().first().enabled, std::optional<bool>(false));
        QVERIFY(scaler.disabled.contains(QStringLiteral("HDMI-1")));
        QVERIFY(!settings.outputEnabled(QStringLiteral("Maker Model HDMI-1")));
        QVERIFY(settings.outputEnabled(QStringLiteral("Maker Model DP-1")));

        // Back on: beside the desktop, in its preferred mode.
        dialog.prepare();
        dialog.displayBox()->setCurrentIndex(1);
        QVERIFY(!dialog.extendBox()->isChecked());
        dialog.extendBox()->setChecked(true);
        QVERIFY(dialog.desktopAreaSlider()->isEnabled());
        dialog.apply();
        const ScreenChange on = scaler.batches.last().last();
        QCOMPARE(on.screen, QStringLiteral("HDMI-1"));
        QCOMPARE(on.enabled, std::optional<bool>(true));
        QCOMPARE(modeKey(on.mode), QStringLiteral("1280x1024@60000"));
        QCOMPARE(on.position, std::optional<QPoint>(QPoint(1920, 0)));
        QVERIFY(settings.outputEnabled(QStringLiteral("Maker Model HDMI-1")));
    }

    void primaryMonitor()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        scaler.addSecondScreen();
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        int asked = 0;
        dialog.setScaleConfirmation([&asked](const std::function<void(bool)> &done) {
            ++asked;
            done(true);
        });
        dialog.prepare();

        dialog.displayBox()->setCurrentIndex(1);
        dialog.primaryBox()->setChecked(true);
        // Now primary: it cannot be unticked or switched off.
        QVERIFY(!dialog.primaryBox()->isEnabled());
        QVERIFY(!dialog.extendBox()->isEnabled());
        QVERIFY(dialog.applyButton()->isEnabled());
        dialog.apply();
        QCOMPARE(settings.primaryOutput(), QStringLiteral("Maker Model HDMI-1"));
        // Nothing for the compositor to do, so nothing to confirm.
        QCOMPARE(scaler.batches.size(), 0);
        QCOMPARE(asked, 0);
        // The other one can now be switched off.
        dialog.displayBox()->setCurrentIndex(0);
        QVERIFY(!dialog.primaryBox()->isChecked());
        QVERIFY(dialog.extendBox()->isEnabled());

        // Remembered next time.
        dialog.prepare();
        dialog.displayBox()->setCurrentIndex(1);
        QVERIFY(dialog.primaryBox()->isChecked());
    }

    void customFontSizeDialog()
    {
        CustomFontSizeDialog custom;
        QCOMPARE(custom.windowTitle(), QStringLiteral("Custom Font Size"));
        custom.setPercent(150);
        QCOMPARE(custom.percent(), 150);
        custom.setPercent(500);
        QCOMPARE(custom.percent(), 300);
        custom.setPercent(10);
        QCOMPARE(custom.percent(), 100);
        custom.percentBox()->setCurrentText(QStringLiteral("abc"));
        QCOMPARE(custom.percent(), 100);
    }

    void applyRunsThemeTool()
    {
        Settings settings(Settings::defaultFilePath());
        DisplayPropertiesDialog dialog(&settings);
        dialog.setThemeTool(QStringLiteral(DAYTONA_THEMETOOL));
        QSignalSpy applied(&dialog, &DisplayPropertiesDialog::schemeApplied);

        dialog.loadThemeFile(QStringLiteral(DAYTONA_TEST_DATA "/themes/Sample.theme"));
        dialog.apply();
        QVERIFY(applied.wait(10000));
        QCOMPARE(applied.first().first().toBool(), true);
        QVERIFY(QFile::exists(colourFilePath()));
        // The tool records the scheme name in the file; read it afresh.
        QCOMPARE(Settings(Settings::defaultFilePath()).colourScheme(), QStringLiteral("Sample"));
        QCOMPARE(settings.themeFiles().size(), 1);

        // Listed with the schemes next time.
        dialog.prepare();
        QCOMPARE(dialog.schemeBox()->currentText(), QStringLiteral("Sample"));
        QVERIFY(dialog.schemeBox()->findText(QStringLiteral("High Contrast White")) >= 0);
    }

private:
    static void writeImage(const QString &path)
    {
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        if (path.endsWith(QLatin1String(".txt"))) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            return;
        }
        QVERIFY(image.save(path, path.endsWith(QLatin1String(".JPG")) ? "JPG" : nullptr));
    }

    QTemporaryDir m_home;
};

QTEST_MAIN(TestDisplayProperties)
#include "tst_displayproperties.moc"
