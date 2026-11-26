// SPDX-License-Identifier: GPL-3.0-or-later

#include "xcursor.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QtEndian>
#include <cmath>
#include <limits>

namespace Daytona {

namespace {

    constexpr quint32 Magic = 0x72756358; // "Xcur", little-endian
    constexpr quint32 FileHeaderSize = 16;
    constexpr quint32 FileVersion = 0x10000;
    constexpr quint32 ImageType = 0xfffd0002;
    constexpr quint32 ImageHeaderSize = 36;
    constexpr quint32 ImageVersion = 1;
    constexpr int MaxDimension = 0x7fff;

    quint32 u32(const QByteArray &data, qsizetype offset)
    {
        return qFromLittleEndian<quint32>(data.constData() + offset);
    }

    void put32(QByteArray &data, quint32 value)
    {
        char bytes[4];
        qToLittleEndian(value, bytes);
        data.append(bytes, 4);
    }

    bool fail(QString *error, const QString &message)
    {
        if (error)
            *error = message;
        return false;
    }

} // namespace

QList<XcursorImage> readXcursor(const QByteArray &data, QString *error)
{
    if (data.size() < qsizetype(FileHeaderSize) || u32(data, 0) != Magic) {
        fail(error, QStringLiteral("not an Xcursor file"));
        return {};
    }
    const quint32 header = u32(data, 4);
    const quint32 count = u32(data, 12);
    if (header < FileHeaderSize || count > 4096
        || quint64(header) + quint64(count) * 12 > quint64(data.size())) {
        fail(error, QStringLiteral("damaged Xcursor table of contents"));
        return {};
    }
    QList<XcursorImage> images;
    for (quint32 i = 0; i < count; ++i) {
        const qsizetype entry = header + i * 12;
        if (u32(data, entry) != ImageType)
            continue; // comments and other chunks
        const quint32 position = u32(data, entry + 8);
        if (quint64(position) + ImageHeaderSize > quint64(data.size())) {
            fail(error, QStringLiteral("damaged Xcursor image header"));
            return {};
        }
        const quint32 width = u32(data, position + 16);
        const quint32 height = u32(data, position + 20);
        if (width == 0 || height == 0 || width > MaxDimension || height > MaxDimension
            || quint64(position) + ImageHeaderSize + quint64(width) * height * 4
                > quint64(data.size())) {
            fail(error, QStringLiteral("damaged Xcursor image"));
            return {};
        }
        XcursorImage image;
        image.nominal = int(u32(data, position + 8));
        image.hotspot = QPoint(int(u32(data, position + 24)), int(u32(data, position + 28)));
        image.delay = u32(data, position + 32);
        image.image = QImage(int(width), int(height), QImage::Format_ARGB32_Premultiplied);
        const char *pixels = data.constData() + position + ImageHeaderSize;
        for (int y = 0; y < int(height); ++y) {
            auto *line = reinterpret_cast<quint32 *>(image.image.scanLine(y));
            for (int x = 0; x < int(width); ++x)
                line[x] = qFromLittleEndian<quint32>(pixels + (qsizetype(y) * width + x) * 4);
        }
        images.append(image);
    }
    if (images.isEmpty())
        fail(error, QStringLiteral("no images in Xcursor file"));
    return images;
}

QByteArray writeXcursor(const QList<XcursorImage> &images)
{
    QByteArray data;
    put32(data, Magic);
    put32(data, FileHeaderSize);
    put32(data, FileVersion);
    put32(data, quint32(images.size()));
    quint32 position = FileHeaderSize + quint32(images.size()) * 12;
    for (const XcursorImage &image : images) {
        put32(data, ImageType);
        put32(data, quint32(image.nominal));
        put32(data, position);
        position += ImageHeaderSize + quint32(image.image.width() * image.image.height() * 4);
    }
    for (const XcursorImage &image : images) {
        const QImage pixels = image.image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        put32(data, ImageHeaderSize);
        put32(data, ImageType);
        put32(data, quint32(image.nominal));
        put32(data, ImageVersion);
        put32(data, quint32(pixels.width()));
        put32(data, quint32(pixels.height()));
        put32(data, quint32(image.hotspot.x()));
        put32(data, quint32(image.hotspot.y()));
        put32(data, image.delay);
        for (int y = 0; y < pixels.height(); ++y) {
            const auto *line = reinterpret_cast<const quint32 *>(pixels.constScanLine(y));
            for (int x = 0; x < pixels.width(); ++x)
                put32(data, line[x]);
        }
    }
    return data;
}

QList<XcursorImage> imagesForSize(const QList<XcursorImage> &images, int nominal)
{
    int best = -1;
    int bestDistance = std::numeric_limits<int>::max();
    for (const XcursorImage &image : images) {
        const int distance = std::abs(image.nominal - nominal);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = image.nominal;
        }
    }
    QList<XcursorImage> result;
    for (const XcursorImage &image : images) {
        if (image.nominal == best)
            result.append(image);
    }
    return result;
}

