// SPDX-License-Identifier: GPL-3.0-or-later

#include "greetd.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <cstdint>
#include <cstring>

Q_LOGGING_CATEGORY(lcGreetd, "daytona.greeter.greetd", QtInfoMsg)

namespace Daytona {

GreetdClient::GreetdClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
{
    connect(m_socket, &QLocalSocket::readyRead, this, &GreetdClient::readMessages);
    connect(m_socket, &QLocalSocket::disconnected, this, &GreetdClient::disconnected);
}

bool GreetdClient::connectTo(const QString &path)
{
    m_socket->connectToServer(path);
    if (!m_socket->waitForConnected(5000)) {
        qCWarning(lcGreetd) << "Cannot connect to greetd at" << path << m_socket->errorString();
        return false;
    }
    return true;
}

bool GreetdClient::isConnected() const
{
    return m_socket->state() == QLocalSocket::ConnectedState;
}

void GreetdClient::createSession(const QString &username)
{
    send({ { QStringLiteral("type"), QStringLiteral("create_session") },
        { QStringLiteral("username"), username } });
}

void GreetdClient::postResponse(const QString &response)
{
    send({ { QStringLiteral("type"), QStringLiteral("post_auth_message_response") },
        { QStringLiteral("response"), response } });
}

void GreetdClient::acknowledge()
{
    send({ { QStringLiteral("type"), QStringLiteral("post_auth_message_response") },
        { QStringLiteral("response"), QJsonValue::Null } });
}

void GreetdClient::startSession(const QStringList &command, const QStringList &environment)
{
    send({ { QStringLiteral("type"), QStringLiteral("start_session") },
        { QStringLiteral("cmd"), QJsonArray::fromStringList(command) },
        { QStringLiteral("env"), QJsonArray::fromStringList(environment) } });
}

void GreetdClient::cancelSession()
{
    send({ { QStringLiteral("type"), QStringLiteral("cancel_session") } });
}

QByteArray GreetdClient::frame(const QJsonObject &message)
{
    const QByteArray json = QJsonDocument(message).toJson(QJsonDocument::Compact);
    const std::uint32_t length = json.size();
    QByteArray data(sizeof length, Qt::Uninitialized);
    std::memcpy(data.data(), &length, sizeof length);
    return data + json;
}

QList<QJsonObject> GreetdClient::unframe(QByteArray &buffer)
{
    QList<QJsonObject> messages;
    std::uint32_t length = 0;
    while (buffer.size() >= qsizetype(sizeof length)) {
        std::memcpy(&length, buffer.constData(), sizeof length);
        if (buffer.size() < qsizetype(sizeof length + length))
            break;
        const QJsonDocument document = QJsonDocument::fromJson(buffer.mid(sizeof length, length));
        buffer.remove(0, sizeof length + length);
        if (document.isObject())
            messages.append(document.object());
    }
    return messages;
}

void GreetdClient::send(const QJsonObject &message)
{
    if (!isConnected()) {
        qCWarning(lcGreetd) << "Not connected to greetd";
        return;
    }
    m_socket->write(frame(message));
    m_socket->flush();
}

void GreetdClient::readMessages()
{
    m_buffer += m_socket->readAll();
    const QList<QJsonObject> messages = unframe(m_buffer);
    for (const QJsonObject &message : messages) {
        const QString type = message.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("success")) {
            Q_EMIT success();
        } else if (type == QLatin1String("error")) {
            Q_EMIT error(message.value(QStringLiteral("error_type")).toString(),
                message.value(QStringLiteral("description")).toString());
        } else if (type == QLatin1String("auth_message")) {
            Q_EMIT authMessage(message.value(QStringLiteral("auth_message_type")).toString(),
                message.value(QStringLiteral("auth_message")).toString());
        } else {
            qCWarning(lcGreetd) << "Unknown message from greetd:" << type;
        }
    }
}

} // namespace Daytona
