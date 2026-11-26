// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationindex.h"
#include "balloon.h"
#include "daytonastyle.h"
#include "desktop.h"
#include "displayproperties.h"
#include "gtktheme.h"
#include "mouseproperties.h"
#include "notificationarea.h"
#include "notificationqueue.h"
#include "notificationservice.h"
#include "outputmanager.h"
#include "rundialog.h"
#include "settings.h"
#include "shellservice.h"
#include "shutdowndialog.h"
#include "startbutton.h"
#include "startmenu.h"
#include "statusnotifierhost.h"
#include "taskbar.h"
#include "taskbarplacement.h"
#include "taskbarproperties.h"
#include "tasklist.h"
#include "toplevelmanager.h"
#include "waylandcheck.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QFile>
#include <QLoggingCategory>
#include <QScreen>
#include <QSignalBlocker>
#include <QWindow>
#include <cstdio>
#include <cstring>

Q_LOGGING_CATEGORY(lcShell, "daytona.shell", QtInfoMsg)

namespace {

bool hasArgument(int argc, char *argv[], const char *name)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0)
            return true;
    }
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    // These need no display connection, so handle them before QApplication.
    if (hasArgument(argc, argv, "--version")) {
        std::printf("daytona-shell %s\n", DAYTONA_VERSION);
        return 0;
    }
    if (hasArgument(argc, argv, "--start-menu")) {
        QCoreApplication app(argc, argv);
        return Daytona::ShellService::requestToggleStartMenu() ? 0 : 1;
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("daytona-shell"));
    QApplication::setApplicationVersion(QStringLiteral(DAYTONA_VERSION));
    QApplication::setQuitOnLastWindowClosed(false);

    // The shell always uses the Daytona style, whatever QT_STYLE_OVERRIDE
    // says, and NT's 8 point UI font.
    // The style also applies the colour scheme from colors.ini and follows
    // changes to it.
    QApplication::setStyle(new Daytona::Style);
    QApplication::setFont(QFont(QStringLiteral("sans"), 8));
    // Window and menu icons come from Chicago95, installed with Daytona.
    QIcon::setThemeName(QStringLiteral("Chicago95"));

    Daytona::ShellService service;
    switch (service.registerOnSessionBus()) {
    case Daytona::ShellService::Registration::Registered:
        break;
    case Daytona::ShellService::Registration::AlreadyRunning:
        qCInfo(lcShell) << "Another daytona-shell is already running; exiting";
        return 0;
    case Daytona::ShellService::Registration::NoSessionBus:
        qCWarning(lcShell) << "No D-Bus session bus: running without single-instance checks"
                           << "or the Start menu key";
        break;
    }

    if (!Daytona::compositorHasGlobal("zwlr_layer_shell_v1")) {
        qCCritical(lcShell) << "The compositor does not support wlr-layer-shell"
                            << "(or this is not a Wayland session); the taskbar cannot be shown";
        return 1;
    }

    // Open windows, for the taskbar's window buttons. Without the protocol
    // the shell still runs, just without window buttons.
    Daytona::ToplevelManager toplevels;
    if (!Daytona::compositorHasGlobal("zwlr_foreign_toplevel_manager_v1")) {
        qCWarning(lcShell) << "The compositor does not support wlr-foreign-toplevel-management;"
                           << "the taskbar will not show window buttons";
    }

    Daytona::Settings settings(Daytona::Settings::defaultFilePath());

    // GTK 3 applications use the cursor theme and size in their settings.ini
    // once a theme is set there (the theme tool sets it with a colour
    // scheme); otherwise they follow XCURSOR_THEME and XCURSOR_SIZE. Keep
    // that file in step with the pointer scheme.
    {
        QFile gtk(Daytona::gtkSettingsPath());
        if (gtk.open(QIODevice::ReadOnly) && gtk.size() < 1024 * 1024
            && gtk.readAll().contains("gtk-cursor-theme-name")) {
            gtk.close();
            QString error;
            if (!Daytona::setGtkCursorSettings(gtk.fileName(),
                    QString::fromLatin1(Daytona::CursorThemeName),
                    Daytona::pointerSchemeSize(settings.pointerScheme()), &error))
                qCWarning(lcShell) << "Cannot update the GTK cursor settings:" << error;
        }
    }

    // Screen scales (Display Properties > Settings > Font Size), restored
    // for each screen when it appears.
    Daytona::OutputManager outputs;
    outputs.setSettings(&settings);
    if (!Daytona::compositorHasGlobal("zwlr_output_manager_v1")) {
        qCWarning(lcShell) << "The compositor does not support wlr-output-management;"
                           << "the font size cannot be changed";
    }
    Daytona::Taskbar taskbar;
    taskbar.setSettings(&settings); // edge and lock state
    taskbar.taskList()->setManager(&toplevels);
    // Tray icons from StatusNotifierItem applications.
    Daytona::StatusNotifierHost trayHost(QDBusConnection::sessionBus());
    if (trayHost.start())
        taskbar.notificationArea()->setHost(&trayHost);
    else
        qCWarning(lcShell) << "No D-Bus session bus: the notification area shows no tray icons";
    // One taskbar, on the primary output, as in NT 4.0.
    taskbar.winId();
    taskbar.windowHandle()->setScreen(QGuiApplication::primaryScreen());
    taskbar.attachToLayerShell();
    taskbar.show();
    QObject::connect(qApp, &QGuiApplication::primaryScreenChanged, &taskbar,
        [&taskbar](QScreen *screen) { taskbar.moveToScreen(screen); });

    // The Start menu: its own layer surface, so it has no parent window.
    settings.watchFile();

    // Notification balloons (org.freedesktop.Notifications).
    Daytona::NotificationQueue notifications;
    Daytona::NotificationService notificationService(&notifications, QDBusConnection::sessionBus());
    if (!notificationService.registerOnBus()) {
        qCWarning(lcShell) << "Another notification server owns org.freedesktop.Notifications;"
                           << "notification balloons will not be shown";
    }
    Daytona::Balloon balloon;
    QObject::connect(&notifications, &Daytona::NotificationQueue::currentChanged, &balloon,
        [&notifications, &balloon, &taskbar] {
            const std::optional<Daytona::Notification> current = notifications.current();
            if (!current) {
                balloon.hide();
                return;
            }
            // Point at the middle of the notification area, measured from the
            // right end of a horizontal taskbar or the bottom of a vertical one.
            Daytona::NotificationArea *area = taskbar.notificationArea();
            const QPoint centre = area->mapTo(&taskbar, area->rect().center());
            const Daytona::TaskbarEdge edge = taskbar.edge();
            const int target = Daytona::isVertical(edge) ? taskbar.height() - centre.y()
                                                         : taskbar.width() - centre.x();
            balloon.showNotification(*current, edge, target, taskbar.screen());
        });
    QObject::connect(
        &balloon, &Daytona::Balloon::closeClicked, &notifications, [&notifications](uint id) {
            notifications.close(id, Daytona::NotificationQueue::CloseReason::Dismissed);
        });
    QObject::connect(&balloon, &Daytona::Balloon::bodyClicked, &notifications,
        &Daytona::NotificationQueue::activateCurrent);

    // The desktop: colour and wallpaper behind every window.
    Daytona::DesktopManager desktops(&settings);
    desktops.start();
    Daytona::ApplicationIndex applications;
    applications.start();
    Daytona::StartMenu startMenu(&applications, &settings);
    startMenu.setEdge(taskbar.edge());
    // An auto-hidden taskbar stays shown while the Start menu is open.
    taskbar.setKeepShown([&startMenu] { return startMenu.isVisible(); });
    QObject::connect(
        &startMenu, &Daytona::StartMenu::closed, &taskbar, &Daytona::Taskbar::collapseLater);

    // Display Properties, from Start > Settings > Display... and the
    // desktop's menu.
    Daytona::DisplayPropertiesDialog displayProperties(&settings);
    displayProperties.setScreenSettings(&outputs);
    const auto showDisplayProperties = [&displayProperties] {
        displayProperties.prepare();
        displayProperties.show();
        displayProperties.raise();
        displayProperties.activateWindow();
    };
    QObject::connect(&startMenu, &Daytona::StartMenu::displayPropertiesRequested,
        &displayProperties, showDisplayProperties);
    QObject::connect(&desktops, &Daytona::DesktopManager::propertiesRequested, &displayProperties,
        showDisplayProperties);

    // Mouse Properties, from Start > Settings > Mouse....
    Daytona::MousePropertiesDialog mouseProperties(&settings);
    QObject::connect(&startMenu, &Daytona::StartMenu::mousePropertiesRequested, &mouseProperties,
        [&mouseProperties] {
            mouseProperties.prepare();
            mouseProperties.show();
            mouseProperties.raise();
            mouseProperties.activateWindow();
        });

    // Taskbar Properties, from Start > Settings > Taskbar... and the
    // taskbar's own menu.
    Daytona::TaskbarPropertiesDialog taskbarProperties(&settings);
    const auto showTaskbarProperties = [&taskbarProperties] {
        taskbarProperties.prepare();
        taskbarProperties.show();
        taskbarProperties.raise();
        taskbarProperties.activateWindow();
    };
    QObject::connect(&startMenu, &Daytona::StartMenu::taskbarPropertiesRequested,
        &taskbarProperties, showTaskbarProperties);
    QObject::connect(&taskbar, &Daytona::Taskbar::propertiesRequested, &taskbarProperties,
        showTaskbarProperties);
    QObject::connect(&taskbar, &Daytona::Taskbar::edgeChanged, &startMenu,
        [&startMenu](Daytona::TaskbarEdge edge) { startMenu.setEdge(edge); });
    Daytona::StartButton *startButton = taskbar.startButton();
    // The Start menu key (via D-Bus) presses or releases the Start button,
    // exactly as a click does, but opens the menu ready for the keyboard.
    bool openedByKeyboard = false;
    QObject::connect(&service, &Daytona::ShellService::toggleStartMenuRequested, startButton,
        [startButton, &openedByKeyboard] {
            openedByKeyboard = true;
            startButton->toggle();
            openedByKeyboard = false;
        });
    QObject::connect(startButton, &Daytona::StartButton::startMenuRequested, &startMenu,
        [&startMenu, startButton, &openedByKeyboard](bool open) {
            if (!open) {
                startMenu.hide();
                return;
            }
            // A click on the Start button while the menu is open closes the
            // menu first; that same click must not open it again.
            if (startMenu.justClosed()) {
                const QSignalBlocker blocker(startButton);
                startButton->setChecked(false);
                return;
            }
            startMenu.popUpFrom(startButton, openedByKeyboard);
        });
    // The Start button stays pressed exactly while the menu is open.
    QObject::connect(&startMenu, &Daytona::StartMenu::closed, startButton, [startButton] {
        const QSignalBlocker blocker(startButton);
        startButton->setChecked(false);
    });

    Daytona::RunDialog runDialog(&settings);
    QObject::connect(&startMenu, &Daytona::StartMenu::runRequested, &runDialog, [&runDialog] {
        runDialog.prepare();
        runDialog.show();
        runDialog.raise();
        runDialog.activateWindow();
    });

    Daytona::SessionActions sessionActions;
    Daytona::ShutDownDialog shutDownDialog(&sessionActions);
    QObject::connect(
        &startMenu, &Daytona::StartMenu::shutDownRequested, &shutDownDialog, [&shutDownDialog] {
            shutDownDialog.prepare();
            shutDownDialog.show();
            shutDownDialog.raise();
            shutDownDialog.activateWindow();
        });

    return app.exec();
}
