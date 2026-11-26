// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

namespace Daytona {

// The PAM service the locker checks passwords with: "daytona"
// (/etc/pam.d/daytona, installed by the package), or "login" if Daytona's is
// not installed, as in a build run from its own prefix.
QByteArray pamService();

// Checks user's password with PAM. Blocks, for as long as PAM takes (a
// failure is deliberately slow), so call it off the GUI thread. The
// password is overwritten once used.
bool authenticate(const QByteArray &service, const QByteArray &user, QByteArray password);

} // namespace Daytona
