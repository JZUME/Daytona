// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <functional>

class QRadioButton;

namespace Daytona {

enum class SessionAction
{
    ShutDown,
    Restart,
    LogOff
};

// The session actions behind the Shut Down dialog: shut down and restart go
// to systemd-logind over D-Bus, log off asks labwc to exit. All calls are
// asynchronous so the shell never blocks on them.
class SessionActions : public QObject
{
    Q_OBJECT

public:
    using Availability = std::function<void(bool canShutDown, bool canRestart)>;
    using Failure = std::function<void(const QString &message)>;

    explicit SessionActions(QObject *parent = nullptr);

    // Asks logind which actions are allowed ("yes" or "challenge").
    virtual void queryAvailability(Availability done);
    virtual void perform(SessionAction action, Failure failed);
};

// NT's Shut Down dialog.
class ShutDownDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShutDownDialog(SessionActions *actions, QWidget *parent = nullptr);

    // Refreshes which options are available and selects the last choice.
    void prepare();

    // Enables or disables the shut down and restart options.
    void setAvailability(bool canShutDown, bool canRestart);

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
    QRadioButton *m_logOff;
    SessionAction m_last = SessionAction::ShutDown;
};

} // namespace Daytona
