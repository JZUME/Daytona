// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "qwayland-wlr-output-management-unstable-v1.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QtWaylandClient/QWaylandClientExtension>
#include <functional>

namespace Daytona {

class Settings;

// Reads and sets a screen's scale (NT's Small and Large Fonts). Screens are
// named as QScreen::name() names them (the compositor's output name, such as
// "eDP-1"). Display Properties uses this; tests give it a stand-in.
class ScreenScaler
{
public:
    virtual ~ScreenScaler() = default;

    // The screen's scale, or 0 when the screen is unknown.
    virtual double scale(const QString &screen) const = 0;
    // What the screen's saved scale is filed under: the monitor itself
    // (make, model and serial number) when it reports a serial number,
    // otherwise the connector it is plugged into.
    virtual QString identity(const QString &screen) const = 0;
    // Asks for a new scale; done(ok) is called once the compositor has
    // applied it or refused.
    virtual void setScale(
        const QString &screen, double scale, const std::function<void(bool)> &done = {})
        = 0;
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
};

// See ScreenScaler::identity.
QString outputIdentity(const OutputHead &head);

class OutputHeadObject;
class OutputConfiguration;

// Binds zwlr_output_manager_v1 (wlr-output-management, as wlr-randr uses):
// follows the screens and sets their scale. With settings, it also gives
// each screen its saved scale when it appears, including at login.
class OutputManager : public QWaylandClientExtensionTemplate<OutputManager>,
                      public QtWayland::zwlr_output_manager_v1,
                      public ScreenScaler
{
    Q_OBJECT

public:
    explicit OutputManager(QObject *parent = nullptr);
    ~OutputManager() override;

    // Saved scales to restore (Settings::outputScale).
    void setSettings(Settings *settings) { m_settings = settings; }

    QList<OutputHead> heads() const;

    double scale(const QString &screen) const override;
    QString identity(const QString &screen) const override;
    void setScale(
        const QString &screen, double scale, const std::function<void(bool)> &done = {}) override;

Q_SIGNALS:
    // The screens or their settings changed.
    void changed();

protected:
    void zwlr_output_manager_v1_head(struct ::zwlr_output_head_v1 *head) override;
    void zwlr_output_manager_v1_done(uint32_t serial) override;
    void zwlr_output_manager_v1_finished() override;

private:
    friend class OutputHeadObject;
    friend class OutputConfiguration;

    struct Request
    {
        QString screen;
        double scale;
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
    void restoreSavedScales();
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
