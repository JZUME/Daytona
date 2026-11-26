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

    // A mode of a head. Nothing here needs modes, but every object the
    // compositor creates must have a listener, so each is wrapped and freed
    // when the compositor is done with it.
    class OutputMode : public QtWayland::zwlr_output_mode_v1
    {
    public:
        explicit OutputMode(struct ::zwlr_output_mode_v1 *object)
            : QtWayland::zwlr_output_mode_v1(object)
        {
        }

    protected:
        void zwlr_output_mode_v1_finished() override
        {
            if (proxyVersion(object()) >= ZWLR_OUTPUT_MODE_V1_RELEASE_SINCE_VERSION)
                release();
            else
                wl_proxy_destroy(reinterpret_cast<wl_proxy *>(object()));
            delete this;
        }
    };

} // namespace

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

    OutputHead state;
    bool restored = false; // its saved scale has been looked at

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
        new OutputMode(mode);
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
        result.append(head->state);
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
    restoreSavedScales();
    sendNext();
}

void OutputManager::zwlr_output_manager_v1_finished()
{
    qCWarning(lcOutputs) << "The compositor stopped sending screen information";
    m_haveSerial = false;
}

void OutputManager::removeHead(OutputHeadObject *head)
{
    m_heads.removeOne(head);
    delete head;
}

void OutputManager::restoreSavedScales()
{
    if (!m_settings)
        return;
    for (OutputHeadObject *head : std::as_const(m_heads)) {
        if (head->restored || head->state.name.isEmpty() || !head->state.enabled)
            continue;
        head->restored = true;
        const QString id = outputIdentity(head->state);
        const double saved = m_settings->outputScale(id);
        if (saved > 0 && !sameScale(saved, head->state.scale)) {
            qCInfo(lcOutputs) << "Restoring scale" << saved << "on" << head->state.name;
            setScale(head->state.name, saved);
        }
    }
}

void OutputManager::setScale(
    const QString &screen, double scale, const std::function<void(bool)> &done)
{
    if (!find(screen)) {
        qCWarning(lcOutputs) << "No screen named" << screen;
        if (done)
            done(false);
        return;
    }
    m_queue.append({ screen, scale, done });
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
    m_queue.first().attempts++;
    m_pending = new OutputConfiguration(create_configuration(m_serial), this);
    // Every head must be listed; the others keep their state.
    for (OutputHeadObject *head : std::as_const(m_heads)) {
        if (!head->state.enabled) {
            m_pending->disable_head(head->object());
            continue;
        }
        struct ::zwlr_output_configuration_head_v1 *config = m_pending->enable_head(head->object());
        if (head == target)
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
        qCWarning(lcOutputs) << "The compositor refused scale" << request.scale << "on"
                             << request.screen;
    if (request.done)
        request.done(ok);
    sendNext();
}

} // namespace Daytona
