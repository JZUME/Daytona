// SPDX-License-Identifier: GPL-3.0-or-later

// Checks pixel colours in a screenshot, for headless smoke tests.
//
// Usage: daytona-pixelcheck IMAGE X Y #RRGGBB [X Y #RRGGBB ...]
//        daytona-pixelcheck --frame IMAGE SCALE
//        daytona-pixelcheck --pointer IMAGE X Y SCALE
//        daytona-pixelcheck --count IMAGE #RRGGBB N
//        daytona-pixelcheck --find IMAGE #RRGGBB SCALE
// Exits 0 if every pixel matches exactly, 1 if any differ (reported on
// stderr), 2 on bad arguments or an unreadable image.
//
// --frame finds the smoke window's magenta client area and checks NT's
// raised window frame around it, as Daytona's labwc draws it: from the
// outside in, light grey, white, then face grey on the top and left side, and
// black, dark grey, then face grey on the bottom and right, each line SCALE
// device pixels wide.
//
// --pointer measures the mouse pointer whose hotspot is at logical X, Y over
// a plain background, and checks it is NT's size in logical pixels (the
// arrow is about 20 tall) whatever the output SCALE: a pointer drawn from
// too small an image comes out SCALE times smaller.
//
// --count checks that exactly N pixels in the image have the colour, for
// counting things of a known size and colour wherever they are drawn.
//
// --find prints the centre of the pixels of the colour, as "X Y" in logical
// pixels (device pixels divided by SCALE), to click things found by colour;
// it exits 1 if there are none.

#include <QColor>
#include <QImage>
#include <QRect>
#include <QString>
#include <cstdio>

namespace {

int checkFrame(const QImage &image, int scale)
{
    const QRgb client = qRgb(0xff, 0x00, 0xff);
    int left = image.width(), right = -1, top = image.height(), bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if ((line[x] & 0xffffff) == (client & 0xffffff)) {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    }
    if (right < 0) {
        std::fprintf(stderr, "no smoke window client area found\n");
        return 1;
    }
    const QRgb light = qRgb(0xc0, 0xc0, 0xc0), white = qRgb(0xff, 0xff, 0xff),
               face = qRgb(0xc0, 0xc0, 0xc0), shadow = qRgb(0x80, 0x80, 0x80),
               dark = qRgb(0x00, 0x00, 0x00);
    const int midY = (top + bottom) / 2;
    const int midX = (left + right) / 2;
    struct Check
    {
        int x, y;
        QRgb colour;
        const char *what;
    };
    const Check checks[] = {
        // Left side, outside in, beside the client area.
        { left - 4 * scale, midY, light, "left outer light" },
        { left - 3 * scale, midY, white, "left inner highlight" },
        { left - 2 * scale, midY, face, "left face" },
        // Right side, inside out.
        { right + 1 + scale, midY, face, "right face" },
        { right + 1 + 2 * scale, midY, shadow, "right inner shadow" },
        { right + 1 + 3 * scale, midY, dark, "right outer dark shadow" },
        // Bottom, inside out.
        { midX, bottom + 1 + scale, face, "bottom face" },
        { midX, bottom + 1 + 2 * scale, shadow, "bottom inner shadow" },
        { midX, bottom + 1 + 3 * scale, dark, "bottom outer dark shadow" },
    };
    int failures = 0;
    for (const Check &check : checks) {
        const QRgb actual
            = image.valid(check.x, check.y) ? image.pixel(check.x, check.y) & 0xffffff : 0x1000000;
        if (actual != (check.colour & 0xffffff)) {
            std::fprintf(stderr, "%s at (%d,%d): expected #%06x, got #%06x\n", check.what, check.x,
                check.y, check.colour & 0xffffff, actual);
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}

} // namespace

int checkPointer(const QImage &image, int x, int y, int scale)
{
    const QPoint origin(x * scale, y * scale);
    const QRgb background = image.pixel(origin - QPoint(4 * scale, 4 * scale));
    int left = image.width(), right = -1, top = image.height(), bottom = -1;
    const QRect area = QRect(origin - QPoint(2 * scale, 2 * scale), QSize(40 * scale, 40 * scale))
                           .intersected(image.rect());
    for (int py = area.top(); py <= area.bottom(); ++py) {
        for (int px = area.left(); px <= area.right(); ++px) {
            if (image.pixel(px, py) == background)
                continue;
            left = qMin(left, px);
            right = qMax(right, px);
            top = qMin(top, py);
            bottom = qMax(bottom, py);
        }
    }
    if (right < 0) {
        std::fprintf(stderr, "no pointer near %d,%d\n", x, y);
        return 1;
    }
    const double width = double(right - left + 1) / scale;
    const double height = double(bottom - top + 1) / scale;
    std::fprintf(stderr, "pointer %.1fx%.1f logical pixels\n", width, height);
    if (height < 16 || height > 26) {
        std::fprintf(stderr, "the pointer should be about 20 logical pixels tall\n");
        return 1;
    }
    return 0;
}

int countPixels(const QImage &image, QRgb colour, int expected)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if ((line[x] & 0xffffff) == (colour & 0xffffff))
                ++count;
        }
    }
    if (count == expected)
        return 0;
    std::fprintf(stderr, "%d pixels of the colour, expected %d\n", count, expected);
    return 1;
}

