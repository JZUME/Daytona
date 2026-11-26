// SPDX-License-Identifier: GPL-3.0-or-later

#include "inputlocaleicon.h"
#include "inputlocales.h"
#include "keyboardlayout.h"
#include "keyboardproperties.h"
#include "settings.h"

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <csignal>
#include <unistd.h>

using namespace Daytona;

namespace {

InputLocale locale(const char *code)
{
    return InputLocale::fromCode(QString::fromLatin1(code));
}

} // namespace

class TestKeyboard : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString writeFile(const QString &name, const QByteArray &contents)
    {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(contents);
        return path;
    }

    QByteArray readFile(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        // The cut-down layout list.
        qputenv("XKB_CONFIG_ROOT", DAYTONA_TEST_XKB);
        // applyToLabwc() signals labwc; here the test itself stands in.
        std::signal(SIGHUP, SIG_IGN);
        qputenv("LABWC_PID", QByteArray::number(::getpid()));
    }

    void localeCodes()
    {
        QCOMPARE(locale("gb").code(), QStringLiteral("gb"));
        QCOMPARE(locale("de(nodeadkeys)").layout, QStringLiteral("de"));
        QCOMPARE(locale("de(nodeadkeys)").variant, QStringLiteral("nodeadkeys"));
        QCOMPARE(locale(" de ( nodeadkeys ) ").code(), QStringLiteral("de(nodeadkeys)"));
        QVERIFY(!locale("").isValid());
        QVERIFY(!locale("de(").isValid());
        QVERIFY(!locale("de $HOME").isValid());
    }

    void systemKeyboardFile()
    {
        const QString path = writeFile(QStringLiteral("keyboard"),
            "# KEYBOARD CONFIGURATION FILE\n\n"
            "XKBMODEL=\"pc105\"\nXKBLAYOUT=\"gb,de\"\nXKBVARIANT=\",nodeadkeys\"\n"
            "XKBOPTIONS='grp:ctrl_shift_toggle,ctrl:nocaps'\n\nBACKSPACE=\"guess\"\n");
        const XkbConfig config = systemKeyboard(path);
        QCOMPARE(config.model, QStringLiteral("pc105"));
        QCOMPARE(config.locales, QList<InputLocale>({ locale("gb"), locale("de(nodeadkeys)") }));
        QCOMPARE(config.options,
            QStringList(
                { QStringLiteral("grp:ctrl_shift_toggle"), QStringLiteral("ctrl:nocaps") }));
        QCOMPARE(config.hotkey(), LayoutHotkey::CtrlShift);

        // No file: US English.
        const XkbConfig fallback = systemKeyboard(m_dir.filePath(QStringLiteral("missing")));
        QCOMPARE(fallback.locales, QList<InputLocale>({ locale("us") }));
        QVERIFY(fallback.options.isEmpty());
    }

    void hotkeys()
    {
        XkbConfig config;
        QCOMPARE(config.hotkey(), LayoutHotkey::LeftAltShift); // NT's default
        config.options = { QStringLiteral("grp:lalt_lshift_toggle") };
        QCOMPARE(config.hotkey(), LayoutHotkey::LeftAltShift);
        config.options = { QStringLiteral("grp:win_space_toggle") };
        QCOMPARE(config.hotkey(), LayoutHotkey::None);
        QCOMPARE(hotkeyOption(LayoutHotkey::LeftAltShift), QStringLiteral("grp:alt_shift_toggle"));
        QCOMPARE(hotkeyOption(LayoutHotkey::CtrlShift), QStringLiteral("grp:ctrl_shift_toggle"));
        QVERIFY(hotkeyOption(LayoutHotkey::None).isEmpty());
    }

    void sessionFromSettings()
    {
        XkbConfig system;
        system.model = QStringLiteral("pc105");
        system.locales = { locale("gb") };
        system.options = { QStringLiteral("grp:win_space_toggle"), QStringLiteral("ctrl:nocaps") };

        // Nothing chosen: the system's keyboard as it is.
        Settings settings(m_dir.filePath(QStringLiteral("session.ini")));
        QVERIFY(settings.keyboardLayouts().isEmpty());
        QCOMPARE(sessionKeyboard(&settings, system).options, system.options);
        QCOMPARE(sessionKeyboard(nullptr, system).locales, system.locales);

        settings.setKeyboardLayouts({ QStringLiteral("de(nodeadkeys)"), QStringLiteral("gb") });
        XkbConfig config = sessionKeyboard(&settings, system);
        QCOMPARE(config.model, QStringLiteral("pc105"));
        QCOMPARE(config.locales, QList<InputLocale>({ locale("de(nodeadkeys)"), locale("gb") }));
        // The hot key replaces the system's; other options stay.
        QCOMPARE(config.options,
            QStringList({ QStringLiteral("grp:alt_shift_toggle"), QStringLiteral("ctrl:nocaps") }));

        settings.setLayoutHotkey(LayoutHotkey::None);
        QCOMPARE(sessionKeyboard(&settings, system).options,
            QStringList({ QStringLiteral("ctrl:nocaps") }));

        // One locale needs no hot key.
        settings.setLayoutHotkey(LayoutHotkey::CtrlShift);
        settings.setKeyboardLayouts({ QStringLiteral("de") });
        QCOMPARE(sessionKeyboard(&settings, system).options,
            QStringList({ QStringLiteral("ctrl:nocaps") }));

        Settings reread(m_dir.filePath(QStringLiteral("session.ini")));
        QCOMPARE(reread.layoutHotkey(), LayoutHotkey::CtrlShift);
        QCOMPARE(reread.keyboardLayouts(), QStringList({ QStringLiteral("de") }));
    }

    void environmentFile()
    {
        XkbConfig config;
        config.model = QStringLiteral("pc105");
        config.locales = { locale("gb"), locale("de(nodeadkeys)") };
        config.options = { QStringLiteral("grp:alt_shift_toggle") };
        const QByteArray text = labwcEnvironment(config);
        QVERIFY(text.contains("\nXKB_DEFAULT_MODEL=pc105\n"));
        QVERIFY(text.contains("\nXKB_DEFAULT_LAYOUT=gb,de\n"));
        QVERIFY(text.contains("\nXKB_DEFAULT_VARIANT=,nodeadkeys\n"));
        QVERIFY(text.contains("\nXKB_DEFAULT_OPTIONS=grp:alt_shift_toggle\n"));
        QCOMPARE(localesFromEnvironment(text), config.locales);

        // Nothing that labwc would expand or that could start a new line.
        config.options = { QStringLiteral("$HOME"), QStringLiteral("a\nXKB_DEFAULT_LAYOUT=x") };
        const QByteArray unsafe = labwcEnvironment(config);
        QVERIFY(!unsafe.contains('$'));
        QVERIFY(unsafe.contains("\nXKB_DEFAULT_OPTIONS=,\n"));
    }

    void applyWritesAndSignals()
    {
        XkbConfig config;
        config.locales = { locale("us") };
        QString error;
        QVERIFY(!applyToLabwc(config, QString(), &error));
        QVERIFY(applyToLabwc(config, m_dir.path(), &error));
        QVERIFY(readFile(m_dir.filePath(QStringLiteral("environment")))
                .contains("XKB_DEFAULT_LAYOUT=us\n"));
    }

    void registry()
    {
        const LayoutRegistry registry;
        QCOMPARE(registry.layouts().size(), 6);
        const LayoutInfo dvorak = registry.find(locale("us(dvorak)"));
        QCOMPARE(dvorak.description, QStringLiteral("English (Dvorak)"));
        // A variant takes its layout's language and indicator unless it has
        // its own.
        QCOMPARE(dvorak.shortName, QStringLiteral("en"));
        QCOMPARE(dvorak.languages, QStringList({ QStringLiteral("eng") }));
        QCOMPARE(registry.find(locale("de(ru)")).languages, QStringList({ QStringLiteral("rus") }));
        QCOMPARE(LayoutRegistry::indicatorText(registry.find(locale("de(nodeadkeys)"))),
            QStringLiteral("De"));
        QCOMPARE(LayoutRegistry::languageName(QStringLiteral("eng")), QStringLiteral("English"));
        QCOMPARE(LayoutRegistry::languageName(QStringLiteral("ger")), QStringLiteral("German"));
        // Unknown layouts are still shown.
        const LayoutInfo unknown = registry.find(locale("xx(yy)"));
        QCOMPARE(unknown.description, QStringLiteral("xx(yy)"));
        QCOMPARE(LayoutRegistry::indicatorText(unknown), QStringLiteral("Xx"));
    }

    void propertiesDialog()
    {
        const QString system = writeFile(QStringLiteral("dialog-keyboard"), "XKBLAYOUT=gb\n");
        Settings settings(m_dir.filePath(QStringLiteral("dialog.ini")));
        KeyboardPropertiesDialog dialog(&settings);
        dialog.setSystemKeyboardFile(system);
        dialog.prepare();
        QSignalSpy applied(&dialog, &KeyboardPropertiesDialog::applied);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Keyboard Properties"));

        // The installer's layout until something is chosen.
        QCOMPARE(dialog.locales(), QList<InputLocale>({ locale("gb") }));
        QTreeWidget *list = dialog.localeList();
        QCOMPARE(list->topLevelItemCount(), 1);
        QCOMPARE(list->topLevelItem(0)->text(0), QStringLiteral("English"));
        QCOMPARE(list->topLevelItem(0)->text(1), QStringLiteral("English (UK)"));
        QVERIFY(!dialog.removeButton()->isEnabled());
        QVERIFY(!dialog.applyButton()->isEnabled());
        QVERIFY(dialog.hotkeyButton(LayoutHotkey::LeftAltShift)->isChecked());
        QVERIFY(!dialog.hotkeyButton(LayoutHotkey::LeftAltShift)->isEnabled());

        dialog.addLocale(locale("de(nodeadkeys)"));
        QCOMPARE(list->topLevelItemCount(), 2);
        QCOMPARE(list->currentItem()->text(1), QStringLiteral("German (no dead keys)"));
        QVERIFY(dialog.applyButton()->isEnabled());
        QVERIFY(dialog.removeButton()->isEnabled());
        QVERIFY(dialog.defaultButton()->isEnabled());
        QVERIFY(dialog.hotkeyButton(LayoutHotkey::CtrlShift)->isEnabled());
        // Adding one already there just selects it.
        dialog.addLocale(locale("gb"));
        QCOMPARE(list->topLevelItemCount(), 2);
        QCOMPARE(list->currentItem(), list->topLevelItem(0));
        QVERIFY(!dialog.defaultButton()->isEnabled());

        list->setCurrentItem(list->topLevelItem(1));
        dialog.setSelectedAsDefault();
        QCOMPARE(dialog.locales(), QList<InputLocale>({ locale("de(nodeadkeys)"), locale("gb") }));
        QVERIFY(dialog.defaultLabel()->text().contains(QStringLiteral("German (no dead keys)")));
        dialog.hotkeyButton(LayoutHotkey::CtrlShift)->setChecked(true);

        dialog.apply();
        QCOMPARE(applied.count(), 1);
        QCOMPARE(settings.keyboardLayouts(),
            QStringList({ QStringLiteral("de(nodeadkeys)"), QStringLiteral("gb") }));
        QCOMPARE(settings.layoutHotkey(), LayoutHotkey::CtrlShift);
        QVERIFY(!dialog.applyButton()->isEnabled());

        list->setCurrentItem(list->topLevelItem(0));
        dialog.removeSelected();
        QCOMPARE(dialog.locales(), QList<InputLocale>({ locale("gb") }));
        QVERIFY(!dialog.removeButton()->isEnabled());
        dialog.reject();
        // Cancelled: the saved settings stay.
        dialog.prepare();
        QCOMPARE(dialog.locales().size(), 2);
        QVERIFY(dialog.hotkeyButton(LayoutHotkey::CtrlShift)->isChecked());
    }

    void addDialog()
    {
        const LayoutRegistry registry;
        AddInputLocaleDialog dialog(&registry);
        // The test runs in the C locale: English.
        QCOMPARE(dialog.languageBox()->currentText(), QStringLiteral("English"));
        const int german = dialog.languageBox()->findText(QStringLiteral("German"));
        QVERIFY(german >= 0);
        dialog.languageBox()->setCurrentIndex(german);
        QCOMPARE(dialog.layoutBox()->count(), 2);
        QCOMPARE(dialog.layoutBox()->itemText(0), QStringLiteral("German"));
        dialog.layoutBox()->setCurrentIndex(1);
        QCOMPARE(dialog.selectedLocale(), locale("de(nodeadkeys)"));
    }

    void indicatorFollowsGroup()
    {
        QTemporaryDir labwc;
        const QString system
            = writeFile(QStringLiteral("indicator-keyboard"), "XKBLAYOUT=\"us,de\"\n");
        InputLocales locales(nullptr, labwc.path());
        locales.setSystemKeyboardFile(system);
        locales.start();
        InputLocaleIcon icon(&locales);
        QCOMPARE(icon.text(), QStringLiteral("En"));
        QCOMPARE(icon.toolTip(), QStringLiteral("English (US)"));
        QVERIFY(!icon.isHidden());

        // The hot key changed the keyboard's group.
        locales.setGroup(1);
        QCOMPARE(icon.text(), QStringLiteral("De"));
        QMenu *menu = icon.localeMenu();
        QCOMPARE(menu->actions().size(), 2);
        QVERIFY(!menu->actions().at(0)->isChecked());
        QVERIFY(menu->actions().at(1)->isChecked());
        QCOMPARE(menu->actions().at(1)->text(), QStringLiteral("German"));
        QCOMPARE(menu->actions().at(0)->text(), QStringLiteral("English (US)"));

        // Choosing English again: labwc's keymap must change for the group
        // to go back to the first layout, so it is given German first, then
        // English first again.
        const QString environment = labwc.filePath(QStringLiteral("environment"));
        menu->actions().at(0)->trigger();
        QCOMPARE(icon.text(), QStringLiteral("En"));
        QVERIFY(readFile(environment).contains("XKB_DEFAULT_LAYOUT=de,us\n"));
        QTRY_VERIFY(readFile(environment).contains("XKB_DEFAULT_LAYOUT=us,de\n"));

        // Choosing German puts it first.
        locales.select(1);
        QCOMPARE(locales.locales(), QList<InputLocale>({ locale("de"), locale("us") }));
        QCOMPARE(locales.currentIndex(), 0);
        QCOMPARE(icon.text(), QStringLiteral("De"));
        QVERIFY(readFile(environment).contains("XKB_DEFAULT_LAYOUT=de,us\n"));

        // A restarted shell keeps labwc's order.
        InputLocales restarted(nullptr, labwc.path());
        restarted.setSystemKeyboardFile(system);
        restarted.start();
        QCOMPARE(restarted.locales(), QList<InputLocale>({ locale("de"), locale("us") }));

        // One locale: no indicator.
        const QString single = writeFile(QStringLiteral("single-keyboard"), "XKBLAYOUT=us\n");
        InputLocales one(nullptr, QString());
        one.setSystemKeyboardFile(single);
        one.start();
        InputLocaleIcon hidden(&one);
        QVERIFY(hidden.isHidden());
    }

    void reloadAppliesSettings()
    {
        QTemporaryDir labwc;
        const QString system = writeFile(QStringLiteral("reload-keyboard"),
            "XKBMODEL=pc105\nXKBLAYOUT=us\nXKBOPTIONS=ctrl:nocaps\n");
        Settings settings(m_dir.filePath(QStringLiteral("reload.ini")));
        InputLocales locales(&settings, labwc.path());
        locales.setSystemKeyboardFile(system);
        locales.start();
        QSignalSpy changed(&locales, &InputLocales::changed);
        settings.setKeyboardLayouts({ QStringLiteral("gb"), QStringLiteral("de") });
        locales.reload();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(locales.locales(), QList<InputLocale>({ locale("gb"), locale("de") }));
        const QByteArray text = readFile(labwc.filePath(QStringLiteral("environment")));
        QVERIFY(text.contains("XKB_DEFAULT_MODEL=pc105\n"));
        QVERIFY(text.contains("XKB_DEFAULT_LAYOUT=gb,de\n"));
        QVERIFY(text.contains("XKB_DEFAULT_OPTIONS=grp:alt_shift_toggle,ctrl:nocaps\n"));
    }
};

QTEST_MAIN(TestKeyboard)
#include "tst_keyboard.moc"
