// SPDX-License-Identifier: GPL-3.0-or-later

#include "eventsounds.h"

#include "gtktheme.h"
#include "settings.h"

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <canberra.h>

Q_LOGGING_CATEGORY(lcSounds, "daytona.sounds", QtInfoMsg)

namespace Daytona {

namespace {

    // Sounds Properties' Preview, so that a new preview stops the last one.
    constexpr uint32_t PreviewId = 0xffffffffu;

    // A sound never holds up its caller (log off) for longer than this.
    constexpr int MaxSoundMs = 10000;

    const QString GSettingsSchema = QStringLiteral("org.gnome.desktop.sound");

    QString gsettings()
    {
        return QStandardPaths::findExecutable(QStringLiteral("gsettings"));
    }

    // A GVariant text value: 'name' or true/false.
    QString unquote(QString value)
    {
        value = value.trimmed();
        if (value.size() >= 2 && value.startsWith(QLatin1Char('\''))
            && value.endsWith(QLatin1Char('\'')))
            value = value.mid(1, value.size() - 2);
        return value;
    }

} // namespace

EventSounds::EventSounds(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    if (const int error = ca_context_create(&m_context); error < 0) {
        qCWarning(lcSounds) << "Cannot use libcanberra:" << ca_strerror(error);
        m_context = nullptr;
        return;
    }
    ca_context_change_props(m_context, CA_PROP_APPLICATION_NAME, "Daytona", CA_PROP_APPLICATION_ID,
        "org.daytona.Shell", CA_PROP_APPLICATION_ICON_NAME, "preferences-desktop-sound", nullptr);
    setScheme(m_scheme);
}

EventSounds::~EventSounds()
{
    if (m_context)
        ca_context_destroy(m_context);
}

void EventSounds::setScheme(const SoundScheme &scheme)
{
    m_scheme = scheme;
    if (m_scheme.theme.isEmpty())
        m_scheme.theme = QString::fromLatin1(SoundScheme::DefaultTheme);
    if (!m_context)
        return;
    ca_context_change_props(m_context, CA_PROP_CANBERRA_XDG_THEME_NAME,
        m_scheme.theme.toUtf8().constData(), CA_PROP_CANBERRA_ENABLE, m_scheme.enabled ? "1" : "0",
        nullptr);
}

void EventSounds::loadScheme(Finished ready)
{
    const GtkSoundSettings gtk = readGtkSoundSettings(gtkSettingsPath());
    if (gtk.theme) {
        setScheme({ *gtk.theme, gtk.enabled.value_or(true) });
        ready();
        return;
    }
    const QString program = gsettings();
    if (program.isEmpty()) {
        setScheme({ QString::fromLatin1(SoundScheme::DefaultTheme), gtk.enabled.value_or(true) });
        ready();
        return;
    }
    auto *process = new QProcess(this);
    auto *limit = new QTimer(process);
    limit->setSingleShot(true);
    const bool enabledInIni = gtk.enabled.value_or(true);
    const bool knownInIni = gtk.enabled.has_value();
    auto finish = [this, process, ready, enabledInIni, knownInIni](bool ok) {
        SoundScheme scheme;
        scheme.enabled = enabledInIni;
        if (ok) {
            // "org.gnome.desktop.sound theme-name 'freedesktop'"
            const QStringList lines = QString::fromUtf8(process->readAllStandardOutput())
                                          .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                const QStringList parts = line.split(QLatin1Char(' '));
                if (parts.size() < 3)
                    continue;
                const QString value = unquote(parts.mid(2).join(QLatin1Char(' ')));
                if (parts.at(1) == QLatin1String("theme-name") && !value.isEmpty())
                    scheme.theme = value;
                else if (parts.at(1) == QLatin1String("event-sounds") && !knownInIni)
                    scheme.enabled = value != QLatin1String("false");
            }
        }
        process->disconnect(this);
        process->deleteLater();
        setScheme(scheme);
        ready();
    };
    connect(process, &QProcess::finished, this, [finish](int code, QProcess::ExitStatus status) {
        finish(status == QProcess::NormalExit && code == 0);
    });
    connect(process, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(false);
    });
    connect(limit, &QTimer::timeout, this, [process, finish] {
        process->kill();
        finish(false);
    });
    process->start(program, { QStringLiteral("list-recursively"), GSettingsSchema });
    limit->start(2000);
}

void EventSounds::saveScheme(const SoundScheme &scheme)
{
    setScheme(scheme);
    QString error;
    if (!writeGtkSoundSettings(gtkSettingsPath(), m_scheme, &error))
        qCWarning(lcSounds) << "Cannot save the sound theme:" << error;
    // GTK 3 on Wayland, libcanberra's GTK module and GSound read GSettings.
    // Without gsettings (or its schema) there is nothing more to tell.
    const QString program = gsettings();
    if (program.isEmpty())
        return;
    QProcess::startDetached(program,
        { QStringLiteral("set"), GSettingsSchema, QStringLiteral("theme-name"), m_scheme.theme });
    QProcess::startDetached(program,
        { QStringLiteral("set"), GSettingsSchema, QStringLiteral("event-sounds"),
            m_scheme.enabled ? QStringLiteral("true") : QStringLiteral("false") });
}

