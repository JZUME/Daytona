// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationindex.h"
#include "controlpanel.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

DesktopEntry entry(const QString &name, const QStringList &categories)
{
    DesktopEntry e;
    e.id = name.toLower() + QStringLiteral(".desktop");
    e.type = QStringLiteral("Application");
    e.name = name;
    e.comment = name + QStringLiteral(" settings");
    e.exec = QStringLiteral("true");
    e.categories = categories;
    return e;
}

QStringList names(const QListWidget *view)
{
    QStringList out;
    for (int i = 0; i < view->count(); ++i)
        out << view->item(i)->text();
    return out;
}

} // namespace

class TestControlPanel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void appletsFromSettingsCategory()
    {
        const QList<ControlPanelApplet> applets = controlPanelApplets({
            entry(QStringLiteral("Sound"),
                { QStringLiteral("AudioVideo"), QStringLiteral("Settings") }),
            entry(QStringLiteral("Editor"), { QStringLiteral("Utility") }),
            entry(QStringLiteral("Printers"),
                { QStringLiteral("System"), QStringLiteral("Settings") }),
            entry(QStringLiteral("Uncategorised"), {}),
        });
        QStringList got;
        for (const ControlPanelApplet &applet : applets)
            got << applet.name;
        // Daytona's own dialogs first, then the applications, each by name.
        QCOMPARE(got,
            QStringList({ QStringLiteral("Display"), QStringLiteral("Keyboard"),
                QStringLiteral("Mouse"), QStringLiteral("Taskbar"), QStringLiteral("Printers"),
                QStringLiteral("Sound") }));
        QCOMPARE(applets.at(0).builtin, ControlPanelApplet::Builtin::Display);
        QCOMPARE(applets.at(1).builtin, ControlPanelApplet::Builtin::Keyboard);
        QCOMPARE(applets.at(4).builtin, ControlPanelApplet::Builtin::None);
        QCOMPARE(applets.at(4).comment, QStringLiteral("Printers settings"));
        QCOMPARE(applets.at(4).entry.id, QStringLiteral("printers.desktop"));
    }

    void builtinsOnly()
    {
        ControlPanel panel(nullptr);
        QCOMPARE(panel.windowTitle(), QStringLiteral("Control Panel"));
        QCOMPARE(names(panel.builtinView()),
            QStringList({ QStringLiteral("Display"), QStringLiteral("Keyboard"),
                QStringLiteral("Mouse"), QStringLiteral("Taskbar") }));
        QCOMPARE(panel.applicationView()->count(), 0);
        // With no applications, their heading is not shown.
        QVERIFY(panel.applicationHeading()->isHidden());
        QCOMPARE(panel.statusText()->text(), QStringLiteral("4 object(s)"));
        QVERIFY(!panel.openAction()->isEnabled());
    }

    void builtinsOpenDialogs()
    {
        ControlPanel panel(nullptr);
        QSignalSpy display(&panel, &ControlPanel::displayPropertiesRequested);
        QSignalSpy keyboard(&panel, &ControlPanel::keyboardPropertiesRequested);
        QSignalSpy mouse(&panel, &ControlPanel::mousePropertiesRequested);
        QSignalSpy taskbar(&panel, &ControlPanel::taskbarPropertiesRequested);

        QListWidget *view = panel.builtinView();
        view->setCurrentRow(2);
        QCOMPARE(panel.statusText()->text(), QStringLiteral("Changes the mouse pointer scheme."));
        QVERIFY(panel.openAction()->isEnabled());
        panel.openAction()->trigger();
        QCOMPARE(mouse.count(), 1);

        // Double-clicking (or Enter) activates an item.
        Q_EMIT view->itemActivated(view->item(0));
        QCOMPARE(display.count(), 1);
        Q_EMIT view->itemActivated(view->item(3));
        QCOMPARE(taskbar.count(), 1);
        Q_EMIT view->itemActivated(view->item(1));
        QCOMPARE(keyboard.count(), 1);
    }

    void applicationsFromIndex()
    {
        QTemporaryDir dir;
        const auto write = [&dir](const QString &name, const QByteArray &contents) {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(contents);
        };
        write(QStringLiteral("printers.desktop"),
            "[Desktop Entry]\nType=Application\nName=Printers\nComment=Set up printers\n"
            "Exec=true\nCategories=System;Settings;\n");
        write(QStringLiteral("editor.desktop"),
            "[Desktop Entry]\nType=Application\nName=Editor\nExec=true\nCategories=Utility;\n");
        write(QStringLiteral("hidden.desktop"),
            "[Desktop Entry]\nType=Application\nName=Hidden\nExec=true\nNoDisplay=true\n"
            "Categories=Settings;\n");

        ApplicationIndex index;
        QSignalSpy changed(&index, &ApplicationIndex::changed);
        index.start({ dir.path() });
        QTRY_VERIFY(changed.count() > 0);

        ControlPanel panel(&index);
        QCOMPARE(names(panel.applicationView()), QStringList({ QStringLiteral("Printers") }));
        QVERIFY(!panel.applicationHeading()->isHidden());
        QCOMPARE(panel.statusText()->text(), QStringLiteral("5 object(s)"));

        // One selection across both groups.
        panel.builtinView()->setCurrentRow(0);
        panel.applicationView()->setCurrentRow(0);
        QVERIFY(panel.builtinView()->selectedItems().isEmpty());
        QCOMPARE(panel.statusText()->text(), QStringLiteral("Set up printers"));

        // Newly installed settings applications appear.
        write(QStringLiteral("sound.desktop"),
            "[Desktop Entry]\nType=Application\nName=Sound\nExec=true\nCategories=Settings;\n");
        QTRY_COMPARE_WITH_TIMEOUT(names(panel.applicationView()),
            QStringList({ QStringLiteral("Printers"), QStringLiteral("Sound") }), 10000);
    }

    void viewModes()
    {
        ControlPanel panel(nullptr);
        QCOMPARE(panel.viewMode(), ControlPanel::ViewMode::LargeIcons);
        QCOMPARE(panel.builtinView()->viewMode(), QListView::IconMode);
        panel.setViewMode(ControlPanel::ViewMode::SmallIcons);
        QCOMPARE(panel.builtinView()->viewMode(), QListView::ListMode);
        QCOMPARE(panel.builtinView()->iconSize(), QSize(16, 16));
        panel.setViewMode(ControlPanel::ViewMode::List);
        QCOMPARE(panel.builtinView()->flow(), QListView::TopToBottom);
        // One row per applet in a list.
        QCOMPARE(panel.builtinView()->height(), 4 * panel.builtinView()->gridSize().height());
    }
};

QTEST_MAIN(TestControlPanel)
#include "tst_controlpanel.moc"