XcursorImage scaleCursorImage(const XcursorImage &source, int nominal)
{
    const QImage in = source.image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = in.width();
    const int h = in.height();
    const double factor = double(nominal) / double(source.nominal > 0 ? source.nominal : w);
    const int outW = qMax(1, int(std::lround(w * factor)));
    const int outH = qMax(1, int(std::lround(h * factor)));

    const auto opaque = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < w && y < h && qAlpha(in.pixel(x, y)) > 0;
    };
    // Edge: opaque with a transparent side. Body: opaque and not an edge.
    QList<bool> edge(w * h), body(w * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!opaque(x, y))
                continue;
            const bool isEdge
                = !opaque(x - 1, y) || !opaque(x + 1, y) || !opaque(x, y - 1) || !opaque(x, y + 1);
            edge[y * w + x] = isEdge;
            body[y * w + x] = !isEdge;
        }
    }
    // An edge pixel is part of the outline around a body when a body pixel
    // is within two pixels (which takes in an arrow's tip); other edge
    // pixels are strokes of their own and are enlarged as they are.
    QList<bool> stroke(w * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!edge[y * w + x])
                continue;
            bool nearBody = false;
            for (int dy = -2; dy <= 2 && !nearBody; ++dy) {
                for (int dx = -2; dx <= 2 && !nearBody; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    nearBody = nx >= 0 && ny >= 0 && nx < w && ny < h && body[ny * w + nx];
                }
            }
            stroke[y * w + x] = !nearBody;
        }
    }

    const auto sourceOf = [&](int x, int y) {
        return QPoint(qMin(w - 1, int(x / factor)), qMin(h - 1, int(y / factor)));
    };
    QImage out(outW, outH, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QList<char> fromBody(outW * outH);
    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            const QPoint s = sourceOf(x, y);
            const int i = s.y() * w + s.x();
            if (body[i] || stroke[i]) {
                out.setPixel(x, y, in.pixel(s));
                fromBody[y * outW + x] = body[i];
            }
        }
    }
    // A 1 pixel outline around the enlarged body, inside the area the
    // original outline covered, in that outline's colours.
    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            if (qAlpha(out.pixel(x, y)) > 0)
                continue;
            const QPoint s = sourceOf(x, y);
            if (!edge[s.y() * w + s.x()])
                continue;
            bool touches = false;
            for (int dy = -1; dy <= 1 && !touches; ++dy) {
                for (int dx = -1; dx <= 1 && !touches; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    touches
                        = nx >= 0 && ny >= 0 && nx < outW && ny < outH && fromBody[ny * outW + nx];
                }
            }
            if (touches)
                out.setPixel(x, y, in.pixel(s));
        }
    }

    // The hotspot: the same point of the shape, moved onto the nearest
    // drawn pixel if the thinner outline left it on a transparent one.
    QPoint hot(qMin(outW - 1, int((source.hotspot.x() + 0.5) * factor)),
        qMin(outH - 1, int((source.hotspot.y() + 0.5) * factor)));
    if (qAlpha(out.pixel(hot)) == 0) {
        int bestDistance = std::numeric_limits<int>::max();
        QPoint best = hot;
        const int reach = int(std::ceil(factor)) + 2;
        for (int dy = -reach; dy <= reach; ++dy) {
            for (int dx = -reach; dx <= reach; ++dx) {
                const QPoint p = hot + QPoint(dx, dy);
                if (p.x() < 0 || p.y() < 0 || p.x() >= outW || p.y() >= outH
                    || qAlpha(out.pixel(p)) == 0)
                    continue;
                const int distance = dx * dx + dy * dy;
                if (distance < bestDistance) {
                    bestDistance = distance;
                    best = p;
                }
            }
        }
        hot = best;
    }

    XcursorImage result;
    result.nominal = nominal;
    result.image = out;
    result.hotspot = hot;
    result.delay = source.delay;
    return result;
}

int pointerSchemeSize(PointerScheme scheme)
{
    switch (scheme) {
    case PointerScheme::Large:
        return 64;
    case PointerScheme::ExtraLarge:
        return 96;
    case PointerScheme::Standard:
        break;
    }
    return 32;
}

