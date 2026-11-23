// SPDX-License-Identifier: GPL-3.0-or-later

// Makes the Daytona style available to any Qt 6 application, for example
// with QT_STYLE_OVERRIDE=daytona.

#include "daytonastyle.h"

#include <QStylePlugin>

class DaytonaStylePlugin : public QStylePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QStyleFactoryInterface_iid FILE "daytona.json")

public:
    QStyle *create(const QString &key) override
    {
        if (key.compare(QLatin1String("daytona"), Qt::CaseInsensitive) == 0)
            return new Daytona::Style;
        return nullptr;
    }
};

#include "plugin.moc"
