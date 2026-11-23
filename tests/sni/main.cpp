// SPDX-License-Identifier: GPL-3.0-or-later

// daytona-sni-test-item: shows a tray icon through StatusNotifierItem and
// prints each click it receives. For trying the notification area by hand:
//   daytona-sni-test-item [--status Active|Passive|NeedsAttention]
//                         [--icon NAME] [--tooltip TEXT] [--menu]

#include "testitem.h"
#include "testmenu.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QPainter>
#include <cstdio>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption status(QStringLiteral("status"), QStringLiteral("Item status."),
        QStringLiteral("status"), QStringLiteral("Active"));
    const QCommandLineOption icon(QStringLiteral("icon"),
        QStringLiteral("Icon name (else a drawn pixmap)."), QStringLiteral("name"));
    const QCommandLineOption tooltip(QStringLiteral("tooltip"), QStringLiteral("Tooltip title."),
        QStringLiteral("text"), QStringLiteral("Daytona test item"));
    const QCommandLineOption menu(
        QStringLiteral("menu"), QStringLiteral("Export a com.canonical.dbusmenu menu."));
    parser.addOptions({ status, icon, tooltip, menu });
    parser.process(app);

    SniTestItem::registerTypes();
    SniTestMenu::registerTypes();
    SniTestMenu menuObject;
    SniTestItem item;
    if (parser.isSet(menu)) {
        menuObject.publish(QDBusConnection::sessionBus());
        item.menu = QDBusObjectPath(QString::fromLatin1(SniTestMenu::Path));
        QObject::connect(&menuObject, &SniTestMenu::eventReceived, [](const QString &event) {
            std::printf("menu %s\n", qPrintable(event));
            std::fflush(stdout);
        });
    }
    item.status = parser.value(status);
    item.iconName = parser.value(icon);
    if (item.iconName.isEmpty()) {
        // A red square with a white centre, at 16 and 32 pixels.
        for (int size : { 16, 32 }) {
            QImage image(size, size, QImage::Format_ARGB32);
            image.fill(Qt::red);
            QPainter(&image).fillRect(size / 4, size / 4, size / 2, size / 2, Qt::white);
            item.iconPixmap << sniTestPixmap(image);
        }
    }
    item.toolTip.title = parser.value(tooltip);
    QObject::connect(&item, &SniTestItem::called, [](const QString &call) {
        std::printf("%s\n", qPrintable(call));
        std::fflush(stdout);
    });
    if (!item.publish(QDBusConnection::sessionBus())) {
        std::fprintf(stderr, "No StatusNotifierWatcher on the session bus\n");
        return 1;
    }
    std::printf("registered as %s\n", qPrintable(QDBusConnection::sessionBus().baseService()));
    std::fflush(stdout);
    return app.exec();
}
