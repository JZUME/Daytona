// SPDX-License-Identifier: GPL-3.0-or-later
//
// Build-time tool: writes Daytona's cursor theme from Chicago95's cursors.
// Usage: daytona-cursorgen SOURCE_CURSORS_DIR TARGET_THEME_DIR

#include "xcursor.h"

#include <QCoreApplication>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 3) {
        std::fprintf(stderr, "usage: daytona-cursorgen SOURCE_CURSORS_DIR TARGET_DIR\n");
        return 2;
    }
    QHash<QString, QString> replace;
    for (const QString &name : Daytona::replacedCursorNames())
        replace.insert(name, QStringLiteral("left_ptr"));
    QString error;
    if (!Daytona::generateCursorTheme(args.at(1), args.at(2), replace, &error)) {
        std::fprintf(stderr, "daytona-cursorgen: %s\n", qPrintable(error));
        return 1;
    }
    return 0;
}
