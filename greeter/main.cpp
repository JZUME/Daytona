// SPDX-License-Identifier: GPL-3.0-or-later

// daytona-logon: the Daytona greeter's log on dialog, a greetd greeter.
// greetd runs it as its greeter user, inside a labwc of its own started by
// daytona-greeter, with the socket to talk to in $GREETD_SOCK. It exits 0
// once greetd has accepted the session to start.
//
// DAYTONA_GREETER_SCALE (daytona-greeter --scale) sets every screen's scale.
//
// --labwc-environment prints labwc's environment file for the system
// keyboard layout, for daytona-greeter to give the compositor.

#include "daytonastyle.h"
#include "greetd.h"
#include "greeter.h"
#include "keyboard.h"
#include "outputscale.h"
#include "scale.h"
#include "sessions.h"

#include <LayerShellQt/Window>
#include <QApplication>
#include <QLocale>
#include <QLoggingCategory>
#include <QPointer>
#include <QScreen>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sys/prctl.h>

Q_LOGGING_CATEGORY(lcLogon, "daytona.greeter.main", QtInfoMsg)

namespace {

bool hasArgument(int argc, char *argv[], const char *name)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0)
            return true;
    }
    return false;
}

// The desktop colour behind the dialog, on every output.
QWidget *addBackground(QScreen *screen)
{
    auto *window = new QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint);
    window->setObjectName(QStringLiteral("greeter-background-") + screen->name());
    QPalette palette = window->palette();
    palette.setColor(QPalette::Window, Daytona::Style::schemeColour(QStringLiteral("Background")));
    window->setPalette(palette);
    window->setAutoFillBackground(true);
    window->winId();
    window->windowHandle()->setScreen(screen);
    auto *layer = LayerShellQt::Window::get(window->windowHandle());
    layer->setScope(QStringLiteral("daytona-greeter-background"));
    layer->setLayer(LayerShellQt::Window::LayerBackground);
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop
        | LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft
        | LayerShellQt::Window::AnchorRight));
    layer->setExclusiveZone(-1);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    window->show();
    return window;
}

} // namespace

int main(int argc, char *argv[])
{
    if (hasArgument(argc, argv, "--version")) {
        std::printf("daytona-logon %s\n", DAYTONA_VERSION);
        return 0;
    }
    if (hasArgument(argc, argv, "--labwc-environment")) {
        QCoreApplication app(argc, argv);
        std::fputs(
            Daytona::labwcEnvironment(Daytona::systemKeyboard()).toUtf8().constData(), stdout);
        return 0;
    }

    // Keep passwords out of core dumps and other processes' reach.
    ::prctl(PR_SET_DUMPABLE, 0);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("daytona-logon"));
    QApplication::setApplicationVersion(QStringLiteral(DAYTONA_VERSION));
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setStyle(new Daytona::Style);
    QApplication::setFont(QFont(QStringLiteral("sans"), 8));
    QIcon::setThemeName(QStringLiteral("Chicago95"));

    const QString socket = qEnvironmentVariable("GREETD_SOCK");
    if (socket.isEmpty()) {
        qCCritical(lcLogon) << "GREETD_SOCK is not set: daytona-logon is run by greetd";
        return 1;
    }
    Daytona::GreetdClient greetd;
    if (!greetd.connectTo(socket))
        return 1;
    QObject::connect(&greetd, &Daytona::GreetdClient::disconnected, &app, [] {
        qCWarning(lcLogon) << "greetd closed the connection";
        QCoreApplication::exit(1);
    });

    const QList<Daytona::DesktopSession> sessions
        = Daytona::findSessions(Daytona::systemDataDirs(), QLocale::system().name());
    if (sessions.isEmpty())
        qCWarning(lcLogon) << "No sessions in the wayland-sessions directories";

    Daytona::PowerActions power;
    Daytona::Greeter greeter(&greetd, sessions, Daytona::Greeter::defaultStatePath(), &power);
    QObject::connect(&greeter, &Daytona::Greeter::finished, &app, &QCoreApplication::quit);

    // daytona-greeter --scale: for high-density screens.
    std::unique_ptr<Daytona::OutputScaler> scaler;
    const QString scaleSetting = qEnvironmentVariable("DAYTONA_GREETER_SCALE");
    if (!scaleSetting.isEmpty() && QGuiApplication::platformName() == QLatin1String("wayland")) {
        const double scale = Daytona::parseScale(scaleSetting);
        if (scale > 0)
            scaler = std::make_unique<Daytona::OutputScaler>(scale);
        else
            qCWarning(lcLogon) << "Ignoring scale" << scaleSetting << "(1 to 3, such as 1.5 or 2)";
    }

    // The background needs layer-shell; elsewhere (a test, another
    // compositor) the dialog is shown alone.
    QList<QPointer<QWidget>> backgrounds;
    if (QGuiApplication::platformName() == QLatin1String("wayland")) {
        for (QScreen *screen : QGuiApplication::screens())
            backgrounds.append(addBackground(screen));
        QObject::connect(&app, &QGuiApplication::screenAdded, &app,
            [&backgrounds](QScreen *screen) { backgrounds.append(addBackground(screen)); });
        QObject::connect(
            &app, &QGuiApplication::screenRemoved, &app, [&backgrounds](QScreen *screen) {
                for (const QPointer<QWidget> &window : std::as_const(backgrounds)) {
                    if (window && window->screen() == screen)
                        window->deleteLater();
                }
            });
    }

    greeter.dialog()->show();
    greeter.dialog()->focusFirstEmpty();
    const int result = app.exec();
    for (const QPointer<QWidget> &window : std::as_const(backgrounds))
        delete window;
    return result;
}
