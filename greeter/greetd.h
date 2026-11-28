// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>

class QLocalSocket;

namespace Daytona {

// greetd's IPC (greetd-ipc(7)): JSON messages over the Unix socket named by
// $GREETD_SOCK, each preceded by its length as a 32-bit integer in the
// machine's byte order. greetd answers every request with one response:
// success, error, or auth_message (a PAM prompt or message to answer with
// postResponse()).
class GreetdClient : public QObject
{
    Q_OBJECT

public:
    explicit GreetdClient(QObject *parent = nullptr);

    // Connects to path; false if it cannot.
    bool connectTo(const QString &path);
    bool isConnected() const;

    void createSession(const QString &username);
    // An answer to a prompt; a null response acknowledges a message.
    void postResponse(const QString &response);
    void acknowledge();
    void startSession(const QStringList &command, const QStringList &environment);
    void cancelSession();

    // Frames message as greetd expects, and back; for tests.
    static QByteArray frame(const QJsonObject &message);
    // Takes every whole message from the front of buffer.
    static QList<QJsonObject> unframe(QByteArray &buffer);

Q_SIGNALS:
    void success();
    // errorType is "auth_error" (the password was wrong) or "error".
    void error(const QString &errorType, const QString &description);
    // type is "visible", "secret", "info" or "error".
    void authMessage(const QString &type, const QString &message);
    void disconnected();

private:
    void send(const QJsonObject &message);
    void readMessages();

    QLocalSocket *m_socket;
    QByteArray m_buffer;
};

} // namespace Daytona
