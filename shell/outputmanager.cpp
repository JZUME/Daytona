// SPDX-License-Identifier: GPL-3.0-or-later

#include "outputmanager.h"

#include "settings.h"

#include <QLoggingCategory>
#include <cmath>
#include <wayland-client-core.h>

Q_LOGGING_CATEGORY(lcOutputs, "daytona.outputs", QtInfoMsg)

namespace Daytona {

namespace {

    uint32_t proxyVersion(void *object)
    {
        return wl_proxy_get_version(static_cast<wl_proxy *>(object));
    }

    bool sameScale(double a, double b)
    {
        return std::abs(a - b) < 0.001;
    }

    void releaseProxy(QtWayland::zwlr_output_mode_v1 *mode)
    {
        if (proxyVersion(mode->object()) >= ZWLR_OUTPUT_MODE_V1_RELEASE_SINCE_VERSION)
            mode->release();
        else
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(mode->object()));
    }

} // namespace

// A mode of a head, kept in its head's list while the compositor offers it.
class OutputModeObject : public QtWayland::zwlr_output_mode_v1
{
public:
    OutputModeObject(struct ::zwlr_output_mode_v1 *object, OutputHeadObject *head)
        : QtWayland::zwlr_output_mode_v1(object)
        , head(head)
    {
    }

    ScreenMode state;
    OutputHeadObject *head;

protected:
    void zwlr_output_mode_v1_size(int32_t width, int32_t height) override
    {
        state.size = QSize(width, height);
    }
    void zwlr_output_mode_v1_refresh(int32_t refresh) override { state.refresh = refresh; }
    void zwlr_output_mode_v1_preferred() override { state.preferred = true; }
    void zwlr_output_mode_v1_finished() override;
};

QString outputIdentity(const OutputHead &head)
{
    if (head.serial.isEmpty())
        return head.name;
    return QStringList { head.make, head.model, head.serial }.join(QLatin1Char(' ')).simplified();
}

class OutputHeadObject : public QtWayland::zwlr_output_head_v1
{
public:
    OutputHeadObject(struct ::zwlr_output_head_v1 *object, OutputManager *manager)
        : QtWayland::zwlr_output_head_v1(object)
        , m_manager(manager)
    {
    }

    OutputHead state; // modes and currentMode are filled in by snapshot()
    QList<OutputModeObject *> modes;
    OutputModeObject *current = nullptr;
    bool restored = false; // its saved settings have been looked at

    OutputHead snapshot() const
    {
        OutputHead head = state;
        head.modes.clear();
        for (const OutputModeObject *mode : modes)
            head.modes.append(mode->state);
        head.currentMode = current ? current->state : ScreenMode();
        return head;
    }

    OutputModeObject *modeFor(const ScreenMode &wanted) const
    {
        const ScreenMode match = matchMode(snapshot().modes, wanted);
        for (OutputModeObject *mode : modes) {
            if (match.isValid() && mode->state.sameAs(match))
                return mode;
        }
        return nullptr;
    }

    void forgetModes()
    {
        for (OutputModeObject *mode : std::as_const(modes))
            mode->head = nullptr;
        modes.clear();
        current = nullptr;
    }

protected:
    void zwlr_output_head_v1_name(const QString &name) override { state.name = name; }
    void zwlr_output_head_v1_description(const QString &description) override
    {
        state.description = description;
    }
    void zwlr_output_head_v1_make(const QString &make) override { state.make = make; }
    void zwlr_output_head_v1_model(const QString &model) override { state.model = model; }
    void zwlr_output_head_v1_serial_number(const QString &serial) override
    {
        state.serial = serial;
    }
    void zwlr_output_head_v1_enabled(int32_t enabled) override { state.enabled = enabled != 0; }
    void zwlr_output_head_v1_scale(wl_fixed_t scale) override
    {
        state.scale = wl_fixed_to_double(scale);
    }
    void zwlr_output_head_v1_mode(struct ::zwlr_output_mode_v1 *mode) override
    {
        modes.append(new OutputModeObject(mode, this));
    }
    void zwlr_output_head_v1_current_mode(struct ::zwlr_output_mode_v1 *mode) override
    {
        current = nullptr;
        for (OutputModeObject *candidate : std::as_const(modes)) {
            if (candidate->object() == mode)
                current = candidate;
        }
    }
    void zwlr_output_head_v1_finished() override
    {
        if (proxyVersion(object()) >= ZWLR_OUTPUT_HEAD_V1_RELEASE_SINCE_VERSION)
            release();
        else
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(object()));
        m_manager->removeHead(this);
    }

