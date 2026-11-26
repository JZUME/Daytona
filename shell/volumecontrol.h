// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <memory>

namespace Daytona {

// The volume and mute state of the default sound output, read and changed
// through the PulseAudio client library. This also reaches PipeWire through
// pipewire-pulse, Debian 13's default sound server. Changes made elsewhere
// (the volume keys, a mixer) are followed through PulseAudio's change
// subscription, so nothing is polled.
class VolumeControl : public QObject
{
    Q_OBJECT

public:
    explicit VolumeControl(QObject *parent = nullptr);
    ~VolumeControl() override;

    // Connects to the sound server, now or whenever it appears. Returns false
    // if the shell cannot talk to one at all.
    bool start();

    // True while connected and the server has a default output.
    bool isAvailable() const;
    // The default output's volume, 0 to 100. Amplified volumes above 100%
    // (which a mixer can set) are shown as 100.
    int volume() const;
    bool isMuted() const;

    // Sets the default output's volume, keeping its balance between
    // channels.
    void setVolume(int percent);
    void setMuted(bool muted);

    // Between PulseAudio volumes (PA_VOLUME_NORM is 100%) and percentages,
    // the scale pavucontrol and wpctl show.
    static int percentFromVolume(quint32 volume);
    static quint32 volumeFromPercent(int percent);

Q_SIGNALS:
    // The availability, volume or mute state changed.
    void changed();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace Daytona
