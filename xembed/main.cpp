// SPDX-License-Identifier: GPL-3.0-or-later

// daytona-xembed-proxy: shows the icons of X11 programs that only know the
// XEmbed system tray (GtkStatusIcon, Wine, older Java and X11 utilities) in
// Daytona's notification area. Started by the session when Xwayland is
// available; ends with the X display.

#include "xembedproxy.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("daytona-xembed-proxy"));
    app.setApplicationVersion(QStringLiteral(DAYTONA_VERSION));

    Daytona::XEmbedProxy proxy;
    if (!proxy.start())
        return 1;
    return app.exec();
}
