// SPDX-License-Identifier: GPL-3.0-or-later

#include "screenlock.h"

#include "settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>
#include <csignal>
#include <sys/prctl.h>

Q_LOGGING_CATEGORY(lcLock, "daytona.lock", QtInfoMsg)

namespace Daytona {

namespace {

    // A locker that keeps crashing is not restarted more than this often a
    // minute; the session stays locked by the compositor regardless.
    constexpr int MaxRestartsPerMinute = 5;

    // Single quotes for sh -c, which swayidle runs its commands with.
    QString shellQuote(const QString &text)
    {
        QString quoted = text;
        quoted.replace(QLatin1Char('\''), QLatin1String("'\\''"));
        return QLatin1Char('\'') + quoted + QLatin1Char('\'');
    }

} // namespace

QStringList lockArguments(LockKind kind)
{
    if (kind == LockKind::ScreenSaver)
        return { QStringLiteral("--screen-saver") };
    return {};
}

QString defaultLockProgram()
{
    const QDir here(QCoreApplication::applicationDirPath());
    for (const char *path : { "../libexec/daytona/daytona-lock", "../locker/daytona-lock" }) {
        const QString candidate = QDir::cleanPath(here.filePath(QString::fromLatin1(path)));
        if (QFileInfo(candidate).isExecutable())
            return candidate;
    }
    return QDir::cleanPath(here.filePath(QStringLiteral("../libexec/daytona/daytona-lock")));
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
    , m_program(defaultLockProgram())
{
}

ScreenLocker::~ScreenLocker()
{
    // Destroying the QProcess would kill the locker and leave the session
    // locked with nothing to unlock it, so let it go instead.
    if (m_process) {
        m_process->disconnect(this);
        m_process->setParent(nullptr);
        m_process = nullptr;
    }
}

bool ScreenLocker::isAvailable() const
{
    return QFileInfo(m_program).isExecutable()
        || (!m_program.contains(QLatin1Char('/'))
            && !QStandardPaths::findExecutable(m_program).isEmpty());
}

void ScreenLocker::lock(LockKind kind, std::function<void()> done)
{
    if (m_locked) {
        if (done)
            done();
        return;
    }
    if (done)
        m_waiting.append(std::move(done));
    if (m_process)
        return;
    m_kind = kind;
    m_restarts.clear();
    start();
}

void ScreenLocker::start()
{
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        if (!m_process->readAllStandardOutput().contains("locked"))
            return;
        if (!std::exchange(m_locked, true))
            Q_EMIT lockedChanged(true);
        finish();
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const bool wasLocked = std::exchange(m_locked, false);
        m_process->deleteLater();
        m_process = nullptr;
        if (status == QProcess::CrashExit && wasLocked) {
            // The compositor still has the session locked, with nothing
            // to unlock it: start the locker again to take the lock over.
            static QElapsedTimer clock;
            if (!clock.isValid())
                clock.start();
            const qint64 now = clock.elapsed();
            m_restarts.removeIf([now](qint64 at) { return now - at > 60 * 1000; });
            if (m_restarts.size() < MaxRestartsPerMinute) {
                qCWarning(lcLock) << "The screen locker crashed; starting it again";
                m_restarts.append(now);
                start(); // still locked meanwhile: no lockedChanged
                return;
            }
            // loginctl lock-session (from a text console) starts it again.
            qCCritical(lcLock) << "The screen locker keeps crashing; not starting it again";
        } else if (!wasLocked) {
            qCInfo(lcLock) << "The screen locker exited without locking, code" << code;
        }
        if (wasLocked)
            Q_EMIT lockedChanged(false);
        finish();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_process)
            return;
        qCWarning(lcLock) << "Cannot start" << m_program << "- the session cannot be locked";
        m_process->deleteLater();
        m_process = nullptr;
        finish();
    });
    m_process->start(m_program, lockArguments(m_kind));
}

void ScreenLocker::finish()
{
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
