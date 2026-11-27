// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationindex.h"
#include "balloon.h"
#include "battery.h"
#include "blankscreen.h"
#include "clock.h"
#include "controlpanel.h"
#include "datetimeproperties.h"
#include "daytonastyle.h"
#include "desktop.h"
#include "displayproperties.h"
#include "endsession.h"
#include "gtktheme.h"
#include "inputlocaleicon.h"
#include "inputlocales.h"
#include "keyboardproperties.h"
#include "mouseproperties.h"
#include "notificationarea.h"
#include "notificationqueue.h"
#include "notificationservice.h"
#include "outputmanager.h"
#include "printscreen.h"
#include "rundialog.h"
#include "screenchooser.h"
#include "screenlock.h"
#include "screensaverservice.h"
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
#include "timedate.h"
#include "toplevelmanager.h"
#include "volumecontrol.h"
#include "waylandcheck.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QScreen>
#include <QSignalBlocker>
#include <QTimer>
#include <QWindow>
#include <cstdio>
#include <cstring>

Q_LOGGING_CATEGORY(lcShell, "daytona.shell", QtInfoMsg)

namespace {

// The index of name in argv, or 0 if it is not there.
int argumentIndex(int argc, char *argv[], const char *name)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0)
            return i;
    }
    return 0;
}

bool hasArgument(int argc, char *argv[], const char *name)
{
    return argumentIndex(argc, argv, name) != 0;
}

