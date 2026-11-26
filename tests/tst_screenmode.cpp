// SPDX-License-Identifier: GPL-3.0-or-later

#include "screenmode.h"

#include <QTest>

using namespace Daytona;

namespace {

ScreenMode mode(int w, int h, int refresh, bool preferred = false)
{
    ScreenMode m;
    m.size = QSize(w, h);
    m.refresh = refresh;
    m.preferred = preferred;
    return m;
}

} // namespace

class TestScreenMode : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keys()
    {
        QCOMPARE(modeKey(mode(1920, 1080, 59940)), QStringLiteral("1920x1080@59940"));
        QCOMPARE(modeKey(mode(800, 600, 0)), QStringLiteral("800x600"));
        const ScreenMode parsed = parseModeKey(QStringLiteral(" 2560x1440@144000 "));
        QCOMPARE(parsed.size, QSize(2560, 1440));
        QCOMPARE(parsed.refresh, 144000);
        QCOMPARE(parseModeKey(QStringLiteral("1024x768")).refresh, 0);
        for (const char *bad : { "", "1024", "0x0", "axb", "1024x768@", "1024x768@60Hz" })
            QVERIFY2(!parseModeKey(QString::fromLatin1(bad)).isValid(), bad);
    }

    void matching()
    {
        const QList<ScreenMode> modes { mode(1920, 1080, 144000), mode(1920, 1080, 60000, true),
            mode(1024, 768, 60000), mode(1024, 768, 75000) };
        // Exact.
        QCOMPARE(modeKey(matchMode(modes, mode(1920, 1080, 144000))),
            QStringLiteral("1920x1080@144000"));
        // No rate, or one not offered: the size's preferred mode...
        QCOMPARE(modeKey(matchMode(modes, mode(1920, 1080, 0))), QStringLiteral("1920x1080@60000"));
        QCOMPARE(
            modeKey(matchMode(modes, mode(1920, 1080, 50000))), QStringLiteral("1920x1080@60000"));
        // ...else its fastest.
        QCOMPARE(modeKey(matchMode(modes, mode(1024, 768, 0))), QStringLiteral("1024x768@75000"));
        // A size not offered.
        QVERIFY(!matchMode(modes, mode(640, 480, 60000)).isValid());
    }
};

QTEST_GUILESS_MAIN(TestScreenMode)
#include "tst_screenmode.moc"
