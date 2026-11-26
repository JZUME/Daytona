// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "qwayland-wlr-output-management-unstable-v1.h"
#include "screenmode.h"

#include <QList>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QtWaylandClient/QWaylandClientExtension>
#include <functional>
#include <optional>

namespace Daytona {

class Settings;

// A change to one screen, as part of a set made together. Anything left
// unset stays as it is.
struct ScreenChange
{
    QString screen;
    ScreenMode mode; // invalid: unchanged
    double scale = 0; // 0: unchanged
    std::optional<QPoint> position; // top left, in the desktop's logical pixels
    std::optional<bool> enabled;
};

// Reads and changes a screen's mode (NT's Desktop Area and Refresh
// Frequency), scale (NT's Font Size), and where it sits on the desktop and
// whether it is on (Windows 2000's monitor arrangement). Screens are named
// as QScreen::name() names them (the compositor's output name, such as
// "eDP-1"). Display Properties uses this; tests give it a stand-in.
class ScreenSettings
{
public:
    virtual ~ScreenSettings() = default;

    // Every screen, switched on or not, in the compositor's order. Empty
    // when not known; the dialog then shows only its own screen.
    virtual QStringList screens() const { return {}; }
    // What the screen calls itself ("Maker Model (DP-1)"), for people.
    virtual QString description(const QString &screen) const { return screen; }
    virtual bool isEnabled(const QString &screen) const { return !screen.isEmpty(); }
    // Its top left on the desktop, in logical pixels.
    virtual QPoint position(const QString &) const { return {}; }
    // The size it takes on the desktop with mode and scale.
    virtual QSize logicalSize(const QString &, const ScreenMode &mode, double scale) const
    {
        if (!mode.isValid() || scale <= 0)
            return {};
        return QSize(qRound(mode.size.width() / scale), qRound(mode.size.height() / scale));
    }
    // The screen's scale, or 0 when the screen is unknown.
    virtual double scale(const QString &screen) const = 0;
    // The modes the screen offers, and the one in use (invalid if unknown
    // or switched off).
    virtual QList<ScreenMode> modes(const QString &screen) const = 0;
    virtual ScreenMode currentMode(const QString &screen) const = 0;
    // What the screen's saved settings are filed under: the monitor itself
    // (make, model and serial number) when it reports a serial number,
    // otherwise the connector it is plugged into.
    virtual QString identity(const QString &screen) const = 0;
    // Asks for the changes, together; done(ok) is called once the
    // compositor has applied them or refused.
    virtual void change(const QList<ScreenChange> &changes, const std::function<void(bool)> &done)
        = 0;

    // A new mode (if mode is valid) and scale (if scale > 0) for one screen.
    void configure(const QString &screen, const ScreenMode &mode, double scale,
        const std::function<void(bool)> &done = {})
    {
        ScreenChange one;
        one.screen = screen;
        one.mode = mode;
        one.scale = scale;
        change({ one }, done);
    }
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
    QPoint position;
    int transform = 0; // wl_output_transform
    double scale = 1.0;
    QList<ScreenMode> modes;
    ScreenMode currentMode;
};

// See ScreenSettings::identity.
QString outputIdentity(const OutputHead &head);

// The size the head takes on the desktop: its mode (the current one, else
// its preferred one) turned and divided by its scale.
QSize logicalSize(const OutputHead &head);

class OutputHeadObject;
class OutputModeObject;
class OutputConfiguration;

// Binds zwlr_output_manager_v1 (wlr-output-management, as wlr-randr uses):
// follows the screens and their modes, and changes them. With settings, it
// also gives each screen its saved mode, scale, place and on/off state when
// it appears, including at login.
class OutputManager : public QWaylandClientExtensionTemplate<OutputManager>,
                      public QtWayland::zwlr_output_manager_v1,
                      public ScreenSettings
{
    Q_OBJECT

public:
    explicit OutputManager(QObject *parent = nullptr);
    ~OutputManager() override;

    // Saved settings to restore (Settings::outputMode, outputScale,
    // outputPosition and outputEnabled).
    void setSettings(Settings *settings) { m_settings = settings; }

    QList<OutputHead> heads() const;

    QStringList screens() const override;
    QString description(const QString &screen) const override;
    bool isEnabled(const QString &screen) const override;
    QPoint position(const QString &screen) const override;
    QSize logicalSize(const QString &screen, const ScreenMode &mode, double scale) const override;
    double scale(const QString &screen) const override;
    QList<ScreenMode> modes(const QString &screen) const override;
    ScreenMode currentMode(const QString &screen) const override;
    QString identity(const QString &screen) const override;
    void change(
        const QList<ScreenChange> &changes, const std::function<void(bool)> &done = {}) override;

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
        QList<ScreenChange> changes;
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
