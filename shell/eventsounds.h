// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "soundtheme.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <functional>

struct ca_context;

namespace Daytona {

class Settings;

// Plays the shell's event sounds through libcanberra, the reference
// implementation of the freedesktop sound theme specification (theme
// lookup, inheritance, and decoding the Ogg Vorbis files themes ship),
// using the sound server through its PulseAudio backend.
//
// The sound theme and "event sounds off" are the desktop-wide GTK setting
// (see SoundScheme), so that the choice made in Sounds Properties applies to
// every program. Sounds Properties can also give an event a file of its own
// or no sound (Settings::eventSound), which the shell's own sounds follow.
class EventSounds : public QObject
{
    Q_OBJECT

public:
    using Finished = std::function<void()>;

    explicit EventSounds(Settings *settings, QObject *parent = nullptr);
    ~EventSounds() override;

    SoundScheme scheme() const { return m_scheme; }
    // Uses scheme from now on, without saving it.
    void setScheme(const SoundScheme &scheme);
    // Reads the scheme: GTK's settings.ini, else GSettings (asked
    // asynchronously, with a time limit), else the freedesktop theme. ready
    // is called once it is known.
    void loadScheme(Finished ready);
    // Saves scheme for every program (settings.ini and GSettings) and uses it.
    void saveScheme(const SoundScheme &scheme);

    // Plays the event called name (a freedesktop sound name), or the sound
    // chosen for it. finished is called once it has played, or at once if it
    // cannot be played.
    void play(const QString &name, Finished finished = {});
    // Plays a file for an event (a notification's sound-file), unless event
    // sounds are off.
    void playFile(const QString &file, Finished finished = {});
    // Sounds Properties' Preview: plays file even when event sounds are
    // off, stopping the previous preview.
    void preview(const QString &file);

private:
    struct Pending
    {
        QStringList names; // still to try if the theme lacks the current one
        Finished finished;
    };

    void start(uint32_t id, Pending pending, const QString &file, bool force);
    void done(uint32_t id, int error);
    static void callback(ca_context *context, uint32_t id, int error, void *userdata);

    Settings *m_settings;
    ca_context *m_context = nullptr;
    SoundScheme m_scheme;
    QHash<uint32_t, Pending> m_pending;
    uint32_t m_nextId = 1;
};

// Plays a message box's sound when it is shown, as NT did: the sound for its
// icon (dialog-error, dialog-warning, dialog-information, dialog-question),
// or bell without one. Installed on the application.
class MessageBoxSounds : public QObject
{
    Q_OBJECT

public:
    explicit MessageBoxSounds(EventSounds *sounds, QObject *parent = nullptr);

    // The sound name for a QMessageBox::Icon.
    static QString soundFor(int icon);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    EventSounds *m_sounds;
};

// The log on sound plays once a session, not each time the shell starts:
// true the first time for the compositor in sessionId (its process id),
// noted in marker.
bool firstStartInSession(const QString &marker, const QString &sessionId);

} // namespace Daytona
