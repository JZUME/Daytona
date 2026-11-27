// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <functional>

class QRadioButton;

namespace Daytona {

class ProgramCloser;

enum class SessionAction
{
    ShutDown,
    Restart,
    StandBy,
    Hibernate,
    LogOff,
    Lock
};

// Which of logind's actions the user may take ("yes" or "challenge").
struct PowerAvailability
{
    bool shutDown = false;
    bool restart = false;
    bool standBy = false;
    bool hibernate = false;
};

// The session actions behind the Shut Down dialog: shut down, restart, stand
// by and hibernate go to systemd-logind over D-Bus, log off asks labwc to exit, and lock emits
// lockRequested for the shell's screen locker. Before shutting down,
// restarting or logging off, programs are asked to close (see
// setProgramCloser). All calls are asynchronous so the shell never blocks on
// them.
class SessionActions : public QObject
{
    Q_OBJECT

public:
    using Availability = std::function<void(const PowerAvailability &available)>;
    using Failure = std::function<void(const QString &message)>;

    explicit SessionActions(QObject *parent = nullptr);

    // Asks logind which actions are allowed ("yes" or "challenge").
    virtual void queryAvailability(Availability done);
    virtual void perform(SessionAction action, Failure failed);

    // Closes programs before the session ends; without one the session
    // ends straight away.
    void setProgramCloser(ProgramCloser *closer) { m_closer = closer; }
    // Runs once programs have closed, just before the session ends (the log
    // off sound), and calls its argument when done.
    using BeforeEnd = std::function<void(std::function<void()> then)>;
    void setBeforeEnd(BeforeEnd beforeEnd) { m_beforeEnd = std::move(beforeEnd); }

Q_SIGNALS:
    void lockRequested();

private:
    void performNow(SessionAction action, const Failure &failed);

    ProgramCloser *m_closer = nullptr;
    BeforeEnd m_beforeEnd;
};

// NT's Shut Down dialog.
class ShutDownDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShutDownDialog(SessionActions *actions, QWidget *parent = nullptr);

    // Refreshes which options are available and selects the last choice.
    void prepare();

    // Enables or disables the shut down and restart options, as NT did, and
    // shows stand by and hibernate only where the machine can do them.
    void setAvailability(const PowerAvailability &available);
    // Whether the lock option is offered (daytona-lock is installed).
    void setLockAvailable(bool available);

    SessionAction chosen() const;
    QRadioButton *option(SessionAction action) const;

    std::function<void(const QString &title, const QString &message)> showError;

public Q_SLOTS:
    void accept() override;

private:
    void select(SessionAction action);

    SessionActions *m_actions;
    QRadioButton *m_shutDown;
    QRadioButton *m_restart;
    QRadioButton *m_standBy;
    QRadioButton *m_hibernate;
    QRadioButton *m_logOff;
    QRadioButton *m_lock;
    SessionAction m_last = SessionAction::ShutDown;
};

} // namespace Daytona
