// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopfiles.h"

#include "desktopentry.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QMimeDatabase>
#include <QStandardPaths>

namespace Daytona {

namespace {

    const QString GnomeCopiedFiles = QStringLiteral("x-special/gnome-copied-files");

    QString tr(const char *text)
    {
        return QCoreApplication::translate("Daytona::Desktop", text);
    }

    // Copies a file or a folder and everything in it. Symbolic links are
    // copied as links, never followed.
    bool copyRecursively(const QString &source, const QString &target)
    {
        const QFileInfo info(source);
        if (info.isSymLink())
            return QFile::link(info.symLinkTarget(), target);
        if (!info.isDir())
            return QFile::copy(source, target);
        if (!QDir().mkdir(target))
            return false;
        const QDir from(source);
        const QStringList entries
            = from.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            if (!copyRecursively(from.filePath(entry), QDir(target).filePath(entry)))
                return false;
        }
        return true;
    }

    bool removeRecursively(const QString &path)
    {
        const QFileInfo info(path);
        if (info.isDir() && !info.isSymLink())
            return QDir(path).removeRecursively();
        return QFile::remove(path);
    }

} // namespace

QString DesktopItem::key() const
{
    switch (kind) {
    case Kind::Computer:
        return QStringLiteral("/computer");
    case Kind::Network:
        return QStringLiteral("/network");
    case Kind::RecycleBin:
        return QStringLiteral("/recyclebin");
    case Kind::File:
        break;
    }
    return QFileInfo(path).fileName();
}

DesktopItem DesktopItem::system(Kind kind)
{
    DesktopItem item;
    item.kind = kind;
    item.isDir = true;
    switch (kind) {
    case Kind::Computer:
        item.name = tr("My Computer");
        item.iconName = QStringLiteral("computer");
        break;
    case Kind::Network:
        item.name = tr("Network Neighborhood");
        item.iconName = QStringLiteral("network-workgroup");
        break;
    case Kind::RecycleBin:
        item.name = tr("Recycle Bin");
        item.iconName = QStringLiteral("user-trash");
        break;
    case Kind::File:
        break;
    }
    item.fallbackIconName = QStringLiteral("folder");
    return item;
}

QString desktopDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

QString trashFilesDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/Trash/files");
}

QList<DesktopItem> readDesktopFolder(const QString &directory, const QString &localeName)
{
    static const QMimeDatabase mimeDatabase;
    QList<DesktopItem> items;
    const QFileInfoList entries = QDir(directory).entryInfoList(
        QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &info : entries) {
        DesktopItem item;
        item.path = info.absoluteFilePath();
        item.name = info.fileName();
        item.isDir = info.isDir();
        item.isLink = info.isSymLink();
        item.size = info.isDir() ? 0 : info.size();
        item.modified = info.lastModified();
        const QMimeType type = mimeDatabase.mimeTypeForFile(info);
        item.typeName = type.comment();
        item.iconName = type.iconName();
        item.fallbackIconName = type.genericIconName();
        if (item.isDir) {
            item.iconName = QStringLiteral("folder");
            item.fallbackIconName = QStringLiteral("folder");
        }
        if (!item.isDir && info.suffix() == QLatin1String("desktop")) {
            const std::optional<DesktopEntry> entry
                = DesktopEntry::parse(item.path, info.fileName(), localeName);
            if (entry && !entry->name.isEmpty()) {
                item.isDesktopEntry = true;
                item.name = entry->name;
                if (!entry->icon.isEmpty())
                    item.iconName = entry->icon;
                item.fallbackIconName = entry->type == QLatin1String("Link")
                    ? QStringLiteral("text-html")
                    : QStringLiteral("application-x-executable");
            }
        }
        items.append(item);
    }
    return items;
}

bool trashIsFull(const QString &trashFiles)
{
    return !QDir(trashFiles)
                .isEmpty(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
}

QStringList emptyTrash(const QString &trashFiles)
{
    QStringList failed;
    QDir files(trashFiles);
    QDir trash = files;
    trash.cdUp();
    const QString info = trash.filePath(QStringLiteral("info"));
    const QStringList entries
        = files.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        if (!removeRecursively(files.filePath(entry))) {
            failed.append(entry);
            continue;
        }
        QFile::remove(QDir(info).filePath(entry + QStringLiteral(".trashinfo")));
    }
    // Sizes cached by file managers for the trashed folders.
    if (failed.isEmpty())
        QFile::remove(trash.filePath(QStringLiteral("directorysizes")));
    return failed;
}