// --print-screen [FILE] and --print-window [FILE]: Print Screen and
// Alt+Print Screen, done by the running shell. Without FILE the image goes
// to the clipboard.
int printScreen(int argc, char *argv[], int index, bool activeWindow)
{
    QCoreApplication app(argc, argv);
    QString file;
    if (index + 1 < argc && argv[index + 1][0] != '-')
        file = QFileInfo(QString::fromLocal8Bit(argv[index + 1])).absoluteFilePath();
    return Daytona::ShellService::requestPrintScreen(activeWindow, file) ? 0 : 1;
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
    // Lock Workstation (Win+L, swayidle's before-sleep and lock) and the
    // screen saver (swayidle's timeout), done by the running shell.
    if (hasArgument(argc, argv, "--lock")) {
        QCoreApplication app(argc, argv);
        return Daytona::ShellService::requestLock() ? 0 : 1;
    }
    // The keyboard layouts for labwc's environment file, which
    // daytona-session writes at log on.
    if (hasArgument(argc, argv, "--labwc-environment")) {
        QCoreApplication app(argc, argv);
        const Daytona::Settings settings(Daytona::Settings::defaultFilePath());
        const QByteArray text = Daytona::labwcEnvironment(
            Daytona::sessionKeyboard(&settings, Daytona::systemKeyboard()));
        return std::fwrite(text.constData(), 1, size_t(text.size()), stdout) == size_t(text.size())
            ? 0
            : 1;
    }
    if (hasArgument(argc, argv, "--screen-saver")) {
        QCoreApplication app(argc, argv);
        return Daytona::ShellService::requestScreenSaver() ? 0 : 1;
    }
    if (const int index = argumentIndex(argc, argv, "--print-screen"))
        return printScreen(argc, argv, index, false);
    if (const int index = argumentIndex(argc, argv, "--print-window"))
        return printScreen(argc, argv, index, true);

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

    // The screen sharing chooser, run by xdg-desktop-portal-wlr (see
    // xdg/xdg-desktop-portal-wlr): a dialog of its own, beside the shell.
    if (hasArgument(argc, argv, "--choose-screen"))
        return Daytona::runScreenChooser();

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

    // Screen modes, scales and arrangement (Display Properties > Settings),
    // restored for each screen when it appears.
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
    // The speaker icon, for the default sound output.
    Daytona::VolumeControl volume;
    if (volume.start())
        taskbar.notificationArea()->setVolumeControl(&volume);
    // The input locale indicator, while more than one keyboard layout is
    // installed.
    Daytona::InputLocales inputLocales(&settings, Daytona::labwcConfigDir());
    inputLocales.start();
    taskbar.notificationArea()->setInputLocales(&inputLocales);
    // One taskbar, on the primary monitor, as in NT 4.0: the one chosen in
    // Display Properties while it is on, else the compositor's first.
    const auto taskbarScreen = [&outputs, &settings]() -> QScreen * {
        const QString primary = settings.primaryOutput();
        if (!primary.isEmpty()) {
            for (const Daytona::OutputHead &head : outputs.heads()) {
                if (!head.enabled || Daytona::outputIdentity(head) != primary)
                    continue;
                for (QScreen *screen : QGuiApplication::screens()) {
                    if (screen->name() == head.name)
                        return screen;
                }
            }
        }
        return QGuiApplication::primaryScreen();
    };
    taskbar.winId();
    taskbar.windowHandle()->setScreen(taskbarScreen());
    taskbar.attachToLayerShell();
    taskbar.show();
    // Looked at again once screens or the choice change, after Qt and the
    // output manager have both caught up.
    QTimer placeTaskbar;
    placeTaskbar.setSingleShot(true);
    placeTaskbar.setInterval(0);
    QObject::connect(&placeTaskbar, &QTimer::timeout, &taskbar,
        [&taskbar, taskbarScreen] { taskbar.moveToScreen(taskbarScreen()); });
    const auto moveTaskbar = [&placeTaskbar] { placeTaskbar.start(); };
    QObject::connect(qApp, &QGuiApplication::primaryScreenChanged, &placeTaskbar, moveTaskbar);
    QObject::connect(qApp, &QGuiApplication::screenAdded, &placeTaskbar, moveTaskbar);
    QObject::connect(qApp, &QGuiApplication::screenRemoved, &placeTaskbar, moveTaskbar);
    QObject::connect(&outputs, &Daytona::OutputManager::changed, &placeTaskbar, moveTaskbar);
    QObject::connect(
        &settings, &Daytona::Settings::changed, &placeTaskbar, [moveTaskbar](const QString &key) {
            if (key.isEmpty() || key == QLatin1String("Display/PrimaryScreen"))
                moveTaskbar();
        });

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

    // The battery icon, on machines with a battery (UPower), and its low
    // battery balloons.
    Daytona::BatteryMonitor battery;
    QObject::connect(&battery, &Daytona::BatteryMonitor::lowBattery, &notifications,
        [&notifications](const Daytona::BatteryState &state) {
            const bool critical = state.warning >= Daytona::BatteryState::Warning::Critical;
            Daytona::Notification warning;
            warning.appName = QCoreApplication::translate("main", "Power");
            warning.appIcon = QStringLiteral("battery-caution");
            warning.summary = critical
                ? QCoreApplication::translate("main", "Battery critically low")
                : QCoreApplication::translate("main", "Battery low");
            warning.body = critical ? QCoreApplication::translate("main",
                                          "%1% remaining. Plug in the computer now, or save your "
                                          "work and shut it down.")
                                          .arg(state.percent)
                                    : QCoreApplication::translate("main",
                                          "%1% remaining. You might want to plug in the computer.")
                                          .arg(state.percent);
            // A critical warning stays as long as a balloon can.
            if (critical)
                warning.timeoutMs = Daytona::NotificationQueue::MaxTimeoutMs;
            notifications.notify(warning);
        });
    if (battery.start())
        taskbar.notificationArea()->setBatteryMonitor(&battery);

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

    // Print Screen and Alt+Print Screen (labwc key bindings).
    Daytona::PrintScreen printScreen;
    QObject::connect(&service, &Daytona::ShellService::printScreenRequested, &printScreen,
        [&printScreen, &toplevels](
            bool activeWindow, const QString &file, const std::function<void(bool)> &done) {
            // With no active window (or none on a known screen), every
            // screen.
            const QString output = activeWindow ? toplevels.list().activeOutput() : QString();
            printScreen.capture(output, file, done);
        });

    // Locking (daytona-lock) and the screen saver (swayidle's timeout).
    Daytona::ScreenLocker locker;
    if (!locker.isAvailable())
        qCWarning(lcShell) << "daytona-lock is not installed: the session cannot be locked";
    Daytona::BlankScreen blankScreen;
    QObject::connect(&service, &Daytona::ShellService::lockRequested, &locker,
        [&locker](const std::function<void()> &done) {
            locker.lock(Daytona::LockKind::Workstation, done);
        });
    QObject::connect(&service, &Daytona::ShellService::screenSaverRequested, &blankScreen,
        [&settings, &locker, &blankScreen] {
            if (settings.screenSaver() == Daytona::ScreenSaver::None || locker.isLocked())
                return;
            if (settings.screenSaverPassword())
                locker.lock(Daytona::LockKind::ScreenSaver);
            else
                blankScreen.start();
        });
    Daytona::IdleMonitor idle(&settings, QCoreApplication::applicationFilePath());
    idle.start();
    // Programs that keep the screen on (video players, presentations) through
    // org.freedesktop.ScreenSaver rather than Wayland's idle-inhibit.
    Daytona::ScreenSaverService screenSaverService(QDBusConnection::sessionBus());
    if (screenSaverService.registerOnBus()) {
        QObject::connect(&screenSaverService, &Daytona::ScreenSaverService::inhibitedChanged, &idle,
            &Daytona::IdleMonitor::setInhibited);
    } else {
        qCWarning(lcShell) << "Cannot own org.freedesktop.ScreenSaver; programs that ask"
                           << "for it will not keep the screen saver off";
    }

    // Display Properties, from Start > Settings > Display..., the desktop's
    // menu and the Control Panel.
    Daytona::DisplayPropertiesDialog displayProperties(&settings);
    displayProperties.setScreenSettings(&outputs);
    QObject::connect(&displayProperties,
        &Daytona::DisplayPropertiesDialog::screenSaverPreviewRequested, &blankScreen,
        &Daytona::BlankScreen::start);
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

    // Mouse Properties, from Start > Settings > Mouse... and the Control
    // Panel.
    Daytona::MousePropertiesDialog mouseProperties(&settings);
    const auto showMouseProperties = [&mouseProperties] {
        mouseProperties.prepare();
        mouseProperties.show();
        mouseProperties.raise();
        mouseProperties.activateWindow();
    };
    QObject::connect(&startMenu, &Daytona::StartMenu::mousePropertiesRequested, &mouseProperties,
        showMouseProperties);

    // Keyboard Properties, from Start > Settings > Keyboard..., the Control
    // Panel and the input locale indicator.
    Daytona::KeyboardPropertiesDialog keyboardProperties(&settings);
    QObject::connect(&keyboardProperties, &Daytona::KeyboardPropertiesDialog::applied,
        &inputLocales, &Daytona::InputLocales::reload);
    const auto showKeyboardProperties = [&keyboardProperties] {
        keyboardProperties.prepare();
        keyboardProperties.show();
        keyboardProperties.raise();
        keyboardProperties.activateWindow();
    };
    QObject::connect(&startMenu, &Daytona::StartMenu::keyboardPropertiesRequested,
        &keyboardProperties, showKeyboardProperties);
    QObject::connect(taskbar.notificationArea()->inputLocaleIcon(),
        &Daytona::InputLocaleIcon::propertiesRequested, &keyboardProperties,
        showKeyboardProperties);

    // Date/Time Properties, from a double-click on the clock and the Control
    // Panel, through systemd-timedated. The clock follows a new time zone
    // straight away, whoever changed it.
    Daytona::TimeDate timeDate(QDBusConnection::systemBus());
    Daytona::Clock *clock = taskbar.notificationArea()->clock();
    QObject::connect(&timeDate, &Daytona::TimeDate::timeZoneChanged, clock, [clock] {
        Daytona::TimeDate::reloadLocalTimeZone();
        clock->refresh();
    });
    timeDate.start();
    Daytona::DateTimePropertiesDialog dateTimeProperties(&timeDate);
    QObject::connect(&dateTimeProperties, &Daytona::DateTimePropertiesDialog::applied, clock,
        &Daytona::Clock::refresh);
    const auto showDateTimeProperties = [&dateTimeProperties] {
        if (!dateTimeProperties.isVisible())
            dateTimeProperties.prepare();
        dateTimeProperties.show();
        dateTimeProperties.raise();
        dateTimeProperties.activateWindow();
    };
    QObject::connect(
        clock, &Daytona::Clock::propertiesRequested, &dateTimeProperties, showDateTimeProperties);

    // Taskbar Properties, from Start > Settings > Taskbar..., the taskbar's
    // own menu and the Control Panel.
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

    // Control Panel, from Start > Settings > Control Panel: the dialogs
    // above, plus installed settings applications.
    Daytona::ControlPanel controlPanel(&applications);
    QObject::connect(
        &startMenu, &Daytona::StartMenu::controlPanelRequested, &controlPanel, [&controlPanel] {
            controlPanel.prepare();
            controlPanel.show();
            controlPanel.raise();
            controlPanel.activateWindow();
        });
    QObject::connect(&controlPanel, &Daytona::ControlPanel::dateTimePropertiesRequested,
        &dateTimeProperties, showDateTimeProperties);
    QObject::connect(&controlPanel, &Daytona::ControlPanel::displayPropertiesRequested,
        &displayProperties, showDisplayProperties);
    QObject::connect(&controlPanel, &Daytona::ControlPanel::keyboardPropertiesRequested,
        &keyboardProperties, showKeyboardProperties);
    QObject::connect(&controlPanel, &Daytona::ControlPanel::mousePropertiesRequested,
        &mouseProperties, showMouseProperties);
    QObject::connect(&controlPanel, &Daytona::ControlPanel::taskbarPropertiesRequested,
        &taskbarProperties, showTaskbarProperties);

    // Programs are asked to close before the session ends.
    Daytona::ProgramCloser programCloser(
        &toplevels.list(), [&toplevels](quint32 id) { toplevels.close(id); });
    Daytona::SessionActions sessionActions;
    sessionActions.setProgramCloser(&programCloser);
    Daytona::ShutDownDialog shutDownDialog(&sessionActions);
    shutDownDialog.setLockAvailable(locker.isAvailable());
    QObject::connect(&sessionActions, &Daytona::SessionActions::lockRequested, &locker,
        [&locker] { locker.lock(Daytona::LockKind::Workstation); });
    QObject::connect(
        &startMenu, &Daytona::StartMenu::shutDownRequested, &shutDownDialog, [&shutDownDialog] {
            shutDownDialog.prepare();
            shutDownDialog.show();
            shutDownDialog.raise();
            shutDownDialog.activateWindow();
        });

    return app.exec();
}
