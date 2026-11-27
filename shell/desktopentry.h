// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>
#include <optional>

namespace Daytona {

// An application's .desktop file, following the freedesktop Desktop Entry
// specification as far as the Start menu needs. Contents are untrusted.
struct DesktopEntry
{
    QString id; // desktop-file ID, e.g. "org.example.App.desktop"
    QString filePath; // the file it was read from
    QString type;
    QString name; // localised
    QString comment; // localised
    QString icon;
    QString exec;
    QString tryExec;
    QString workingDirectory; // "Path" key
    bool terminal = false;
    bool noDisplay = false;
    bool hidden = false;
    QStringList categories;
    QStringList onlyShowIn;
    QStringList notShowIn;

    // Parses the [Desktop Entry] group. localeName is a POSIX locale such as
    // "de_DE.UTF-8@euro"; it chooses localised Name values. Returns nothing
    // for files that cannot be read or have no [Desktop Entry] group.
    static std::optional<DesktopEntry> parse(
        const QString &filePath, const QString &id, const QString &localeName);

    // Whether the Start menu should list this entry, for a desktop named
    // desktopName ("Daytona"). Checks Type, NoDisplay, Hidden, OnlyShowIn,
    // NotShowIn and TryExec.
    bool shouldShow(const QString &desktopName) const;

    // Expands Exec field codes into a program and arguments, without any
    // shell. files are local paths or URLs passed for %f/%F/%u/%U. Returns
    // nothing if Exec is empty or malformed.
    std::optional<QStringList> commandLine(const QStringList &files = {}) const;
};

// Locale keys to try for localised values, most specific first, per the
// Desktop Entry specification: lang_COUNTRY@MODIFIER, lang_COUNTRY,
// lang@MODIFIER, lang.
QStringList localeKeys(const QString &localeName);

} // namespace Daytona
