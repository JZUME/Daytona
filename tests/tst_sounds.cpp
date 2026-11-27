// SPDX-License-Identifier: GPL-3.0-or-later

#include "eventsounds.h"
#include "settings.h"
#include "soundsproperties.h"
#include "soundtheme.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

using namespace Daytona;

namespace {

void writeFile(const QString &path, const QByteArray &contents = {})
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
}

} // namespace

class TestSounds : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // No gsettings: the tests must not change the real desktop's
        // settings.
        QVERIFY(m_home.isValid());
        qputenv("PATH", m_home.filePath(QStringLiteral("no-bin")).toUtf8());
        qputenv("XDG_CONFIG_HOME", m_home.filePath(QStringLiteral("config")).toUtf8());

        // Two sound directories, as XDG_DATA_HOME and XDG_DATA_DIRS give:
        //   user:   retro (inherits "plain"), freedesktop/stereo/extra.oga
        //   system: freedesktop, plain, hidden
        m_user = m_home.filePath(QStringLiteral("user"));
        m_system = m_home.filePath(QStringLiteral("system"));
        writeFile(m_system + QStringLiteral("/freedesktop/index.theme"),
            "[Sound Theme]\nName=Default\nDirectories=stereo\n\n[stereo]\nOutputProfile=stereo\n");
        for (const char *name : { "bell", "dialog-error", "dialog-warning", "dialog-information",
                 "service-login", "service-logout", "window-question", "message-new-instant" })
            writeFile(
                m_system + QStringLiteral("/freedesktop/stereo/%1.oga").arg(QLatin1String(name)));
        writeFile(m_system + QStringLiteral("/plain/index.theme"),
            "[Sound Theme]\nName=Plain\nDirectories=stereo\n");
        writeFile(m_system + QStringLiteral("/plain/stereo/dialog.wav"));
        writeFile(m_system + QStringLiteral("/plain/stereo/bell.disabled"));
        writeFile(m_system + QStringLiteral("/hidden/index.theme"),
            "[Sound Theme]\nName=Hidden\nHidden=true\nDirectories=stereo\n");
        writeFile(m_user + QStringLiteral("/retro/index.theme"),
            "[Sound Theme]\nName=Retro\nInherits=plain\nDirectories=stereo, mono\n");
        writeFile(m_user + QStringLiteral("/retro/stereo/desktop-login.ogg"));
        writeFile(m_user + QStringLiteral("/retro/mono/dialog-warning.wav"));
        writeFile(m_user + QStringLiteral("/retro/mono/dialog-warning.oga"));
        // A user's addition to a system theme.
        writeFile(m_user + QStringLiteral("/freedesktop/index.theme"),
            "[Sound Theme]\nName=Mine\nDirectories=stereo\n");
        writeFile(m_user + QStringLiteral("/freedesktop/stereo/extra.oga"));
        m_dirs = { m_user, m_system };
    }

    void dataDirs()
    {
        qputenv("XDG_DATA_HOME", "/home/u/.local/share");
        qputenv("XDG_DATA_DIRS", "/usr/local/share:/usr/share/");
        QCOMPARE(soundThemeDirs(),
            QStringList({ QStringLiteral("/home/u/.local/share/sounds"),
                QStringLiteral("/usr/local/share/sounds"), QStringLiteral("/usr/share/sounds") }));
    }

    void installedThemes()
    {
        const QList<SoundThemeInfo> themes = installedSoundThemes(m_dirs);
        QStringList got;
        for (const SoundThemeInfo &theme : themes)
            got << theme.id + QLatin1Char('=') + theme.name;
        // By name, hidden ones left out, the user's copy first.
        QCOMPARE(got,
            QStringList({ QStringLiteral("freedesktop=Mine"), QStringLiteral("plain=Plain"),
                QStringLiteral("retro=Retro") }));
    }

    void lookup()
    {
        const auto find = [this](const char *theme, const char *name) {
            const QString file = findSoundFile(m_dirs, QLatin1String(theme), QLatin1String(name));
            return file.isEmpty() ? QString() : QDir(m_home.path()).relativeFilePath(file);
        };
        // The theme's own, in any of its directories, .oga before .wav.
        QCOMPARE(
            find("retro", "desktop-login"), QStringLiteral("user/retro/stereo/desktop-login.ogg"));
        QCOMPARE(
            find("retro", "dialog-warning"), QStringLiteral("user/retro/mono/dialog-warning.oga"));
        // Then with the last part removed, in the inherited theme.
        QCOMPARE(find("retro", "dialog-error"), QStringLiteral("system/plain/stereo/dialog.wav"));
        QCOMPARE(find("plain", "dialog-question-auth"),
            QStringLiteral("system/plain/stereo/dialog.wav"));
        // A .disabled file turns a sound off, even where freedesktop has one.
        QCOMPARE(find("retro", "bell"), QString());
        // Then the freedesktop theme, every copy of it.
        QCOMPARE(find("retro", "message-new-instant"),
            QStringLiteral("system/freedesktop/stereo/message-new-instant.oga"));
        QCOMPARE(find("freedesktop", "extra"), QStringLiteral("user/freedesktop/stereo/extra.oga"));
        QCOMPARE(find("missing", "bell"), QStringLiteral("system/freedesktop/stereo/bell.oga"));
        QCOMPARE(find("freedesktop", "desktop-login"), QString());
        // Names and themes cannot leave the sound directories.
        QCOMPARE(find("../system/plain", "dialog"), QString());
        QCOMPARE(find("freedesktop", "../freedesktop/stereo/bell"), QString());
    }

    void eventFallbacks()
    {
        // The freedesktop theme names some events differently.
        const SoundEvent *login = soundEvent(QStringLiteral("desktop-login"));
        QVERIFY(login);
        QVERIFY(findEventSoundFile(m_dirs, QStringLiteral("freedesktop"), *login)
                .endsWith(QLatin1String("/service-login.oga")));
        QVERIFY(findEventSoundFile(m_dirs, QStringLiteral("retro"), *login)
                .endsWith(QLatin1String("/desktop-login.ogg")));
        const SoundEvent *question = soundEvent(QStringLiteral("dialog-question"));
        QVERIFY(question);
        QVERIFY(findEventSoundFile(m_dirs, QStringLiteral("freedesktop"), *question)
                .endsWith(QLatin1String("/window-question.oga")));
        QVERIFY(!soundEvent(QStringLiteral("no-such-event")));
        // Every event is a freedesktop name, and the shell plays each.
        for (const SoundEvent &event : soundEvents()) {
            QVERIFY(!event.name.isEmpty() && !event.name.contains(QLatin1Char(' ')));
            QVERIFY(!event.description.isEmpty());
        }
    }

    void themeFiles()
    {
        QStringList names;
        for (const QString &file : soundThemeFiles(m_dirs, QStringLiteral("retro")))
            names << QFileInfo(file).fileName();
        QCOMPARE(names,
            QStringList(
                { QStringLiteral("desktop-login.ogg"), QStringLiteral("dialog-warning.oga") }));
    }

    void gtkSettings()
    {
        const QString path = m_home.filePath(QStringLiteral("gtk/settings.ini"));
        QVERIFY(!readGtkSoundSettings(path).theme);
        writeFile(path,
            "# mine\n[Settings]\ngtk-icon-theme-name=Chicago95\n"
            "gtk-sound-theme-name = \"ubuntu\"\ngtk-enable-event-sounds=false\n");
        GtkSoundSettings read = readGtkSoundSettings(path);
        QCOMPARE(read.theme.value_or(QString()), QStringLiteral("ubuntu"));
        QCOMPARE(read.enabled.value_or(true), false);

        QVERIFY(writeGtkSoundSettings(path, { QStringLiteral("retro"), true }));
        read = readGtkSoundSettings(path);
        QCOMPARE(read.theme.value_or(QString()), QStringLiteral("retro"));
        QCOMPARE(read.enabled.value_or(false), true);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray text = file.readAll();
        // Everything else is kept.
        QVERIFY(text.startsWith("# mine\n[Settings]\ngtk-icon-theme-name=Chicago95\n"));
        QVERIFY(text.contains("gtk-enable-event-sounds=1\n"));
    }

    void eventSoundSettings()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        QCOMPARE(settings.eventSound(QStringLiteral("bell")), QString());
        settings.setEventSound(QStringLiteral("bell"), Settings::EventSoundNone);
        settings.setEventSound(QStringLiteral("dialog-error"), QStringLiteral("/sounds/stop.wav"));
        Settings again(dir.filePath(QStringLiteral("daytona.ini")));
        QCOMPARE(again.eventSound(QStringLiteral("bell")), Settings::EventSoundNone);
        QCOMPARE(
            again.eventSound(QStringLiteral("dialog-error")), QStringLiteral("/sounds/stop.wav"));
        again.setEventSound(QStringLiteral("bell"), QString());
        QCOMPARE(again.eventSound(QStringLiteral("bell")), QString());
        // Relative paths and odd names are ignored.
        again.setEventSound(QStringLiteral("dialog-warning"), QStringLiteral("stop.wav"));
        QCOMPARE(again.eventSound(QStringLiteral("dialog-warning")), QString());
        again.setEventSound(QStringLiteral("a/b"), QStringLiteral("/x.wav"));
        QCOMPARE(again.eventSound(QStringLiteral("a/b")), QString());
    }

    void messageBoxSounds()
    {
        QCOMPARE(MessageBoxSounds::soundFor(QMessageBox::Critical), QStringLiteral("dialog-error"));
        QCOMPARE(
            MessageBoxSounds::soundFor(QMessageBox::Warning), QStringLiteral("dialog-warning"));
        QCOMPARE(MessageBoxSounds::soundFor(QMessageBox::Information),
            QStringLiteral("dialog-information"));
        QCOMPARE(
            MessageBoxSounds::soundFor(QMessageBox::Question), QStringLiteral("dialog-question"));
        QCOMPARE(MessageBoxSounds::soundFor(QMessageBox::NoIcon), QStringLiteral("bell"));
    }

    void logOnOncePerSession()
    {
        QTemporaryDir dir;
        const QString marker = dir.filePath(QStringLiteral("daytona-logon-sound"));
        QVERIFY(firstStartInSession(marker, QStringLiteral("100")));
        // The shell started again in the same session.
        QVERIFY(!firstStartInSession(marker, QStringLiteral("100")));
        // A new session.
        QVERIFY(firstStartInSession(marker, QStringLiteral("200")));
        QVERIFY(!firstStartInSession(marker, QStringLiteral("200")));
        // Outside labwc, or with nowhere to note it, nothing plays.
        QVERIFY(!firstStartInSession(marker, QString()));
        QVERIFY(!firstStartInSession(QString(), QStringLiteral("300")));
    }

    void dialog()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        EventSounds sounds(&settings);
        sounds.setScheme({ QStringLiteral("freedesktop"), true });
        SoundsPropertiesDialog dialog(&settings, &sounds);
        dialog.setSoundDirs(m_dirs);
        dialog.prepare();
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Sounds Properties"));

        // The events by their freedesktop names, in the specification's groups.
        QTreeWidget *events = dialog.eventList();
        QCOMPARE(events->topLevelItemCount(), 2);
        QCOMPARE(events->topLevelItem(0)->text(0), QStringLiteral("Alerts"));
        QCOMPARE(events->topLevelItem(1)->text(0), QStringLiteral("Notifications"));
        QCOMPARE(events->topLevelItem(0)->child(1)->text(0), QStringLiteral("dialog-error"));

        // The themes, then No sounds.
        QComboBox *themes = dialog.themeBox();
        QCOMPARE(themes->count(), 4);
        QCOMPARE(themes->currentText(), QStringLiteral("Mine (freedesktop)"));
        QCOMPARE(themes->itemText(1), QStringLiteral("Plain (plain)"));
        QCOMPARE(themes->itemText(3), QStringLiteral("No sounds"));
        QVERIFY(!dialog.applyButton()->isEnabled());

        dialog.selectEvent(QStringLiteral("desktop-login"));
        QVERIFY(dialog.selectedFile().endsWith(QLatin1String("/service-login.oga")));
        QCOMPARE(dialog.nameBox()->currentText(), QStringLiteral("service-login"));
        QVERIFY(dialog.previewButton()->isEnabled());

        // (None), then a file of one's own.
        dialog.chooseFile(QString());
        QCOMPARE(dialog.selectedFile(), QString());
        QCOMPARE(dialog.nameBox()->currentText(), QStringLiteral("(None)"));
        QVERIFY(!dialog.previewButton()->isEnabled());
        QVERIFY(dialog.applyButton()->isEnabled());
        dialog.selectEvent(QStringLiteral("bell"));
        dialog.chooseFile(m_system + QStringLiteral("/plain/stereo/dialog.wav"));
        QCOMPARE(dialog.nameBox()->currentText(), QStringLiteral("dialog"));
        dialog.apply();
        QCOMPARE(settings.eventSound(QStringLiteral("desktop-login")), Settings::EventSoundNone);
        QCOMPARE(settings.eventSound(QStringLiteral("bell")),
            m_system + QStringLiteral("/plain/stereo/dialog.wav"));
        // Choosing the theme's own sound again follows the theme.
        dialog.chooseFile(
            findSoundFile(m_dirs, QStringLiteral("freedesktop"), QStringLiteral("bell")));
        dialog.apply();
        QCOMPARE(settings.eventSound(QStringLiteral("bell")), QString());

        // A theme gives every event its sound again, and is saved for
        // every program.
        dialog.selectEvent(QStringLiteral("desktop-login"));
        themes->setCurrentIndex(themes->findData(QStringLiteral("retro")));
        Q_EMIT themes->activated(themes->currentIndex());
        QVERIFY(dialog.selectedFile().endsWith(QLatin1String("/retro/stereo/desktop-login.ogg")));
        dialog.apply();
        QCOMPARE(settings.eventSound(QStringLiteral("desktop-login")), QString());
        QCOMPARE(sounds.scheme(), (SoundScheme { QStringLiteral("retro"), true }));
        const QString gtk
            = qEnvironmentVariable("XDG_CONFIG_HOME") + QStringLiteral("/gtk-3.0/settings.ini");
        QCOMPARE(readGtkSoundSettings(gtk).theme.value_or(QString()), QStringLiteral("retro"));

        // No sounds turns event sounds off and leaves nothing to choose.
        themes->setCurrentIndex(3);
        Q_EMIT themes->activated(3);
        QCOMPARE(dialog.selectedFile(), QString());
        QVERIFY(!dialog.nameBox()->isEnabled());
        QVERIFY(!dialog.browseButton()->isEnabled());
        dialog.apply();
        QCOMPARE(sounds.scheme(), (SoundScheme { QStringLiteral("retro"), false }));
        QCOMPARE(readGtkSoundSettings(gtk).enabled.value_or(true), false);

        // Opened again, it shows what was saved.
        SoundsPropertiesDialog again(&settings, &sounds);
        again.setSoundDirs(m_dirs);
        again.prepare();
        QCOMPARE(again.themeBox()->currentText(), QStringLiteral("No sounds"));
    }

    void loadScheme()
    {
        // From GTK's settings.ini, with no gsettings to ask.
        const QString gtk
            = qEnvironmentVariable("XDG_CONFIG_HOME") + QStringLiteral("/gtk-3.0/settings.ini");
        QVERIFY(writeGtkSoundSettings(gtk, { QStringLiteral("plain"), true }));
        EventSounds sounds(nullptr);
        bool ready = false;
        sounds.loadScheme([&ready] { ready = true; });
        QVERIFY(ready);
        QCOMPARE(sounds.scheme(), (SoundScheme { QStringLiteral("plain"), true }));

        QFile::remove(gtk);
        EventSounds fresh(nullptr);
        ready = false;
        fresh.loadScheme([&ready] { ready = true; });
        QTRY_VERIFY(ready);
        QCOMPARE(fresh.scheme(), SoundScheme());
    }

    void playingCallsBack()
    {
        // Whether or not a sound server is there, finished is called.
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        EventSounds sounds(&settings);
        bool finished = false;
        sounds.play(QStringLiteral("dialog-warning"), [&finished] { finished = true; });
        QTRY_VERIFY_WITH_TIMEOUT(finished, 11000);
        // An event with no sound finishes at once.
        settings.setEventSound(QStringLiteral("bell"), Settings::EventSoundNone);
        finished = false;
        sounds.play(QStringLiteral("bell"), [&finished] { finished = true; });
        QVERIFY(finished);
        // So does everything, with event sounds off.
        sounds.setScheme({ QStringLiteral("freedesktop"), false });
        finished = false;
        sounds.play(QStringLiteral("dialog-warning"), [&finished] { finished = true; });
        QTRY_VERIFY_WITH_TIMEOUT(finished, 2000);
    }

private:
    QTemporaryDir m_home;
    QString m_user;
    QString m_system;
    QStringList m_dirs;
};

QTEST_MAIN(TestSounds)
#include "tst_sounds.moc"
