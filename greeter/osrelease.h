// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace Daytona {

// The operating system's name, os-release's NAME ("Debian GNU/Linux",
// "Ubuntu"), from root/etc/os-release or root/usr/lib/os-release; "Linux",
// the specification's default, without one. The log on dialog welcomes the
// user to it, as Windows' welcomed them to Windows.
QString operatingSystemName(const QString &root = QString());

} // namespace Daytona
