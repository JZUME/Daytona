// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

namespace Daytona {

struct RecentDocument
{
    QString path;
    QDateTime modified;
};

// The freedesktop recently-used file: $XDG_DATA_HOME/recently-used.xbel.
QString recentDocumentsFile();

// Reads an XBEL recently-used file and returns up to limit local files that
// still exist, most recently modified first. The file is untrusted: entries
// that are not file:// URLs, files that no longer exist and anything after
// malformed XML are skipped, and unreasonably large files are ignored.
QList<RecentDocument> readRecentDocuments(const QString &xbelPath, int limit = 15);

} // namespace Daytona