int findPixels(const QImage &image, QRgb colour, int scale)
{
    QRect found;
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if ((line[x] & 0xffffff) == (colour & 0xffffff))
                found |= QRect(x, y, 1, 1);
        }
    }
    if (found.isEmpty()) {
        std::fprintf(stderr, "no pixels of the colour\n");
        return 1;
    }
    std::printf("%d %d\n", found.center().x() / scale, found.center().y() / scale);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc == 6 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--pointer")) {
        const QImage image
            = QImage(QString::fromLocal8Bit(argv[2])).convertToFormat(QImage::Format_RGB32);
        bool okX = false, okY = false, okScale = false;
        const int x = QString::fromLocal8Bit(argv[3]).toInt(&okX);
        const int y = QString::fromLocal8Bit(argv[4]).toInt(&okY);
        const int scale = QString::fromLocal8Bit(argv[5]).toInt(&okScale);
        if (image.isNull() || !okX || !okY || !okScale || scale < 1) {
            std::fprintf(stderr, "usage: %s --pointer IMAGE X Y SCALE\n", argv[0]);
            return 2;
        }
        return checkPointer(image, x, y, scale);
    }
    if (argc == 5 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--count")) {
        const QImage image
            = QImage(QString::fromLocal8Bit(argv[2])).convertToFormat(QImage::Format_RGB32);
        const QColor colour = QColor::fromString(QString::fromLocal8Bit(argv[3]));
        bool ok = false;
        const int expected = QString::fromLocal8Bit(argv[4]).toInt(&ok);
        if (image.isNull() || !colour.isValid() || !ok || expected < 0) {
            std::fprintf(stderr, "usage: %s --count IMAGE #RRGGBB N\n", argv[0]);
            return 2;
        }
        return countPixels(image, colour.rgb(), expected);
    }
    if (argc == 5 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--find")) {
        const QImage image
            = QImage(QString::fromLocal8Bit(argv[2])).convertToFormat(QImage::Format_RGB32);
        const QColor colour = QColor::fromString(QString::fromLocal8Bit(argv[3]));
        bool ok = false;
        const int scale = QString::fromLocal8Bit(argv[4]).toInt(&ok);
        if (image.isNull() || !colour.isValid() || !ok || scale < 1) {
            std::fprintf(stderr, "usage: %s --find IMAGE #RRGGBB SCALE\n", argv[0]);
            return 2;
        }
        return findPixels(image, colour.rgb(), scale);
    }
    if (argc == 4 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--frame")) {
        const QImage image
            = QImage(QString::fromLocal8Bit(argv[2])).convertToFormat(QImage::Format_RGB32);
        bool ok = false;
        const int scale = QString::fromLocal8Bit(argv[3]).toInt(&ok);
        if (image.isNull() || !ok || scale < 1) {
            std::fprintf(stderr, "usage: %s --frame IMAGE SCALE\n", argv[0]);
            return 2;
        }
        return checkFrame(image, scale);
    }
    if (argc < 5 || (argc - 2) % 3 != 0) {
        std::fprintf(stderr, "usage: %s IMAGE X Y #RRGGBB [X Y #RRGGBB ...]\n", argv[0]);
        return 2;
    }

    const QImage image(QString::fromLocal8Bit(argv[1]));
    if (image.isNull()) {
        std::fprintf(stderr, "cannot read image %s\n", argv[1]);
        return 2;
    }

    int failures = 0;
    for (int i = 2; i + 2 < argc; i += 3) {
        bool okX = false;
        bool okY = false;
        const int x = QString::fromLocal8Bit(argv[i]).toInt(&okX);
        const int y = QString::fromLocal8Bit(argv[i + 1]).toInt(&okY);
        const QColor expected = QColor::fromString(QString::fromLocal8Bit(argv[i + 2]));
        if (!okX || !okY || !expected.isValid()) {
            std::fprintf(stderr, "bad check: %s %s %s\n", argv[i], argv[i + 1], argv[i + 2]);
            return 2;
        }
        if (!image.valid(x, y)) {
            std::fprintf(stderr, "(%d,%d) is outside the %dx%d image\n", x, y, image.width(),
                image.height());
            ++failures;
            continue;
        }
        const QColor actual = image.pixelColor(x, y);
        if (actual.rgb() != expected.rgb()) {
            std::fprintf(stderr, "(%d,%d): expected %s, got %s\n", x, y,
                qPrintable(expected.name()), qPrintable(actual.name()));
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
