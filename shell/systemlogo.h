// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QIcon>
#include <QString>

namespace Daytona {

// The fields of an os-release file (KEY=value lines, values optionally
// quoted), as in /etc/os-release.
QHash<QString, QString> parseOsRelease(const QString &text);

// The operating system's own logo, for the Start button, as NT's Start
// button showed its own system's logo:
//
// 1. the icon named by os-release's LOGO field, from the icon theme or
//    /usr/share/pixmaps;
// 2. otherwise a logo file the distribution is known to install (Debian
//    sets no LOGO field, but every Debian system has its logo);
// 3. otherwise a null icon, and the caller uses Daytona's own logo.
//
// Only the distribution's own names are looked up, never generic ones such
// as "distributor-logo", which icon themes may draw however they like.
// root is prefixed to every path (for tests).
QIcon systemLogo(const QString &root = QString());

} // namespace Daytona
