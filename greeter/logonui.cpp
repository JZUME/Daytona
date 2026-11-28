// SPDX-License-Identifier: GPL-3.0-or-later

#include "logonui.h"

#include "osrelease.h"

#include <QCloseEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace Daytona {

namespace {

    QPushButton *button(const QString &text, QWidget *parent)
    {
        auto *b = new QPushButton(text, parent);
        b->setMinimumSize(75, 23);
        return b;
    }

    QLabel *iconLabel(const QIcon &icon, QWidget *parent)
    {
        auto *label = new QLabel(parent);
        label->setPixmap(icon.pixmap(32));
        label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        label->setFixedWidth(32 + 11);
        return label;
    }

    QIcon keyIcon()
    {
        // Chicago95's keys, close to Windows 98's log on icon.
        return QIcon::fromTheme(
            QStringLiteral("password"), QIcon::fromTheme(QStringLiteral("dialog-password")));
    }

} // namespace

LogonDialog::LogonDialog(QWidget *parent)
    : QDialog(parent)
{
    // The operating system's name, as Windows' "Welcome to Windows".
    setWindowTitle(tr("Welcome to %1").arg(operatingSystemName()));

    auto *intro = new QLabel(tr("Type a user name and password to log on to this computer."), this);
    intro->setWordWrap(true);

    m_user = new QLineEdit(this);
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    for (QLineEdit *field : { m_user, m_password })
        field->setMinimumWidth(200);
    auto *userLabel = new QLabel(tr("&User name:"), this);
    userLabel->setBuddy(m_user);
    auto *passwordLabel = new QLabel(tr("&Password:"), this);
    passwordLabel->setBuddy(m_password);
    auto *sessionLabel = new QLabel(tr("Session:"), this);
    m_sessionName = new QLabel(this);
    m_sessionButton = button(tr("&Session..."), this);
    auto *sessionRow = new QHBoxLayout;
    sessionRow->setContentsMargins(0, 0, 0, 0);
    sessionRow->addWidget(m_sessionName, 1);
    sessionRow->addWidget(m_sessionButton);

    auto *fields = new QGridLayout;
    fields->setHorizontalSpacing(11);
    fields->setVerticalSpacing(7);
    fields->addWidget(userLabel, 0, 0);
    fields->addWidget(m_user, 0, 1);
    fields->addWidget(passwordLabel, 1, 0);
    fields->addWidget(m_password, 1, 1);
    fields->addWidget(sessionLabel, 2, 0);
    fields->addLayout(sessionRow, 2, 1);

    auto *text = new QVBoxLayout;
    text->setSpacing(11);
    text->addWidget(intro);
    text->addLayout(fields);
    text->addStretch();

    m_ok = button(tr("OK"), this);
    m_ok->setDefault(true);
    m_cancel = button(tr("Cancel"), this);
    m_shutDown = button(tr("Shut &Down..."), this);
    auto *buttons = new QVBoxLayout;
    buttons->setSpacing(6);
    buttons->addWidget(m_ok);
    buttons->addWidget(m_cancel);
    buttons->addStretch();
    buttons->addWidget(m_shutDown);

    auto *row = new QHBoxLayout(this);
    row->setSpacing(11);
    row->addWidget(iconLabel(keyIcon(), this));
    row->addLayout(text, 1);
    row->addLayout(buttons);
    row->setSizeConstraint(QLayout::SetFixedSize);

    // Tab order: User name, Password, OK, as Windows'.
    setTabOrder(m_user, m_password);
    setTabOrder(m_password, m_ok);
    setTabOrder(m_ok, m_cancel);
    setTabOrder(m_cancel, m_sessionButton);
    setTabOrder(m_sessionButton, m_shutDown);

    connect(m_ok, &QPushButton::clicked, this,
        [this] { Q_EMIT logOnRequested(m_user->text().trimmed(), m_password->text()); });
    connect(m_cancel, &QPushButton::clicked, this, &LogonDialog::reject);
    connect(m_sessionButton, &QPushButton::clicked, this, &LogonDialog::sessionRequested);
    connect(m_shutDown, &QPushButton::clicked, this, &LogonDialog::shutDownRequested);
}

void LogonDialog::setSessionName(const QString &name)
{
    m_sessionName->setText(name);
}

void LogonDialog::focusFirstEmpty()
{
    if (m_user->text().isEmpty())
        m_user->setFocus();
    else
        m_password->setFocus();
}

void LogonDialog::reject()
{
    m_user->clear();
    m_password->clear();
    m_user->setFocus();
}