private:
    OutputManager *m_manager;
};

void OutputModeObject::zwlr_output_mode_v1_finished()
{
    if (head) {
        head->modes.removeOne(this);
        if (head->current == this)
            head->current = nullptr;
    }
    releaseProxy(this);
    delete this;
}

// One configuration sent to the compositor: every head, with a new scale
// for one of them.
class OutputConfiguration : public QtWayland::zwlr_output_configuration_v1
{
public:
    OutputConfiguration(struct ::zwlr_output_configuration_v1 *object, OutputManager *manager)
        : QtWayland::zwlr_output_configuration_v1(object)
        , m_manager(manager)
    {
    }
    ~OutputConfiguration() override { destroy(); }

protected:
    void zwlr_output_configuration_v1_succeeded() override
    {
        m_manager->configurationDone(OutputManager::Result::Succeeded);
    }
    void zwlr_output_configuration_v1_failed() override
    {
        m_manager->configurationDone(OutputManager::Result::Failed);
    }
    void zwlr_output_configuration_v1_cancelled() override
    {
        m_manager->configurationDone(OutputManager::Result::Cancelled);
    }

private:
    OutputManager *m_manager;
};

OutputManager::OutputManager(QObject *parent)
    : QWaylandClientExtensionTemplate<OutputManager>(4)
{
    setParent(parent);
}

OutputManager::~OutputManager()
{
    delete m_pending;
    for (OutputHeadObject *head : std::as_const(m_heads)) {
        for (OutputModeObject *mode : std::as_const(head->modes)) {
            releaseProxy(mode);
            delete mode;
        }
        head->modes.clear();
        if (proxyVersion(head->object()) >= ZWLR_OUTPUT_HEAD_V1_RELEASE_SINCE_VERSION)
            head->release();
        delete head;
    }
    if (isActive())
        stop();
}

QList<OutputHead> OutputManager::heads() const
{
    QList<OutputHead> result;
    for (const OutputHeadObject *head : m_heads)
        result.append(head->snapshot());
    return result;
}

OutputHeadObject *OutputManager::find(const QString &screen) const
{
    for (OutputHeadObject *head : m_heads) {
        if (head->state.name == screen)
            return head;
    }
    return nullptr;
}

double OutputManager::scale(const QString &screen) const
{
    const OutputHeadObject *head = find(screen);
    return head ? head->state.scale : 0.0;
}

QList<ScreenMode> OutputManager::modes(const QString &screen) const
{
    const OutputHeadObject *head = find(screen);
    return head ? head->snapshot().modes : QList<ScreenMode>();
}

ScreenMode OutputManager::currentMode(const QString &screen) const
{
    const OutputHeadObject *head = find(screen);
    return head && head->current ? head->current->state : ScreenMode();
}

QString OutputManager::identity(const QString &screen) const
{
    const OutputHeadObject *head = find(screen);
    return head ? outputIdentity(head->state) : screen;
}

void OutputManager::zwlr_output_manager_v1_head(struct ::zwlr_output_head_v1 *head)
{
    m_heads.append(new OutputHeadObject(head, this));
}

void OutputManager::zwlr_output_manager_v1_done(uint32_t serial)
{
    m_serial = serial;
    m_haveSerial = true;
    Q_EMIT changed();
    restoreSavedSettings();
    sendNext();
}

void OutputManager::zwlr_output_manager_v1_finished()
{
    qCWarning(lcOutputs) << "The compositor stopped sending screen information";
    m_haveSerial = false;
}