void EventSounds::play(const QString &name, Finished finished)
{
    const QString chosen = m_settings ? m_settings->eventSound(name) : QString();
    if (chosen == Settings::EventSoundNone) {
        if (finished)
            finished();
        return;
    }
    if (!chosen.isEmpty()) {
        start(m_nextId++, { {}, std::move(finished) }, chosen, false);
        return;
    }
    QStringList names { name };
    if (const SoundEvent *event = soundEvent(name))
        names += event->fallbacks;
    start(m_nextId++, { names, std::move(finished) }, {}, false);
}

void EventSounds::playFile(const QString &file, Finished finished)
{
    start(m_nextId++, { {}, std::move(finished) }, file, false);
}

void EventSounds::preview(const QString &file)
{
    if (m_context)
        ca_context_cancel(m_context, PreviewId);
    if (m_pending.contains(PreviewId))
        done(PreviewId, CA_ERROR_CANCELED);
    start(PreviewId, {}, file, true);
}

void EventSounds::start(uint32_t id, Pending pending, const QString &file, bool force)
{
    if (m_nextId == PreviewId)
        m_nextId = 1;
    if (!m_context || (file.isEmpty() && pending.names.isEmpty())) {
        if (pending.finished)
            pending.finished();
        return;
    }
    ca_proplist *properties = nullptr;
    ca_proplist_create(&properties);
    if (file.isEmpty()) {
        const QByteArray name = pending.names.takeFirst().toUtf8();
        ca_proplist_sets(properties, CA_PROP_EVENT_ID, name.constData());
        // Played often (message boxes): worth keeping in the sound server.
        ca_proplist_sets(properties, CA_PROP_CANBERRA_CACHE_CONTROL, "volatile");
    } else {
        ca_proplist_sets(properties, CA_PROP_MEDIA_FILENAME, QFile::encodeName(file).constData());
    }
    if (force)
        ca_proplist_sets(properties, CA_PROP_CANBERRA_ENABLE, "1");
    m_pending.insert(id, std::move(pending));
    const int error = ca_context_play_full(m_context, id, properties, &EventSounds::callback, this);
    ca_proplist_destroy(properties);
    if (error < 0) {
        // No callback follows; finish later, as a callback would.
        QMetaObject::invokeMethod(
            this, [this, id, error] { done(id, error); }, Qt::QueuedConnection);
        return;
    }
    // A sound server that never answers must not hold up log off.
    if (m_pending.value(id).finished) {
        QTimer::singleShot(MaxSoundMs, this, [this, id] {
            if (m_pending.contains(id))
                done(id, CA_ERROR_CANCELED);
        });
    }
}

void EventSounds::done(uint32_t id, int error)
{
    auto it = m_pending.find(id);
    if (it == m_pending.end())
        return;
    Pending pending = std::move(it.value());
    m_pending.erase(it);
    if (error == CA_ERROR_NOTFOUND && !pending.names.isEmpty()) {
        start(id, std::move(pending), {}, false);
        return;
    }
    if (error < 0 && error != CA_ERROR_DISABLED && error != CA_ERROR_CANCELED)
        qCDebug(lcSounds) << "Sound" << id << "not played:" << ca_strerror(error);
    if (pending.finished)
        pending.finished();
}

void EventSounds::callback(ca_context *, uint32_t id, int error, void *userdata)
{
    // libcanberra calls this from its own thread.
    auto *self = static_cast<EventSounds *>(userdata);
    QMetaObject::invokeMethod(
        self, [self, id, error] { self->done(id, error); }, Qt::QueuedConnection);
}

MessageBoxSounds::MessageBoxSounds(EventSounds *sounds, QObject *parent)
    : QObject(parent)
    , m_sounds(sounds)
{
}

QString MessageBoxSounds::soundFor(int icon)
{
    switch (QMessageBox::Icon(icon)) {
    case QMessageBox::Critical:
        return QStringLiteral("dialog-error");
    case QMessageBox::Warning:
        return QStringLiteral("dialog-warning");
    case QMessageBox::Information:
        return QStringLiteral("dialog-information");
    case QMessageBox::Question:
        return QStringLiteral("dialog-question");
    case QMessageBox::NoIcon:
        break;
    }
    return QStringLiteral("bell");
}

bool MessageBoxSounds::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Show && !event->spontaneous()) {
        if (auto *box = qobject_cast<QMessageBox *>(watched))
            m_sounds->play(soundFor(box->icon()));
    }
    return QObject::eventFilter(watched, event);
}

bool firstStartInSession(const QString &marker, const QString &sessionId)
{
    if (marker.isEmpty() || sessionId.isEmpty())
        return false;
    QFile in(marker);
    if (in.open(QIODevice::ReadOnly) && in.read(64).trimmed() == sessionId.toUtf8())
        return false;
    in.close();
    QSaveFile out(marker);
    if (!out.open(QIODevice::WriteOnly) || out.write(sessionId.toUtf8() + '\n') < 0
        || !out.commit()) {
        // Better silent than playing at every restart of the shell.
        qCWarning(lcSounds) << "Cannot write" << marker << "; the log on sound is not played";
        return false;
    }
    return true;
}

} // namespace Daytona
