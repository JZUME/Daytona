// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "qwayland-wlr-output-management-unstable-v1.h"

#include <QList>
#include <QString>
#include <QtWaylandClient/QWaylandClientExtension>

namespace Daytona {

class ScaleHead;
class ScaleConfiguration;

// Gives every screen the same scale through wlr-output-management (as
// wlr-randr --scale does), for high-density screens, including screens
// plugged in later. The compositor draws the window frames and the pointer
// at that scale too, and Qt follows.
class OutputScaler : public QWaylandClientExtensionTemplate<OutputScaler>,
                     public QtWayland::zwlr_output_manager_v1
{
    Q_OBJECT

public:
    explicit OutputScaler(double scale, QObject *parent = nullptr);
    ~OutputScaler() override;

protected:
    void zwlr_output_manager_v1_head(struct ::zwlr_output_head_v1 *head) override;
    void zwlr_output_manager_v1_done(uint32_t serial) override;

private:
    friend class ScaleHead;
    friend class ScaleConfiguration;
    void finished(bool ok);

    double m_scale;
    QList<ScaleHead *> m_heads;
    ScaleConfiguration *m_pending = nullptr;
    // A refused scale is not asked for again until a screen is added.
    bool m_refused = false;
};

} // namespace Daytona