QString uniqueName(
    const QString &directory, const QString &name, const QString &first, const QString &numbered)
{
    const QDir dir(directory);
    const auto taken = [&dir](const QString &candidate) {
        return QFileInfo::exists(dir.filePath(candidate))
            || QFileInfo(dir.filePath(candidate)).isSymLink();
    };
    if (!taken(name))
        return name;
    QString candidate = first.arg(name);
    for (int n = 2; taken(candidate); ++n)
        candidate = numbered.arg(name).arg(n);
    return candidate;
}

QMimeData *fileClipboardData(const QList<QUrl> &urls, bool cut)
{
    auto *data = new QMimeData;
    data->setUrls(urls);
    QByteArray gnome = cut ? "cut" : "copy";
    for (const QUrl &url : urls)
        gnome += '\n' + url.toEncoded();
    data->setData(GnomeCopiedFiles, gnome);
    return data;
}

ClipboardFiles readFileClipboard(const QMimeData *data)
{
    ClipboardFiles files;
    if (!data)
        return files;
    QList<QUrl> urls;
    if (data->hasFormat(GnomeCopiedFiles)) {
        const QList<QByteArray> lines = data->data(GnomeCopiedFiles).split('\n');
        if (!lines.isEmpty())
            files.cut = lines.first().trimmed() == "cut";
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const QByteArray line = lines.at(i).trimmed();
            if (!line.isEmpty())
                urls.append(QUrl::fromEncoded(line));
        }
    } else {
        urls = data->urls();
    }
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            files.urls.append(url);
    }
    return files;
}

PasteResult pasteFiles(const QList<QUrl> &urls, const QString &directory, bool move)
{
    PasteResult result;
    const QString target = QDir(directory).canonicalPath();
    for (const QUrl &url : urls) {
        const QString source = url.toLocalFile();
        const QFileInfo info(source);
        if (!info.exists() && !info.isSymLink()) {
            result.errors.append(tr("Cannot find '%1'.").arg(info.fileName()));
            continue;
        }
        const bool sameFolder = QDir(info.absolutePath()).canonicalPath() == target;
        if (move && sameFolder)
            continue;
        if (info.isDir() && !info.isSymLink()
            && (target == info.canonicalFilePath()
                || target.startsWith(info.canonicalFilePath() + QLatin1Char('/')))) {
            result.errors.append(
                tr("Cannot copy '%1': the destination folder is inside it.").arg(info.fileName()));
            continue;
        }
        const QString name
            = uniqueName(directory, info.fileName(), tr("Copy of %1"), tr("Copy (%2) of %1"));
        const QString destination = QDir(directory).filePath(name);
        bool ok = false;
        if (move) {
            ok = QFile::rename(source, destination);
            if (!ok) {
                ok = copyRecursively(source, destination);
                if (ok && !removeRecursively(source)) {
                    result.errors.append(
                        tr("'%1' was copied, but cannot be removed from where it was.")
                            .arg(info.fileName()));
                }
            }
        } else {
            ok = copyRecursively(source, destination);
        }
        if (ok) {
            result.created.append(name);
        } else {
            removeRecursively(destination);
            result.errors.append(tr("Cannot copy '%1'.").arg(info.fileName()));
        }
    }
    return result;
}

QStringList createShortcuts(const QStringList &paths, const QString &directory)
{
    QStringList created;
    for (const QString &path : paths) {
        const QString fileName = QFileInfo(path).fileName();
        const QDir dir(directory);
        QString name = tr("Shortcut to %1").arg(fileName);
        for (int n = 2;
            QFileInfo(dir.filePath(name)).exists() || QFileInfo(dir.filePath(name)).isSymLink();
            ++n)
            name = tr("Shortcut (%2) to %1").arg(fileName).arg(n);
        if (QFile::link(QFileInfo(path).absoluteFilePath(), QDir(directory).filePath(name)))
            created.append(name);
    }
    return created;
}

} // namespace Daytona
