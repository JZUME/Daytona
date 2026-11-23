// SPDX-License-Identifier: GPL-3.0-or-later

// Checks pixel colours in a screenshot, for headless smoke tests.
//
// Usage: daytona-pixelcheck IMAGE X Y #RRGGBB [X Y #RRGGBB ...]
// Exits 0 if every pixel matches exactly, 1 if any differ (reported on
// stderr), 2 on bad arguments or an unreadable image.

#include <QColor>
#include <QImage>
#include <QString>
#include <cstdio>

int main(int argc, char *argv[])
{
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
