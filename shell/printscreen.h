// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

namespace Daytona {

// Print Screen and Alt+Print Screen: captures a screen with grim and copies
// it to the clipboard with wl-copy, as NT copied it to the clipboard (no
// dialog, no file), or saves it to a PNG file for scripts.
//
// NT's Alt+Print Screen copies only the active window. labwc 0.8.3 gives
// clients no window geometry and no way to capture one window, so the
// active window's screen is captured instead.
class PrintScreen : public QObject
{
    Q_OBJECT

public:
    explicit PrintScreen(QObject *parent = nullptr);

    // grim's arguments to capture output (every screen when empty) into
    // file, or to stdout as PNG when file is empty.
    static QStringList grimArguments(const QString &output, const QString &file);

    // Captures output (every screen when empty) to file, or to the
    // clipboard when file is empty. done is called with true once the
    // image is in place.
    void capture(const QString &output, const QString &file, std::function<void(bool)> done);
};

} // namespace Daytona
