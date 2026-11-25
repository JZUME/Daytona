// SPDX-License-Identifier: GPL-3.0-or-later

// A plain window for smoke tests: opens maximised (or, with --normal, as a
// 400x300 window) and fills its client area with a colour that does not
// occur in the Daytona theme, so a screenshot can tell whether the window is
// mapped and where.

#include <QApplication>
#include <QPalette>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QStringLiteral("Smoke"));
    QPalette palette = window.palette();
    palette.setColor(QPalette::Window, QColor(0xff, 0x00, 0xff));
    window.setPalette(palette);
    window.setAutoFillBackground(true);
    if (app.arguments().contains(QStringLiteral("--normal"))) {
        window.resize(400, 300);
        window.show();
    } else {
        window.showMaximized();
    }

    return app.exec();
}
