// SPDX-License-Identifier: GPL-3.0-or-later

#include "rundialog.h"

#include "settings.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>

Q_LOGGING_CATEGORY(lcRun, "daytona.run", QtInfoMsg)

namespace Daytona {

namespace {

    QString expandHome(const QString &text)
    {
        if (text == QLatin1String("~"))
            return QDir::homePath();
        if (text.startsWith(QLatin1String("~/")))
            return QDir::homePath() + text.mid(1);
        return text;
    }

} // namespace

RunDecision decideRun(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return {};

    // A quoted path, as Browse... produces for names with spaces.
    QString unquoted = trimmed;
    if (unquoted.size() >= 2 && unquoted.startsWith(QLatin1Char('"'))
        && unquoted.endsWith(QLatin1Char('"'))) {
        unquoted = unquoted.mid(1, unquoted.size() - 2);
    }
    const QString path = expandHome(unquoted);
    if (QFileInfo::exists(path) && (QFileInfo(path).isDir() || !QFileInfo(path).isExecutable()))
        return { RunDecision::Kind::Open, QFileInfo(path).absoluteFilePath(), {} };

    const QUrl url(trimmed, QUrl::StrictMode);
    if (url.isValid() && url.scheme().size() > 1 && !trimmed.contains(QLatin1Char(' '))
        && (trimmed.contains(QLatin1String("://")) || url.scheme() == QLatin1String("mailto"))) {
        return { RunDecision::Kind::Open, trimmed, {} };
    }

    const QStringList words = QProcess::splitCommand(trimmed);
    return { RunDecision::Kind::Command, trimmed, expandHome(words.value(0)) };
}

bool programExists(const QString &program)
{
    if (program.isEmpty())
        return false;
    if (program.contains(QLatin1Char('/'))) {
        const QFileInfo info(program);
        return info.isFile() && info.isExecutable();
    }
    return !QStandardPaths::findExecutable(program).isEmpty();
}

RunDialog::RunDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Run"));
    showError = [this](const QString &title, const QString &message) {
        QMessageBox::warning(this, title, message);
    };

    auto *icon = new QLabel(this);
    icon->setPixmap(QIcon::fromTheme(QStringLiteral("system-run")).pixmap(32));
    icon->setAlignment(Qt::AlignTop);
    auto *text = new QLabel(
        tr("Type the name of a program, folder, or document, and Daytona will open it for you."),
        this);
    text->setWordWrap(true);

    auto *openLabel = new QLabel(tr("&Open:"), this);
    m_command = new QComboBox(this);
    m_command->setEditable(true);
    m_command->setInsertPolicy(QComboBox::NoInsert);
    m_command->setMinimumContentsLength(30);
    openLabel->setBuddy(m_command);

    // NT's buttons: OK, Cancel, Browse..., right-aligned.
    auto *ok = new QPushButton(tr("OK"), this);
    ok->setDefault(true);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    auto *browseButton = new QPushButton(tr("&Browse..."), this);
    for (QPushButton *b : { ok, cancel, browseButton })
        b->setMinimumSize(75, 23);
    connect(ok, &QPushButton::clicked, this, &RunDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &RunDialog::reject);
    connect(browseButton, &QPushButton::clicked, this, &RunDialog::browse);
    connect(m_command->lineEdit(), &QLineEdit::textChanged, ok,
        [ok](const QString &t) { ok->setEnabled(!t.trimmed().isEmpty()); });

    auto *grid = new QGridLayout(this);
    grid->addWidget(icon, 0, 0);
    grid->addWidget(text, 0, 1, 1, 2);
    grid->addWidget(openLabel, 1, 0);
    grid->addWidget(m_command, 1, 1, 1, 2);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    buttons->addWidget(browseButton);
    grid->addLayout(buttons, 2, 0, 1, 3);
    grid->setColumnStretch(2, 1);
    setFixedSize(sizeHint().expandedTo(QSize(347, 0)));
}

void RunDialog::prepare()
{
    m_command->clear();
    if (m_settings)
        m_command->addItems(m_settings->runHistory());
    // NT shows the last command, selected, so typing replaces it.
    m_command->setCurrentIndex(m_command->count() > 0 ? 0 : -1);
    m_command->lineEdit()->selectAll();
    m_command->setFocus();
}

void RunDialog::accept()
{
    const QString text = m_command->currentText();
    const RunDecision decision = decideRun(text);
    switch (decision.kind) {
    case RunDecision::Kind::Nothing:
        return;
    case RunDecision::Kind::Open:
        if (!QProcess::startDetached(QStringLiteral("xdg-open"), { decision.target })) {
            showError(text, tr("Cannot open '%1'.").arg(decision.target));
            return;
        }
        break;
    case RunDecision::Kind::Command:
        if (!programExists(decision.program)) {
            showError(text,
                tr("Cannot find the file '%1' (or one of its components). Make sure the path "
                   "and filename are correct and that all required programs are installed.")
                    .arg(decision.program));
            return;
        }
        // The user is typing a command line, so it runs through the shell,
        // as NT's Run did through the command interpreter. This is the one
        // place Daytona uses a shell; it runs as the user and is not a
        // privilege boundary.
        if (!QProcess::startDetached(QStringLiteral("/bin/sh"),
                { QStringLiteral("-c"), decision.target }, QDir::homePath())) {
            showError(text, tr("Cannot run '%1'.").arg(decision.target));
            return;
        }
        break;
    }
    qCDebug(lcRun) << "ran" << text;
    if (m_settings)
        m_settings->addToRunHistory(text.trimmed());
    QDialog::accept();
}

void RunDialog::browse()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Browse"), QDir::homePath());
    if (file.isEmpty())
        return;
    m_command->setEditText(
        file.contains(QLatin1Char(' ')) ? QLatin1Char('"') + file + QLatin1Char('"') : file);
}

} // namespace Daytona
