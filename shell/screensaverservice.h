// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QDBusContext>
#include <QHash>
#include <QObject>

class QDBusServiceWatcher;

namespace Daytona {

// org.freedesktop.ScreenSaver's idle inhibition, which video players and
// presentation programs (VLC, LibreOffice Impress, Electron apps) use rather
// than Wayland's idle-inhibit protocol. While any inhibit is held the screen
// saver does not start; an inhibit ends with UnInhibit or when its caller
// leaves the bus. Served on /org/freedesktop/ScreenSaver and /ScreenSaver,
// as applications use both.
class ScreenSaverService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")

public:
    static constexpr const char *ServiceName = "org.freedesktop.ScreenSaver";

    explicit ScreenSaverService(QDBusConnection connection, QObject *parent = nullptr);

    // Takes the name. Returns false if another program has it.
    bool registerOnBus();

    bool isInhibited() const { return !m_inhibits.isEmpty(); }

public Q_SLOTS:
    Q_SCRIPTABLE uint Inhibit(const QString &applicationName, const QString &reason);
    Q_SCRIPTABLE void UnInhibit(uint cookie);

Q_SIGNALS:
    void inhibitedChanged(bool inhibited);

private:
    void remove(const QList<uint> &cookies);

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_watcher;
    // Cookie to the unique bus name of the program holding it.
    QHash<uint, QString> m_inhibits;
    uint m_nextCookie = 1;
};

} // namespace Daytona
