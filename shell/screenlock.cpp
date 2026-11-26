// SPDX-License-Identifier: GPL-3.0-or-later

#include "screenlock.h"

#include "daytonastyle.h"
#include "settings.h"

#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>
#include <csignal>
#include <sys/prctl.h>

Q_LOGGING_CATEGORY(lcLock, "daytona.lock", QtInfoMsg)

namespace Daytona {

namespace {

    QString rgb(const SchemeColours &colours, const QString &key)
    {
        const QColor colour = colours.value(key, standardColours().value(key));
        return colour.name(QColor::HexRgb).mid(1);
    }

    // Single quotes for sh -c, which swayidle runs its commands with.
    QString shellQuote(const QString &text)
    {
        QString quoted = text;
        quoted.replace(QLatin1Char('\''), QLatin1String("'\\''"));
        return QLatin1Char('\'') + quoted + QLatin1Char('\'');
    }

} // namespace

QStringList swaylockArguments(LockKind kind, const SchemeColours &colours)
{
    const auto c = [&colours](const char *key) { return rgb(colours, QLatin1String(key)); };
    QStringList arguments {
        // No user configuration: the look is Daytona's, and -f must hold.
        QStringLiteral("--config"),
        QStringLiteral("/dev/null"),
        QStringLiteral("--daemonize"),
        // A key pressed only to wake the screen is not a failed attempt.
        QStringLiteral("--ignore-empty-password"),
        QStringLiteral("--show-failed-attempts"),
        // The password ring in the colour scheme's dialog colours.
        QStringLiteral("--inside-color"),
        c("ButtonFace"),
        QStringLiteral("--ring-color"),
        c("ButtonShadow"),
        QStringLiteral("--key-hl-color"),
        c("Hilight"),
        QStringLiteral("--line-color"),
        c("WindowFrame"),
        QStringLiteral("--separator-color"),
        c("WindowFrame"),
        QStringLiteral("--text-color"),
        c("ButtonText"),
        QStringLiteral("--inside-clear-color"),
        c("ButtonFace"),
        QStringLiteral("--text-clear-color"),
        c("ButtonText"),
        QStringLiteral("--inside-ver-color"),
        c("ButtonFace"),
        QStringLiteral("--text-ver-color"),
        c("ButtonText"),
        QStringLiteral("--inside-wrong-color"),
        c("ButtonFace"),
        QStringLiteral("--text-wrong-color"),
        c("ButtonText"),
    };
    if (kind == LockKind::Workstation) {
        // Lock Workstation shows that the computer is locked and waits for
        // the password, on the desktop colour.
        arguments << QStringLiteral("--color") << c("Background")
                  << QStringLiteral("--indicator-idle-visible");
    } else {
        // A password-protected Blank Screen stays black until a key is
        // pressed.
        arguments << QStringLiteral("--color") << QStringLiteral("000000");
    }
    return arguments;
}

QStringList swayidleArguments(int timeoutSeconds, const QString &shell)
{
    const QString lock = shellQuote(shell) + QStringLiteral(" --lock");
    // -w: wait for each command, so the screen is locked before the
    // computer sleeps.
    QStringList arguments { QStringLiteral("-w") };
    if (timeoutSeconds > 0) {
        arguments << QStringLiteral("timeout") << QString::number(timeoutSeconds)
                  << shellQuote(shell) + QStringLiteral(" --screen-saver");
    }
    arguments << QStringLiteral("before-sleep") << lock << QStringLiteral("lock") << lock;
    return arguments;
}

ScreenLocker::ScreenLocker(QObject *parent)
    : QObject(parent)
    , m_program(QStringLiteral("swaylock"))
{
}

bool ScreenLocker::isAvailable() const
{
    return !QStandardPaths::findExecutable(m_program).isEmpty();
}

void ScreenLocker::lock(LockKind kind, std::function<void()> done)
{
    if (done)
        m_waiting.append(std::move(done));
    if (m_process)
        return;

    SchemeColours colours;
    for (const QString &key : colourKeys())
        colours.insert(key, Style::schemeColour(key));

    m_process = new QProcess(this);
    // The daemonised locker keeps no pipe to the shell open.
    m_process->setStandardOutputFile(QProcess::nullDevice());
    m_process->setStandardErrorFile(QProcess::nullDevice());
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        // swaylock -f exits once the session is locked. It fails if the
        // session is already locked, which is just as good.
        if (status != QProcess::NormalExit || code != 0)
            qCInfo(lcLock) << "swaylock exited with code" << code << "(already locked?)";
        finish();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        qCWarning(lcLock) << "Cannot start" << m_program << "- the session cannot be locked";
        finish();
    });
    m_process->start(m_program, swaylockArguments(kind, colours));
}

void ScreenLocker::finish()
{
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    const QList<std::function<void()>> waiting = std::exchange(m_waiting, {});
    for (const auto &done : waiting)
        done();
}

IdleMonitor::IdleMonitor(Settings *settings, const QString &shell, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_shell(shell)
    , m_program(QStringLiteral("swayidle"))
{
    if (m_settings) {
        connect(m_settings, &Settings::changed, this, [this](const QString &key) {
            if (key.isEmpty() || key.startsWith(QLatin1String("ScreenSaver/")))
                update();
        });
    }
}

IdleMonitor::~IdleMonitor()
{
    stop();
    // Ends swayidle with the shell, before its QProcess is destroyed.
    for (QProcess *process : findChildren<QProcess *>(Qt::FindDirectChildrenOnly)) {
        process->terminate();
        process->waitForFinished(1000);
    }
}

bool IdleMonitor::start()
{
    if (QStandardPaths::findExecutable(m_program).isEmpty()) {
        qCWarning(lcLock) << "swayidle is not installed: the screen saver will not start and"
                          << "the session will not lock before sleep";
        return false;
    }
    m_started = true;
    update();
    return true;
}

void IdleMonitor::setInhibited(bool inhibited)
{
    if (inhibited == m_inhibited)
        return;
    m_inhibited = inhibited;
    qCDebug(lcLock) << "Screen saver" << (inhibited ? "inhibited" : "allowed");
    update();
}

QStringList IdleMonitor::arguments() const
{
    int timeout = 0;
    if (m_settings && !m_inhibited && m_settings->screenSaver() != ScreenSaver::None)
        timeout = m_settings->screenSaverWait() * 60;
    return swayidleArguments(timeout, m_shell);
}

void IdleMonitor::update()
{
    if (!m_started)
        return;
    const QStringList wanted = arguments();
    if (m_process && wanted == m_running)
        return;
    stop();
    m_running = wanted;
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::ForwardedChannels);
    // swayidle goes when the shell goes, even if the shell is killed, so a
    // restarted shell does not leave two running.
    m_process->setChildProcessModifier([] { ::prctl(PR_SET_PDEATHSIG, SIGTERM); });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        qCWarning(lcLock) << "swayidle exited with code" << code;
        m_process->deleteLater();
        m_process = nullptr;
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_process)
            return;
        qCWarning(lcLock) << "Cannot start" << m_program;
        m_process->deleteLater();
        m_process = nullptr;
    });
    m_process->start(m_program, wanted);
}

void IdleMonitor::stop()
{
    if (!m_process)
        return;
    QProcess *process = std::exchange(m_process, nullptr);
    process->disconnect(this);
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    // Still this object's child, so it is killed at the latest when this
    // monitor is destroyed.
    process->terminate();
}

} // namespace Daytona
