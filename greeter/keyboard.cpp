// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboard.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QFile>
#include <QVariantMap>

namespace Daytona {

SystemKeyboard parseKeyboardDefaults(const QByteArray &text)
{
    SystemKeyboard keyboard;
    const QList<QByteArray> lines = text.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.startsWith("export "))
            line = line.mid(7).trimmed();
        const qsizetype equals = line.indexOf('=');
        if (line.startsWith('#') || equals <= 0)
            continue;
        const QByteArray key = line.left(equals).trimmed();
        QByteArray value = line.mid(equals + 1).trimmed();
        if (value.size() >= 2
            && ((value.startsWith('"') && value.endsWith('"'))
                || (value.startsWith('\'') && value.endsWith('\''))))
            value = value.mid(1, value.size() - 2);
        const QString text = QString::fromUtf8(value);
        if (key == "XKBMODEL")
            keyboard.model = text;
        else if (key == "XKBLAYOUT")
            keyboard.layout = text;
        else if (key == "XKBVARIANT")
            keyboard.variant = text;
        else if (key == "XKBOPTIONS")
            keyboard.options = text;
    }
    return keyboard;
}

SystemKeyboard systemKeyboard()
{
    SystemKeyboard keyboard;
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.locale1"),
        QStringLiteral("/org/freedesktop/locale1"),
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
    call << QStringLiteral("org.freedesktop.locale1");
    const QDBusReply<QVariantMap> reply
        = QDBusConnection::systemBus().call(call, QDBus::Block, 5000);
    if (reply.isValid()) {
        const QVariantMap properties = reply.value();
        keyboard.model = properties.value(QStringLiteral("X11Model")).toString();
        keyboard.layout = properties.value(QStringLiteral("X11Layout")).toString();
        keyboard.variant = properties.value(QStringLiteral("X11Variant")).toString();
        keyboard.options = properties.value(QStringLiteral("X11Options")).toString();
    }
    if (keyboard.isEmpty()) {
        QFile file(QStringLiteral("/etc/default/keyboard"));
        if (file.open(QIODevice::ReadOnly))
            keyboard = parseKeyboardDefaults(file.readAll());
    }
    return keyboard;
}

QString labwcEnvironment(const SystemKeyboard &keyboard)
{
    QString text;
    const auto add = [&text](const char *name, const QString &value) {
        // labwc's environment file has no quoting; values never need it.
        if (!value.isEmpty() && !value.contains(QLatin1Char('\n')))
            text += QLatin1String(name) + QLatin1Char('=') + value + QLatin1Char('\n');
    };
    add("XKB_DEFAULT_MODEL", keyboard.model);
    add("XKB_DEFAULT_LAYOUT", keyboard.layout);
    add("XKB_DEFAULT_VARIANT", keyboard.variant);
    add("XKB_DEFAULT_OPTIONS", keyboard.options);
    return text;
}

} // namespace Daytona