void LogonDialog::closeEvent(QCloseEvent *event)
{
    // The title bar's Close button: as Cancel.
    event->ignore();
    reject();
}

SessionDialog::SessionDialog(const QList<DesktopSession> &sessions, int current, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Session"));
    auto *intro = new QLabel(tr("Choose the desktop to start when you log on:"), this);
    auto *label = intro;
    m_list = new QListWidget(this);
    for (const DesktopSession &session : sessions)
        m_list->addItem(session.name);
    m_list->setCurrentRow(current >= 0 ? current : 0);
    label->setBuddy(m_list);
    connect(m_list, &QListWidget::itemActivated, this, &QDialog::accept);

    auto *ok = button(tr("OK"), this);
    ok->setDefault(true);
    auto *cancel = button(tr("Cancel"), this);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);

    auto *column = new QVBoxLayout(this);
    column->addWidget(intro);
    column->addWidget(m_list);
    column->addLayout(buttons);
    resize(300, 220);
}

int SessionDialog::chosen() const
{
    return m_list->currentRow();
}

PromptDialog::PromptDialog(const QString &prompt, bool secret, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Log On"));
    m_label = new QLabel(prompt.trimmed(), this);
    m_label->setWordWrap(true);
    m_field = new QLineEdit(this);
    m_field->setMinimumWidth(200);
    if (secret)
        m_field->setEchoMode(QLineEdit::Password);
    m_label->setBuddy(m_field);

    auto *ok = button(tr("OK"), this);
    ok->setDefault(true);
    auto *cancel = button(tr("Cancel"), this);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QVBoxLayout;
    buttons->setSpacing(6);
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    buttons->addStretch();

    auto *fields = new QVBoxLayout;
    fields->setSpacing(7);
    fields->addWidget(m_label);
    fields->addWidget(m_field);
    fields->addStretch();

    auto *row = new QHBoxLayout(this);
    row->setSpacing(11);
    row->addWidget(iconLabel(keyIcon(), this));
    row->addLayout(fields, 1);
    row->addLayout(buttons);
    row->setSizeConstraint(QLayout::SetFixedSize);
    m_field->setFocus();
}

GreeterShutDownDialog::GreeterShutDownDialog(const GreeterPower &power, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Shut Down Computer"));
    auto *icon = new QLabel(this);
    icon->setPixmap(QIcon::fromTheme(QStringLiteral("system-shutdown")).pixmap(32));
    icon->setAlignment(Qt::AlignTop);
    auto *question = new QLabel(tr("Are you sure you want to:"), this);
    m_shutDown = new QRadioButton(tr("&Shut down the computer?"), this);
    m_restart = new QRadioButton(tr("&Restart the computer?"), this);
    m_standBy = new QRadioButton(tr("Put the computer on s&tand by?"), this);
    m_shutDown->setEnabled(power.shutDown);
    m_restart->setEnabled(power.restart);
    m_standBy->setVisible(power.standBy);
    if (power.shutDown)
        m_shutDown->setChecked(true);
    else if (power.restart)
        m_restart->setChecked(true);
    else if (power.standBy)
        m_standBy->setChecked(true);

    auto *yes = button(tr("&Yes"), this);
    yes->setDefault(true);
    yes->setEnabled(power.shutDown || power.restart || power.standBy);
    auto *no = button(tr("&No"), this);
    connect(yes, &QPushButton::clicked, this, &QDialog::accept);
    connect(no, &QPushButton::clicked, this, &QDialog::reject);

    auto *options = new QVBoxLayout;
    options->addWidget(question);
    options->addWidget(m_shutDown);
    options->addWidget(m_restart);
    options->addWidget(m_standBy);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(yes);
    buttons->addWidget(no);
    buttons->addStretch();

    auto *grid = new QGridLayout(this);
    grid->addWidget(icon, 0, 0);
    grid->addLayout(options, 0, 1);
    grid->addLayout(buttons, 1, 0, 1, 2);
    grid->setSizeConstraint(QLayout::SetFixedSize);
}

PowerAction GreeterShutDownDialog::chosen() const
{
    if (m_restart->isChecked())
        return PowerAction::Restart;
    if (m_standBy->isChecked())
        return PowerAction::StandBy;
    return PowerAction::ShutDown;
}

QRadioButton *GreeterShutDownDialog::option(PowerAction action) const
{
    switch (action) {
    case PowerAction::ShutDown:
        return m_shutDown;
    case PowerAction::Restart:
        return m_restart;
    case PowerAction::StandBy:
        return m_standBy;
    }
    return m_shutDown;
}

} // namespace Daytona
