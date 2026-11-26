// SPDX-License-Identifier: GPL-3.0-or-later

#include "xcursor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

constexpr QRgb Black = 0xff000000;
constexpr QRgb White = 0xffffffff;

// A small arrow-like shape: a white body with a 1 pixel black outline,
// and a 1 pixel black line of its own at the bottom right.
XcursorImage sample()
{
    XcursorImage cursor;
    cursor.nominal = 8;
    cursor.image = QImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    cursor.image.fill(Qt::transparent);
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            const bool edge = x == 0 || y == 0 || x == 4 || y == 4;
            cursor.image.setPixel(x, y, edge ? Black : White);
        }
    }
    cursor.image.setPixel(7, 6, Black); // a lone stroke
    cursor.image.setPixel(7, 7, Black);
    cursor.hotspot = QPoint(0, 0);
    return cursor;
}

int countColour(const QImage &image, QRgb colour)
{
    int n = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            n += image.pixel(x, y) == colour;
    return n;
}

} // namespace

class TestXcursor : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTrip()
    {
        XcursorImage a = sample();
        a.delay = 50;
        a.hotspot = QPoint(2, 3);
        XcursorImage b = scaleCursorImage(sample(), 16);
        const QList<XcursorImage> read = readXcursor(writeXcursor({ a, b }));
        QCOMPARE(read.size(), 2);
        QCOMPARE(read[0].nominal, 8);
        QCOMPARE(read[0].hotspot, QPoint(2, 3));
        QCOMPARE(read[0].delay, 50u);
        QCOMPARE(read[0].image, a.image);
        QCOMPARE(read[1].nominal, 16);
        QCOMPARE(read[1].image.size(), QSize(16, 16));
    }

    void rejectsBadData()
    {
        QString error;
        QVERIFY(readXcursor(QByteArrayLiteral("not a cursor"), &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QByteArray truncated = writeXcursor({ sample() });
        truncated.chop(10);
        QVERIFY(readXcursor(truncated, &error).isEmpty());
    }

    void nearestSize()
    {
        XcursorImage small = sample();
        XcursorImage big = scaleCursorImage(sample(), 32);
        const QList<XcursorImage> all { small, big };
        QCOMPARE(imagesForSize(all, 8).first().nominal, 8);
        QCOMPARE(imagesForSize(all, 30).first().nominal, 32);
        QCOMPARE(imagesForSize(all, 12).first().nominal, 8);
    }

    void sameSizeIsUnchanged()
    {
        const XcursorImage s = sample();
        const XcursorImage same = scaleCursorImage(s, 8);
        QCOMPARE(same.image, s.image);
        QCOMPARE(same.hotspot, s.hotspot);
    }

    void thinOutline()
    {
        // At 3 times the size the body is 3 times larger but the outline
        // stays 1 pixel: the 3x3 white body becomes 9x9, ringed once.
        const XcursorImage big = scaleCursorImage(sample(), 24);
        QCOMPARE(big.image.size(), QSize(24, 24));
        QCOMPARE(countColour(big.image, White), 9 * 9);
        QCOMPARE(big.image.pixel(3, 3), White); // body starts at 3
        QCOMPARE(big.image.pixel(2, 2), Black); // 1 pixel ring
        QCOMPARE(qAlpha(big.image.pixel(1, 1)), 0); // not a thick outline
        QCOMPARE(qAlpha(big.image.pixel(0, 0)), 0);
        QCOMPARE(big.image.pixel(12, 12), Black); // ring on the far side
        QCOMPARE(qAlpha(big.image.pixel(13, 13)), 0);
        // The lone stroke is enlarged, not lost: 2 pixels become 2 blocks.
        int stroke = 0;
        for (int y = 18; y < 24; ++y)
            for (int x = 21; x < 24; ++x)
                stroke += big.image.pixel(x, y) == Black;
        QCOMPARE(stroke, 2 * 9);
        // The hotspot was the outline's corner, now transparent: it moves
        // onto the shape.
        QVERIFY(qAlpha(big.image.pixel(big.hotspot)) > 0);
        QVERIFY((big.hotspot - QPoint(2, 2)).manhattanLength() <= 1);
    }

    void fractionalScale()
    {
        const XcursorImage mid = scaleCursorImage(sample(), 12);
        QCOMPARE(mid.image.size(), QSize(12, 12));
        QVERIFY(countColour(mid.image, White) > 9);
        QVERIFY(qAlpha(mid.image.pixel(mid.hotspot)) > 0);
    }

    void schemeSizes()
    {
        QCOMPARE(pointerSchemeSize(PointerScheme::Standard), 32);
        QCOMPARE(pointerSchemeSize(PointerScheme::Large), 64);
        QCOMPARE(pointerSchemeSize(PointerScheme::ExtraLarge), 96);
        // Every scheme at screen scales 1 and 2 has its own size.
        const QList<int> sizes = cursorThemeSizes();
        for (PointerScheme s :
            { PointerScheme::Standard, PointerScheme::Large, PointerScheme::ExtraLarge }) {
            QVERIFY(sizes.contains(pointerSchemeSize(s)));
            QVERIFY(sizes.contains(pointerSchemeSize(s) * 2));
        }
    }

    void generateTheme()
    {
        QTemporaryDir source, target;
        const QByteArray arrow = writeXcursor({ sample() });
        XcursorImage other = sample();
        other.image.setPixel(2, 2, Black);
        write(source.filePath(QStringLiteral("left_ptr")), arrow);
        write(source.filePath(QStringLiteral("default")), arrow); // a copy
        write(source.filePath(QStringLiteral("watch")), writeXcursor({ other }));
        write(source.filePath(QStringLiteral("X_cursor")), writeXcursor({ other }));

        QString error;
        QVERIFY2(generateCursorTheme(source.path(), target.path(),
                     { { QStringLiteral("X_cursor"), QStringLiteral("left_ptr") } }, &error),
            qPrintable(error));
        const QDir cursors(target.filePath(QStringLiteral("cursors")));
        // Copies are linked to the first of them (by name), not written again.
        QVERIFY(!QFileInfo(cursors.filePath(QStringLiteral("default"))).isSymLink());
        QCOMPARE(QFileInfo(cursors.filePath(QStringLiteral("left_ptr"))).symLinkTarget(),
            cursors.filePath(QStringLiteral("default")));
        QVERIFY(!QFileInfo(cursors.filePath(QStringLiteral("watch"))).isSymLink());
        // A replaced name shows another cursor.
        QCOMPARE(QFileInfo(cursors.filePath(QStringLiteral("X_cursor"))).symLinkTarget(),
            cursors.filePath(QStringLiteral("left_ptr")));

        QFile file(cursors.filePath(QStringLiteral("left_ptr")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QList<XcursorImage> images = readXcursor(file.readAll());
        QList<int> nominal;
        for (const XcursorImage &image : images)
            nominal << image.nominal;
        QCOMPARE(nominal, cursorThemeSizes());

        QFile index(target.filePath(QStringLiteral("index.theme")));
        QVERIFY(index.open(QIODevice::ReadOnly));
        QVERIFY(index.readAll().contains("Name=Daytona Cursors"));
    }

    void replacedNamesCoverTheLogo()
    {
        // The Windows logo parody in Chicago95 is never shipped.
        const QStringList names = replacedCursorNames();
        QVERIFY(names.contains(QStringLiteral("X_cursor")));
        QVERIFY(names.contains(QStringLiteral("X-cursor")));
    }

    void findsCursorFiles()
    {
        QTemporaryDir icons;
        const QString dir = icons.filePath(QStringLiteral("Some Theme/cursors"));
        QVERIFY(QDir().mkpath(dir));
        write(dir + QStringLiteral("/left_ptr"), writeXcursor({ sample() }));
        qputenv("XCURSOR_PATH", QFile::encodeName(QStringLiteral("/nonexistent:") + icons.path()));
        QCOMPARE(findCursorFile(QStringLiteral("Some Theme"), QStringLiteral("left_ptr")),
            dir + QStringLiteral("/left_ptr"));
        QVERIFY(findCursorFile(QStringLiteral("Some Theme"), QStringLiteral("watch")).isEmpty());
        qunsetenv("XCURSOR_PATH");
    }

private:
    static void write(const QString &path, const QByteArray &data)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }
};

QTEST_GUILESS_MAIN(TestXcursor)
#include "tst_xcursor.moc"
