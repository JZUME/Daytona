// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <functional>

class QComboBox;

namespace Daytona {

class Settings;

// How the Run dialog runs what was typed.
struct RunDecision
{
    enum class Kind
    {
        Nothing,
        Open,
        Command
    };
    Kind kind = Kind::Nothing;
    QString target; // file, folder or URL for Open; the command line for Command
    QString program; // for Command: the program the command line starts
};

// Decides how to run text typed into the Run dialog: an existing file or
// folder ("~" means home) or a URL is opened; anything else is a command.
RunDecision decideRun(const QString &text);

// True if a Command's program can be found (absolute path, or on PATH).
bool programExists(const QString &program);

// NT's Run dialog.
class RunDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RunDialog(Settings *settings, QWidget *parent = nullptr);

    // Prepares the dialog with the history, most recent first, selected.
    void prepare();

    QComboBox *commandBox() const { return m_command; }

    // How errors are shown; a message box by default. Tests replace it.
    std::function<void(const QString &title, const QString &message)> showError;

public Q_SLOTS:
    void accept() override;

private:
    void browse();

    Settings *m_settings;
    QComboBox *m_command;
};

} // namespace Daytona
