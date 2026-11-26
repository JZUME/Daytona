// SPDX-License-Identifier: GPL-3.0-or-later

#include "printscreen.h"

#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcPrintScreen, "daytona.printscreen", QtInfoMsg)

namespace Daytona {

PrintScreen::PrintScreen(QObject *parent)
    : QObject(parent)
{
}

QStringList PrintScreen::grimArguments(const QString &output, const QString &file)
{
    QStringList arguments;
    if (!output.isEmpty())
        arguments << QStringLiteral("-o") << output;
    // grim writes PNG by default; "-" is standard output.
    arguments << (file.isEmpty() ? QStringLiteral("-") : file);
    return arguments;
}

namespace {

    // Runs program, calling finished with whether it succeeded once it has
    // exited (or failed to start). input, if not empty, is its standard
    // input.
    void run(QObject *parent, const QString &program, const QStringList &arguments,
        const QByteArray &input, std::function<void(bool, const QByteArray &)> finished)
    {
        if (QStandardPaths::findExecutable(program).isEmpty()) {
            qCWarning(lcPrintScreen) << program << "is not installed";
            finished(false, {});
            return;
        }
        auto *process = new QProcess(parent);
        process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        QObject::connect(process, &QProcess::finished, process,
            [process, program, finished](int exitCode, QProcess::ExitStatus status) {
                const bool ok = status == QProcess::NormalExit && exitCode == 0;
                if (!ok)
                    qCWarning(lcPrintScreen) << program << "failed";
                finished(ok, process->readAllStandardOutput());
                process->deleteLater();
            });
        QObject::connect(process, &QProcess::errorOccurred, process,
            [process, program, finished](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart)
                    return; // finished follows
                qCWarning(lcPrintScreen) << program << "could not be started";
                finished(false, {});
                process->deleteLater();
            });
        process->start(program, arguments);
        if (!input.isEmpty()) {
            process->write(input);
            process->closeWriteChannel();
        }
    }

} // namespace

void PrintScreen::capture(
    const QString &output, const QString &file, std::function<void(bool)> done)
{
    // grim's image is held until it has finished, so a failed capture
    // leaves the clipboard as it was.
    run(this, QStringLiteral("grim"), grimArguments(output, file), {},
        [this, file, done](bool ok, const QByteArray &image) {
            if (!ok || !file.isEmpty()) {
                done(ok);
                return;
            }
            // wl-copy keeps serving the clipboard from a background process
            // of its own, so the one started here exits once it has read
            // the image.
            run(this, QStringLiteral("wl-copy"),
                { QStringLiteral("--type"), QStringLiteral("image/png") }, image,
                [done](bool copied, const QByteArray &) { done(copied); });
        });
}

} // namespace Daytona
