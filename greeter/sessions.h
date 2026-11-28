// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace Daytona {

// A desktop session a user can log on to: a .desktop file in a
// wayland-sessions directory, as display managers read them.
struct DesktopSession
{
    // The file name without .desktop ("daytona"), which is
    // XDG_SESSION_DESKTOP.
    QString id;
    // Name, localised.
    QString name;
    // Exec, split into a program and its arguments.
    QStringList command;
    // DesktopNames, which is XDG_CURRENT_DESKTOP (colon separated).
    QStringList desktopNames;

    // What greetd's start_session is given besides the command.
    QStringList environment() const;
};

// Reads one session file. Returns false for a file that is not a usable
// session: no [Desktop Entry], no Exec, Hidden or NoDisplay, or a TryExec
// program that is not installed. locale is a POSIX locale name such as
// "de_DE.UTF-8", for the localised Name.
bool readSessionFile(const QString &path, const QString &locale, DesktopSession *session);

// Every session in dir/wayland-sessions for each of dataDirs, in the order
// given (the first directory with a file id wins), sorted by name.
QList<DesktopSession> findSessions(const QStringList &dataDirs, const QString &locale);

// XDG_DATA_DIRS, or its default.
QStringList systemDataDirs();

// Splits a desktop entry's Exec value into arguments, following the Desktop
// Entry Specification's quoting, and drops field codes (%f, %U and the
// rest), which mean nothing for a session. Empty if malformed.
QStringList splitExec(const QString &exec);

// Which session to select: the remembered one if it is still installed,
// else Daytona, else the first. -1 if there are none.
int defaultSession(const QList<DesktopSession> &sessions, const QString &remembered);

} // namespace Daytona
