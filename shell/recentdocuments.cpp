// SPDX-License-Identifier: GPL-3.0-or-later

#include "recentdocuments.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QXmlStreamReader>

namespace Daytona {

namespace {
    constexpr qint64 MaxFileSize = 8 * 1024 * 1024;
} // namespace

QString recentDocumentsFile()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("recently-used.xbel"));
}

QList<RecentDocument> readRecentDocuments(const QString &xbelPath, int limit)
{
    QList<RecentDocument> documents;
    QFile file(xbelPath);
    if (file.size() > MaxFileSize || !file.open(QIODevice::ReadOnly))
        return documents;

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() != QXmlStreamReader::StartElement
            || xml.name() != QLatin1String("bookmark")) {
            continue;
        }
        const QXmlStreamAttributes attributes = xml.attributes();
        const QUrl url(attributes.value(QLatin1String("href")).toString());
        if (!url.isValid() || !url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        QDateTime when = QDateTime::fromString(
            attributes.value(QLatin1String("modified")).toString(), Qt::ISODateWithMs);
        if (!when.isValid()) {
            when = QDateTime::fromString(
                attributes.value(QLatin1String("added")).toString(), Qt::ISODateWithMs);
        }
        documents.append({ path, when });
    }

    std::stable_sort(documents.begin(), documents.end(),
        [](const RecentDocument &a, const RecentDocument &b) { return a.modified > b.modified; });
    // Only the newest that still exist: look at files in that order and
    // stop once there are enough, rather than at every file the list has.
    QList<RecentDocument> existing;
    for (const RecentDocument &document : std::as_const(documents)) {
        if (existing.size() >= limit)
            break;
        if (QFileInfo(document.path).isFile())
            existing.append(document);
    }
    return existing;
}

} // namespace Daytona
