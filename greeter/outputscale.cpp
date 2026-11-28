// SPDX-License-Identifier: GPL-3.0-or-later

#include "outputscale.h"

#include <QLoggingCategory>
#include <cmath>
#include <wayland-client-core.h>

Q_LOGGING_CATEGORY(lcScale, "daytona.greeter.scale", QtInfoMsg)

namespace Daytona {

// A screen, with what the compositor last said about it. Its modes are not
// needed: the compositor keeps each screen's mode when only the scale
// changes.
class ScaleHead : public QtWayland::zwlr_output_head_v1
{
public:
    ScaleHead(struct ::zwlr_output_head_v1 *object, OutputScaler *scaler)
        : QtWayland::zwlr_output_head_v1(object)
        , m_scaler(scaler)
    {
    }

    bool enabled = false;
    double scale = 1.0;

protected:
    void zwlr_output_head_v1_enabled(int32_t on) override { enabled = on; }
    void zwlr_output_head_v1_scale(wl_fixed_t value) override { scale = wl_fixed_to_double(value); }
    void zwlr_output_head_v1_finished() override
    {
        m_scaler->m_heads.removeOne(this);
        if (wl_proxy_get_version(reinterpret_cast<wl_proxy *>(object()))
            >= ZWLR_OUTPUT_HEAD_V1_RELEASE_SINCE_VERSION)
            release();
        else
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(object()));
        delete this;
    }

private:
    OutputScaler *m_scaler;
};

class ScaleConfiguration : public QtWayland::zwlr_output_configuration_v1
{
public:
    ScaleConfiguration(struct ::zwlr_output_configuration_v1 *object, OutputScaler *scaler)
        : QtWayland::zwlr_output_configuration_v1(object)
        , m_scaler(scaler)
    {
    }
    ~ScaleConfiguration() override { destroy(); }

protected:
    void zwlr_output_configuration_v1_succeeded() override { m_scaler->finished(true); }
    void zwlr_output_configuration_v1_failed() override { m_scaler->finished(false); }
    // Screens changed meanwhile; the next done event tries again.
    void zwlr_output_configuration_v1_cancelled() override { m_scaler->finished(true); }

private:
    OutputScaler *m_scaler;
};

OutputScaler::OutputScaler(double scale, QObject *parent)
    : QWaylandClientExtensionTemplate<OutputScaler>(2)
    , m_scale(scale)
{
    setParent(parent);
    initialize();
    if (!isActive())
        qCWarning(lcScale) << "The compositor cannot change screen scales"
                           << "(no wlr-output-management)";
}

OutputScaler::~OutputScaler()
{
    delete m_pending;
    for (ScaleHead *head : std::as_const(m_heads)) {
        wl_proxy_destroy(reinterpret_cast<wl_proxy *>(head->object()));
        delete head;
    }
    if (isActive())
        stop();
}

void OutputScaler::zwlr_output_manager_v1_head(struct ::zwlr_output_head_v1 *head)
{
    m_heads.append(new ScaleHead(head, this));
    m_refused = false;
}

void OutputScaler::zwlr_output_manager_v1_done(uint32_t serial)
{
    if (m_pending || m_refused)
        return;
    bool needed = false;
    for (const ScaleHead *head : std::as_const(m_heads)) {
        if (head->enabled && std::abs(head->scale - m_scale) > 0.001)
            needed = true;
    }
    if (!needed)
        return;
    qCInfo(lcScale) << "Setting every screen's scale to" << m_scale;
    m_pending = new ScaleConfiguration(create_configuration(serial), this);
    // Every head must be listed; switched-off ones stay off.
    for (ScaleHead *head : std::as_const(m_heads)) {
        if (!head->enabled) {
            m_pending->disable_head(head->object());
            continue;
        }
        struct ::zwlr_output_configuration_head_v1 *config = m_pending->enable_head(head->object());
        zwlr_output_configuration_head_v1_set_scale(config, wl_fixed_from_double(m_scale));
        // No events and no destructor; freed with the configuration.
        wl_proxy_destroy(reinterpret_cast<wl_proxy *>(config));
    }
    m_pending->apply();
}

void OutputScaler::finished(bool ok)
{
    if (!ok) {
        qCWarning(lcScale) << "The compositor refused scale" << m_scale;
        m_refused = true;
    }
    // Called from the configuration's own event: free it afterwards.
    ScaleConfiguration *configuration = m_pending;
    m_pending = nullptr;
    QMetaObject::invokeMethod(
        this, [configuration] { delete configuration; }, Qt::QueuedConnection);
}

} // namespace Daytona
