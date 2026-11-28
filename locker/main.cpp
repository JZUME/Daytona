// SPDX-License-Identifier: GPL-3.0-or-later

// daytona-lock: Daytona's screen locker, run by the shell for Lock
// Workstation and the password-protected screen saver (--screen-saver);
// --no-ctrl-alt-delete opens it on Unlock Workstation. It locks the session
// with ext-session-lock-v1, prints "locked" on standard output once every
// output is covered, and exits 0 once the user has unlocked it. It exits 1,
// without locking, if the session cannot be locked (for example because it
// already is).

#include "authenticator.h"
#include "daytonastyle.h"
#include "lockui.h"
#include "sessionlock.h"

#include <QApplication>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QScreen>
#include <QtConcurrent>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcLock, "daytona.lock", QtInfoMsg)

namespace {

bool hasArgument(int argc, char *argv[], const char *name)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0)
            return true;
    }
    return false;
}

// Tells the shell the session is locked, then lets go of its pipe.
void reportLocked()
{
    std::fputs("locked\n", stdout);
    std::fflush(stdout);
    const int null = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (null >= 0) {
        ::dup2(null, STDOUT_FILENO);
        ::close(null);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    if (hasArgument(argc, argv, "--version")) {
        std::printf("daytona-lock %s\n", DAYTONA_VERSION);
        return 0;
    }
    const bool screenSaver = hasArgument(argc, argv, "--screen-saver");
    // Lock/DisableCtrlAltDelete: open on Unlock Workstation.
    const bool noCtrlAltDelete = hasArgument(argc, argv, "--no-ctrl-alt-delete");

    // Keep passwords out of core dumps and other processes' reach.
    ::prctl(PR_SET_DUMPABLE, 0);
    // A lock only exists on Wayland; never fall back to another platform.
    qputenv("QT_QPA_PLATFORM", "wayland");
    qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("daytona-lock"));
    QApplication::setApplicationVersion(QStringLiteral(DAYTONA_VERSION));
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setStyle(new Daytona::Style);
    QApplication::setFont(QFont(QStringLiteral("sans"), 8));
    QIcon::setThemeName(QStringLiteral("Chicago95"));

    Daytona::SessionLockManager manager;
    if (!manager.isActive()) {
        qCCritical(lcLock) << "The compositor does not support ext-session-lock-v1;"
                           << "the session cannot be locked";
        return 1;
    }
    Daytona::SessionLock lock(&manager);

    const Daytona::LockUser user = Daytona::currentLockUser();
    const QByteArray service = Daytona::pamService();
    Daytona::LockController controller(user, screenSaver,
        [service, name = user.name.toLocal8Bit()](
            const QString &password, std::function<void(bool)> done) {
            auto *watcher = new QFutureWatcher<bool>;
            QObject::connect(watcher, &QFutureWatcher<bool>::finished, watcher, [watcher, done] {
                done(watcher->result());
                watcher->deleteLater();
            });
            watcher->setFuture(
                QtConcurrent::run(Daytona::authenticate, service, name, password.toLocal8Bit()));
        });
    controller.setRequiresCtrlAltDelete(!noCtrlAltDelete);

    const auto addScreen = [&controller, &lock](QScreen *screen) {
        QWidget *widget = controller.addScreen(screen);
        if (lock.attach(widget))
            widget->show();
        else
            controller.removeScreen(screen);
    };
    for (QScreen *screen : QGuiApplication::screens())
        addScreen(screen);
    QObject::connect(&app, &QGuiApplication::screenAdded, &controller, addScreen);
    QObject::connect(&app, &QGuiApplication::screenRemoved, &controller,
        [&controller](QScreen *screen) { controller.removeScreen(screen); });

    QObject::connect(&lock, &Daytona::SessionLock::locked, &app, [] {
        qCInfo(lcLock) << "Session locked";
        reportLocked();
    });
    QObject::connect(&lock, &Daytona::SessionLock::finished, &app, [] {
        qCWarning(lcLock) << "The session could not be locked (is it locked already?)";
        QCoreApplication::exit(1);
    });
    QObject::connect(&controller, &Daytona::LockController::unlocked, &app, [&lock] {
        lock.unlock();
        qCInfo(lcLock) << "Session unlocked";
        QCoreApplication::exit(0);
    });

    return app.exec();
}
