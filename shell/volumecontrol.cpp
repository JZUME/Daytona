// SPDX-License-Identifier: GPL-3.0-or-later

#include "volumecontrol.h"

#include <QAbstractEventDispatcher>
#include <QLoggingCategory>
#include <QTimer>
#include <pulse/context.h>
#include <pulse/error.h>
#include <pulse/glib-mainloop.h>
#include <pulse/introspect.h>
#include <pulse/subscribe.h>
#include <pulse/volume.h>
#include <type_traits>

Q_LOGGING_CATEGORY(lcVolume, "daytona.volume")

namespace Daytona {

static_assert(std::is_same_v<pa_volume_t, quint32>);

namespace {
    // PulseAudio's name for whichever output is the default, so a change of
    // default output needs no separate lookup.
    constexpr char DefaultSink[] = "@DEFAULT_SINK@";
    // After the connection is lost (the sound server stopped or restarted),
    // wait this long before connecting again. The new connection then waits
    // for the server itself.
    constexpr int ReconnectDelayMs = 1000;
} // namespace

struct VolumeControl::Private
{
    VolumeControl *q;
    pa_glib_mainloop *mainloop = nullptr;
    pa_context *context = nullptr;
    bool available = false;
    bool muted = false;
    pa_cvolume channels {};

    void connect();
    void disconnect();
    void refresh();
    void setUnavailable();

    static void stateCallback(pa_context *context, void *data);
    static void subscribeCallback(
        pa_context *context, pa_subscription_event_type_t type, uint32_t index, void *data);
    static void sinkCallback(pa_context *context, const pa_sink_info *info, int eol, void *data);
};

void VolumeControl::Private::connect()
{
    pa_proplist *properties = pa_proplist_new();
    pa_proplist_sets(properties, PA_PROP_APPLICATION_NAME, "Daytona");
    pa_proplist_sets(properties, PA_PROP_APPLICATION_ID, "org.daytona.Shell");
    pa_proplist_sets(properties, PA_PROP_APPLICATION_ICON_NAME, "multimedia-volume-control");
    context
        = pa_context_new_with_proplist(pa_glib_mainloop_get_api(mainloop), "Daytona", properties);
    pa_proplist_free(properties);
    if (!context)
        return;
    pa_context_set_state_callback(context, &Private::stateCallback, this);
    // NOFAIL waits for a server that has not started yet (the shell can start
    // before PipeWire). NOAUTOSPAWN: the session, not the shell, starts it.
    const auto flags = pa_context_flags_t(PA_CONTEXT_NOFAIL | PA_CONTEXT_NOAUTOSPAWN);
    if (pa_context_connect(context, nullptr, flags, nullptr) < 0) {
        qCWarning(lcVolume) << "Cannot connect to the sound server:"
                            << pa_strerror(pa_context_errno(context));
        disconnect();
    }
}

void VolumeControl::Private::disconnect()
{
    if (!context)
        return;
    pa_context_set_state_callback(context, nullptr, nullptr);
    pa_context_set_subscribe_callback(context, nullptr, nullptr);
    pa_context_disconnect(context);
    pa_context_unref(context);
    context = nullptr;
}

void VolumeControl::Private::refresh()
{
    if (pa_operation *op
        = pa_context_get_sink_info_by_name(context, DefaultSink, &Private::sinkCallback, this))
        pa_operation_unref(op);
}

void VolumeControl::Private::setUnavailable()
{
    if (!available)
        return;
    available = false;
    Q_EMIT q->changed();
}

void VolumeControl::Private::stateCallback(pa_context *context, void *data)
{
    auto *d = static_cast<Private *>(data);
    if (context != d->context)
        return;
    switch (pa_context_get_state(context)) {
    case PA_CONTEXT_READY:
        pa_context_set_subscribe_callback(context, &Private::subscribeCallback, d);
        // Sink events report volume and mute changes; server events report a
        // change of default output.
        if (pa_operation *op = pa_context_subscribe(context,
                pa_subscription_mask_t(PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SERVER),
                nullptr, nullptr))
            pa_operation_unref(op);
        d->refresh();
        break;
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
        qCInfo(lcVolume) << "Lost the connection to the sound server";
        d->setUnavailable();
        // Not from inside the context's own callback.
        QTimer::singleShot(ReconnectDelayMs, d->q, [d] {
            d->disconnect();
            d->connect();
        });
        break;
    default:
        break;
    }
}

void VolumeControl::Private::subscribeCallback(
    pa_context *context, pa_subscription_event_type_t, uint32_t, void *data)
{
    auto *d = static_cast<Private *>(data);
    if (context == d->context)
        d->refresh();
}

void VolumeControl::Private::sinkCallback(
    pa_context *context, const pa_sink_info *info, int eol, void *data)
{
    auto *d = static_cast<Private *>(data);
    if (context != d->context)
        return;
    if (eol < 0) {
        // No default output (no sound device).
        d->setUnavailable();
        return;
    }
    if (!info)
        return;
    const bool changed = !d->available || d->muted != bool(info->mute)
        || !pa_cvolume_equal(&d->channels, &info->volume);
    d->available = true;
    d->muted = info->mute;
    d->channels = info->volume;
    if (changed)
        Q_EMIT d->q->changed();
}

VolumeControl::VolumeControl(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    pa_cvolume_init(&d->channels);
}

VolumeControl::~VolumeControl()
{
    d->disconnect();
    if (d->mainloop)
        pa_glib_mainloop_free(d->mainloop);
}

bool VolumeControl::start()
{
    if (d->mainloop)
        return true;
    // libpulse runs on the GLib main loop, which is Qt's own event loop on
    // Linux unless Qt was built without GLib or QT_NO_GLIB is set.
    QAbstractEventDispatcher *dispatcher = QAbstractEventDispatcher::instance();
    if (!dispatcher || !dispatcher->inherits("QEventDispatcherGlib")) {
        qCWarning(lcVolume) << "Qt is not using the GLib event loop; the volume control is not"
                            << "available";
        return false;
    }
    d->mainloop = pa_glib_mainloop_new(nullptr);
    if (!d->mainloop)
        return false;
    d->connect();
    return d->context != nullptr;
}

bool VolumeControl::isAvailable() const
{
    return d->available;
}

int VolumeControl::volume() const
{
    if (!d->available)
        return 0;
    return qMin(100, percentFromVolume(pa_cvolume_max(&d->channels)));
}

bool VolumeControl::isMuted() const
{
    return d->muted;
}

void VolumeControl::setVolume(int percent)
{
    if (!d->available || !d->context)
        return;
    pa_cvolume target = d->channels;
    pa_cvolume_scale(&target, volumeFromPercent(percent));
    if (pa_operation *op
        = pa_context_set_sink_volume_by_name(d->context, DefaultSink, &target, nullptr, nullptr))
        pa_operation_unref(op);
}

void VolumeControl::setMuted(bool muted)
{
    if (!d->available || !d->context)
        return;
    if (pa_operation *op
        = pa_context_set_sink_mute_by_name(d->context, DefaultSink, muted, nullptr, nullptr))
        pa_operation_unref(op);
}

int VolumeControl::percentFromVolume(quint32 volume)
{
    return int((quint64(volume) * 100 + PA_VOLUME_NORM / 2) / PA_VOLUME_NORM);
}

quint32 VolumeControl::volumeFromPercent(int percent)
{
    return quint32((quint64(qBound(0, percent, 100)) * PA_VOLUME_NORM + 50) / 100);
}

} // namespace Daytona
