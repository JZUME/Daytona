// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

class QMimeData;

namespace Daytona {

// One icon on the desktop: one of NT's system icons, or an entry in the
// Desktop folder.
struct DesktopItem
{
    enum class Kind
    {
        Computer,
        Network,
        RecycleBin,
        File
    };

    Kind kind = Kind::File;
    QString name; // the label
    QString path; // absolute path, for files
    QString iconName; // a theme icon name, or an absolute image path
    QString fallbackIconName;
    bool isDir = false;
    bool isLink = false; // a symbolic link (NT's shortcut arrow)
    bool isDesktopEntry = false; // a .desktop file, shown by its Name and Icon
    qint64 size = 0;
    QDateTime modified;
    QString typeName; // the MIME type's description, for Arrange by Type

    // What its position is remembered by: the file name, or a name starting
    // with "/" (which no file name can) for the system icons.
    QString key() const;
    bool isFile() const { return kind == Kind::File; }

    static DesktopItem system(Kind kind);
};

// The XDG Desktop directory (normally ~/Desktop).
QString desktopDirectory();
// The home trash's files directory ($XDG_DATA_HOME/Trash/files).
QString trashFilesDirectory();

// The visible entries of the Desktop folder, sorted by name. Reads the disk
// and parses .desktop files, so run it off the UI thread. localeName picks
// localised .desktop names.
QList<DesktopItem> readDesktopFolder(const QString &directory, const QString &localeName);

// Whether the trash holds anything (the Recycle Bin's full icon).
bool trashIsFull(const QString &trashFiles);

// Empties the home trash, as NT's Empty Recycle Bin. Returns the names that
// could not be removed.
QStringList emptyTrash(const QString &trashFiles);

// A name for a new entry in directory that no entry has yet: name itself
// when free, else the NT-style alternatives pattern gives, where %1 is the
// name and %2 a number from 2 ("Copy of %1", "Copy (%2) of %1"; "New Folder",
// "New Folder (%2)").
QString uniqueName(const QString &directory, const QString &name, const QString &first,
    const QString &numbered);

// The clipboard formats file managers share: text/uri-list, plus GNOME's
// x-special/gnome-copied-files ("copy" or "cut", then the URLs), which
// PCManFM-Qt, Nautilus, Thunar and Dolphin read and write.
QMimeData *fileClipboardData(const QList<QUrl> &urls, bool cut);
struct ClipboardFiles
{
    QList<QUrl> urls; // local files only
    bool cut = false;
};
ClipboardFiles readFileClipboard(const QMimeData *data);

// Copies (or moves, if move) files and folders into directory, as Paste.
// An entry already in directory is copied as "Copy of X" (and moving it
// does nothing); one whose name is taken there gets the same treatment rather
// than replacing what is there. Run it off the UI thread.
struct PasteResult
{
    QStringList created; // file names in directory
    QStringList errors; // human-readable, one per failed item
};
PasteResult pasteFiles(const QList<QUrl> &urls, const QString &directory, bool move);

// Makes symbolic links in directory to each of paths, named "Shortcut to X",
// as Paste Shortcut and Create Shortcut. Returns the created file names.
QStringList createShortcuts(const QStringList &paths, const QString &directory);

} // namespace Daytona