void OutputManager::removeHead(OutputHeadObject *head)
{
    // Its modes are finished on their own; they no longer have a head.
    head->forgetModes();
    m_heads.removeOne(head);
    delete head;
}

void OutputManager::restoreSavedSettings()
{
    if (!m_settings)
        return;
    for (OutputHeadObject *head : std::as_const(m_heads)) {
        if (head->restored || head->state.name.isEmpty() || !head->state.enabled)
            continue;
        head->restored = true;
        const OutputHead state = head->snapshot();
        const QString id = outputIdentity(state);
        // A saved mode the screen no longer offers is left alone.
        ScreenMode mode = matchMode(state.modes, m_settings->outputMode(id));
        if (mode.isValid() && mode.sameAs(state.currentMode))
            mode = {};
        double scale = m_settings->outputScale(id);
        if (scale > 0 && sameScale(scale, state.scale))
            scale = 0;
        if (mode.isValid() || scale > 0) {
            qCInfo(lcOutputs) << "Restoring" << state.name << "mode"
                              << (mode.isValid() ? modeKey(mode) : QStringLiteral("unchanged"))
                              << "scale" << (scale > 0 ? scale : state.scale);
            configure(state.name, mode, scale);
        }
    }
}

void OutputManager::configure(const QString &screen, const ScreenMode &mode, double scale,
    const std::function<void(bool)> &done)
{
    if (!find(screen)) {
        qCWarning(lcOutputs) << "No screen named" << screen;
        if (done)
            done(false);
        return;
    }
    Request request;
    request.screen = screen;
    request.mode = mode;
    request.scale = scale;
    request.done = done;
    m_queue.append(request);
    sendNext();
}

void OutputManager::sendNext()
{
    if (m_pending || m_queue.isEmpty() || !m_haveSerial || !isActive())
        return;
    const Request &request = m_queue.first();
    OutputHeadObject *target = find(request.screen);
    if (!target) {
        finishRequest(false);
        return;
    }
    OutputModeObject *mode = nullptr;
    if (request.mode.isValid()) {
        mode = target->modeFor(request.mode);
        if (!mode) {
            qCWarning(lcOutputs) << request.screen << "has no mode" << modeKey(request.mode);
            finishRequest(false);
            return;
        }
    }
    m_queue.first().attempts++;
    m_pending = new OutputConfiguration(create_configuration(m_serial), this);
    // Every head must be listed; the others keep their state.
    for (OutputHeadObject *head : std::as_const(m_heads)) {
        if (!head->state.enabled) {
            m_pending->disable_head(head->object());
            continue;
        }
        struct ::zwlr_output_configuration_head_v1 *config = m_pending->enable_head(head->object());
        if (head == target && mode)
            zwlr_output_configuration_head_v1_set_mode(config, mode->object());
        if (head == target && request.scale > 0)
            zwlr_output_configuration_head_v1_set_scale(
                config, wl_fixed_from_double(request.scale));
        // It has no events and no destructor; the compositor frees it with
        // the configuration.
        wl_proxy_destroy(reinterpret_cast<wl_proxy *>(config));
    }
    m_pending->apply();
}

void OutputManager::configurationDone(Result result)
{
    // Called from the configuration's own event: free it afterwards.
    OutputConfiguration *configuration = m_pending;
    m_pending = nullptr;
    QMetaObject::invokeMethod(
        this, [configuration] { delete configuration; }, Qt::QueuedConnection);
    if (result == Result::Cancelled && !m_queue.isEmpty() && m_queue.first().attempts < 3) {
        // The screens changed after the configuration was made; try again
        // against their new state once the compositor has described it.
        sendNext();
        return;
    }
    finishRequest(result == Result::Succeeded);
}

void OutputManager::finishRequest(bool ok)
{
    if (m_queue.isEmpty())
        return;
    const Request request = m_queue.takeFirst();
    if (!ok)
        qCWarning(lcOutputs) << "The compositor refused mode"
                             << (request.mode.isValid() ? modeKey(request.mode) : QString())
                             << "scale" << request.scale << "on" << request.screen;
    if (request.done)
        request.done(ok);
    sendNext();
}

} // namespace Daytona
