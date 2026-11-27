// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "desktopentry.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace Daytona {

// One item of the Start menu's Programs tree.
struct MenuNode
{
    enum class Kind
    {
        Folder,
        Entry,
        Separator
    };
    Kind kind = Kind::Folder;
    QString name; // a folder's display name
    DesktopEntry entry; // an entry's application
    QList<MenuNode> children; // a folder's contents, in display order
};

// Where menu files, applications and .directory files are looked for. Each
// list is most important first.
struct XdgMenuPaths
{
    QStringList configDirs; // $XDG_CONFIG_HOME, then $XDG_CONFIG_DIRS
    QStringList dataDirs; // $XDG_DATA_HOME, then $XDG_DATA_DIRS
    static XdgMenuPaths fromEnvironment();
};

// The Daytona menu compiled into the shell, used when no menu file is
// installed (it is the same file the package installs as
// /etc/xdg/menus/daytona-applications.menu).
extern const char *const BuiltInMenuFile;

// The applications menu to read, per the Desktop Menu Specification:
// menus/${prefix}applications.menu in the first config directory that has
// it, else menus/applications.menu, else BuiltInMenuFile.
QString findApplicationsMenu(const XdgMenuPaths &paths, const QString &prefix);

struct XdgMenu
{
    MenuNode root; // Programs: the top-level <Menu>
    QStringList watchFiles; // menu and .directory files read
    QStringList watchDirs; // menu, merge, application and .directory folders
};

// Builds the Programs tree from menuFile, following the freedesktop Desktop
// Menu Specification: merging (<MergeFile>, <MergeDir>,
// <DefaultMergeDirs>), <AppDir>/<DirectoryDir> and their defaults,
// <Include>/<Exclude> rules, <OnlyUnallocated>, <Deleted>, <Move> and
// <Layout>/<DefaultLayout> (<Filename>, <Menuname>, <Separator>,
// <Merge>). Entries the Start menu should not show (see
// DesktopEntry::shouldShow) are left out, as are empty folders and folders
// whose .directory file says NoDisplay. Folders and entries without a
// <Layout> are sorted by name, folders first. Menu files are untrusted.
XdgMenu loadXdgMenu(const QString &menuFile, const XdgMenuPaths &paths, const QString &desktopName,
    const QString &localeName);

// Every entry in a tree, each application once, sorted by name: the
// Programs menu as one list, without folders.
QList<DesktopEntry> flattenMenu(const MenuNode &root);

} // namespace Daytona
