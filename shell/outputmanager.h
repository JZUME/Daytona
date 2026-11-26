// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "qwayland-wlr-output-management-unstable-v1.h"
#include "screenmode.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QtWaylandClient/QWaylandClientExtension>
#include <functional>

namespace Daytona {

class Settings;

// Reads and changes a screen's mode (NT's Desktop Area and Refresh
// Frequency) and scale (NT's Font Size). Screens are named as
// QScreen::name() names them (the compositor's output name, such as
// "eDP-1"). Display Properties uses this; tests give it a stand-in.
class ScreenSettings
{
public:
    virtual ~ScreenSettings() = default;

    // The screen's scale, or 0 when the screen is unknown.
    virtual double scale(const QString &screen) const = 0;
    // The modes the screen offers, and the one in use (invalid if unknown).
    virtual QList<ScreenMode> modes(const QString &screen) const = 0;
    virtual ScreenMode currentMode(const QString &screen) const = 0;
    // What the screen's saved settings are filed under: the monitor itself
    // (make, model and serial number) when it reports a serial number,
    // otherwise the connector it is plugged into.
    virtual QString identity(const QString &screen) const = 0;
    // Asks for a new mode (if mode is valid) and scale (if scale > 0),
    // together; done(ok) is called once the compositor has applied them or
    // refused.
    virtual void configure(const QString &screen, const ScreenMode &mode, double scale,
        const std::function<void(bool)> &done = {})
        = 0;

    void setScale(const QString &screen, double scale, const std::function<void(bool)> &done = {})
    {
        configure(screen, {}, scale, done);
    }
};

// A screen as zwlr_output_head_v1 describes it.
struct OutputHead
{
    QString name;
    QString description;
    QString make;
    QString model;
    QString serial;
    bool enabled = false;
    double scale = 1.0;
    QList<ScreenMode> modes;
    ScreenMode currentMode;
};

// See ScreenSettings::identity.
QString outputIdentity(const OutputHead &head);

class OutputHeadObject;
class OutputModeObject;
class OutputConfiguration;

// Binds zwlr_output_manager_v1 (wlr-output-management, as wlr-randr uses):
// follows the screens and their modes, and sets their mode and scale. With
// settings, it also gives each screen its saved mode and scale when it
// appears, including at login.
class OutputManager : public QWaylandClientExtensionTemplate<OutputManager>,
                      public QtWayland::zwlr_output_manager_v1,
                      public ScreenSettings
{
    Q_OBJECT

public:
    explicit OutputManager(QObject *parent = nullptr);
    ~OutputManager() override;

    // Saved modes and scales to restore (Settings::outputMode/outputScale).
    void setSettings(Settings *settings) { m_settings = settings; }

    QList<OutputHead> heads() const;

    double scale(const QString &screen) const override;
    QList<ScreenMode> modes(const QString &screen) const override;
    ScreenMode currentMode(const QString &screen) const override;
    QString identity(const QString &screen) const override;
    void configure(const QString &screen, const ScreenMode &mode, double scale,
        const std::function<void(bool)> &done = {}) override;

Q_SIGNALS:
    // The screens or their settings changed.
    void changed();

protected:
    void zwlr_output_manager_v1_head(struct ::zwlr_output_head_v1 *head) override;
    void zwlr_output_manager_v1_done(uint32_t serial) override;
    void zwlr_output_manager_v1_finished() override;

private:
    friend class OutputHeadObject;
    friend class OutputModeObject;
    friend class OutputConfiguration;

    struct Request
    {
        QString screen;
        ScreenMode mode;
        double scale = 0;
        std::function<void(bool)> done;
        int attempts = 0;
    };
    enum class Result
    {
        Succeeded,
        Failed,
        Cancelled
    };

    OutputHeadObject *find(const QString &screen) const;
    void removeHead(OutputHeadObject *head);
    void restoreSavedSettings();
    void sendNext();
    void configurationDone(Result result);
    void finishRequest(bool ok);

    QList<OutputHeadObject *> m_heads;
    QPointer<Settings> m_settings;
    uint32_t m_serial = 0;
    bool m_haveSerial = false;
    QList<Request> m_queue;
    OutputConfiguration *m_pending = nullptr;
};

} // namespace Daytona