QList<int> cursorThemeSizes()
{
    return { 32, 48, 64, 96, 128, 192 };
}

QString findCursorFile(const QString &theme, const QString &name)
{
    QStringList dirs;
    const QString home = QDir::homePath();
    const QString path = qEnvironmentVariable("XCURSOR_PATH");
    if (!path.isEmpty()) {
        for (QString dir : path.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
            if (dir.startsWith(QLatin1Char('~')))
                dir.replace(0, 1, home);
            dirs << dir;
        }
    } else {
        const QString dataHome
            = qEnvironmentVariable("XDG_DATA_HOME", home + QStringLiteral("/.local/share"));
        dirs << dataHome + QStringLiteral("/icons") << home + QStringLiteral("/.icons");
    }
    const QStringList dataDirs
        = qEnvironmentVariable("XDG_DATA_DIRS", QStringLiteral("/usr/local/share:/usr/share"))
              .split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &dir : dataDirs)
        dirs << dir + QStringLiteral("/icons");
    for (const QString &dir : std::as_const(dirs)) {
        const QString file = dir + QLatin1Char('/') + theme + QStringLiteral("/cursors/") + name;
        if (QFileInfo(file).isFile())
            return file;
    }
    return {};
}

QStringList replacedCursorNames()
{
    return { QStringLiteral("X-cursor"), QStringLiteral("X_cursor"), QStringLiteral("boat"),
        QStringLiteral("coffee_mug"), QStringLiteral("pirate"), QStringLiteral("sailboat"),
        QStringLiteral("shuttle"), QStringLiteral("spider,"), QStringLiteral("umbrella") };
}

bool generateCursorTheme(const QString &sourceCursorsDir, const QString &targetDir,
    const QHash<QString, QString> &replace, QString *error)
{
    const QDir source(sourceCursorsDir);
    const QDir target(targetDir);
    if (!source.exists())
        return fail(error, QStringLiteral("%1 does not exist").arg(sourceCursorsDir));
    if (!QDir().mkpath(target.filePath(QStringLiteral("cursors"))))
        return fail(error, QStringLiteral("cannot create %1/cursors").arg(targetDir));
    const QDir cursors(target.filePath(QStringLiteral("cursors")));

    // Identical files (Chicago95 copies a cursor under each of its names)
    // are written once; the other names link to the first.
    QHash<QByteArray, QString> written;
    const QStringList names = source.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &name : names) {
        if (replace.contains(name))
            continue; // linked below
        QFile in(source.filePath(name));
        if (!in.open(QIODevice::ReadOnly))
            return fail(error, QStringLiteral("cannot read %1").arg(in.fileName()));
        const QByteArray data = in.readAll();
        const QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
        const QString path = cursors.filePath(name);
        QFile::remove(path);
        if (written.contains(hash)) {
            if (!QFile::link(written.value(hash), path))
                return fail(error, QStringLiteral("cannot link %1").arg(path));
            continue;
        }
        QString why;
        const QList<XcursorImage> images = readXcursor(data, &why);
        if (images.isEmpty())
            return fail(error, QStringLiteral("%1: %2").arg(in.fileName(), why));
        // Every size from the source's own (largest) images.
        const QList<XcursorImage> base = imagesForSize(images, std::numeric_limits<int>::max());
        QList<XcursorImage> out;
        for (int size : cursorThemeSizes()) {
            for (const XcursorImage &frame : base)
                out.append(scaleCursorImage(frame, size));
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(writeXcursor(out)) < 0 || !file.commit())
            return fail(error, QStringLiteral("cannot write %1").arg(path));
        written.insert(hash, name);
    }

    for (auto it = replace.constBegin(); it != replace.constEnd(); ++it) {
        if (!QFileInfo::exists(cursors.filePath(it.value())))
            return fail(error,
                QStringLiteral("%1 replaces %2, which does not exist").arg(it.value(), it.key()));
        const QString path = cursors.filePath(it.key());
        QFile::remove(path);
        if (!QFile::link(it.value(), path))
            return fail(error, QStringLiteral("cannot link %1").arg(path));
    }

    QSaveFile index(target.filePath(QStringLiteral("index.theme")));
    const QByteArray text = "[Icon Theme]\nName=" + QByteArray(CursorThemeName)
        + "\nComment=Chicago95's Windows 95 cursors at every Daytona pointer scheme and "
          "screen scale\n";
    if (!index.open(QIODevice::WriteOnly) || index.write(text) < 0 || !index.commit())
        return fail(error, QStringLiteral("cannot write %1").arg(index.fileName()));
    return true;
}

} // namespace Daytona
