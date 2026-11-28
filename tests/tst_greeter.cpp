// SPDX-License-Identifier: GPL-3.0-or-later

// The greeter against a scripted greetd: the requests it sends for each PAM
// conversation, and the session files and keyboard settings it reads.

#include "greetd.h"
#include "greeter.h"
#include "keyboard.h"
#include "logonui.h"
#include "sessions.h"

#include <QApplication>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

// greetd's side of the socket.
class FakeGreetd : public QObject
{
public:
    explicit FakeGreetd(const QString &path)
    {
        QLocalServer::removeServer(path);
        m_server.listen(path);
        connect(&m_server, &QLocalServer::newConnection, this, [this] {
            m_client = m_server.nextPendingConnection();
            connect(m_client, &QLocalSocket::readyRead, this, [this] {
                m_buffer += m_client->readAll();
                m_requests += GreetdClient::unframe(m_buffer);
            });
        });
    }

    // Waits for the greeter's next request.
    QJsonObject next()
    {
        if (m_requests.isEmpty() && !QTest::qWaitFor([this] { return !m_requests.isEmpty(); }))
            return {};
        return m_requests.takeFirst();
    }
    bool idle()
    {
        QTest::qWait(50);
        return m_requests.isEmpty();
    }

    void send(const QJsonObject &message)
    {
        QVERIFY(m_client);
        m_client->write(GreetdClient::frame(message));
        m_client->flush();
    }
    void success() { send({ { "type", "success" } }); }
    void authMessage(const QString &type, const QString &text)
    {
        send({ { "type", "auth_message" }, { "auth_message_type", type },
            { "auth_message", text } });
    }
    void error(const QString &type, const QString &description)
    {
        send({ { "type", "error" }, { "error_type", type }, { "description", description } });
    }

private:
    QLocalServer m_server;
    QPointer<QLocalSocket> m_client;
    QByteArray m_buffer;
    QList<QJsonObject> m_requests;
};

class FakePower : public PowerActions
{
public:
    void queryAvailability(Availability done) override { done(available); }
    void perform(PowerAction action, Failure) override { performed << action; }
    GreeterPower available;
    QList<PowerAction> performed;
};

QList<DesktopSession> testSessions()
{
    DesktopSession daytona;
    daytona.id = "daytona";
    daytona.name = "Daytona";
    daytona.command = { "daytona-session" };
    daytona.desktopNames = { "Daytona" };
    DesktopSession other;
    other.id = "other";
    other.name = "Other Desktop";
    other.command = { "other-session", "--flag" };
    return { daytona, other };
}

template <typename T> T *waitForOpen(Greeter &greeter)
{
    T *dialog = nullptr;
    const bool shown = QTest::qWaitFor([&] {
        dialog = qobject_cast<T *>(greeter.openDialog());
        return dialog && dialog->isVisible();
    });
    return shown ? dialog : nullptr;
}

} // namespace

class TestGreeter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void framing();
    void logOn();
    void wrongPassword();
    void expiredPassword();
    void expiredPasswordRefused();
    void infoAndOneTimeCode();
    void cancelPrompt();
    void rememberedUserAndSession();
    void chooseSession();
    void cancelClears();
    void shutDown();

    void sessionFiles();
    void splitExec_data();
    void splitExec();
    void keyboardDefaults();

private:
    void logOnAs(const QString &user, const QString &password);

    QTemporaryDir m_dir;
    FakeGreetd *m_greetd = nullptr;
    GreetdClient *m_client = nullptr;
    FakePower *m_power = nullptr;
    Greeter *m_greeter = nullptr;
    QString m_state;
};

void TestGreeter::init()
{
    QVERIFY(m_dir.isValid());
    const QString path = m_dir.filePath("greetd.sock");
    m_state = m_dir.filePath("state");
    m_greetd = new FakeGreetd(path);
    m_client = new GreetdClient;
    QVERIFY(m_client->connectTo(path));
    m_power = new FakePower;
    m_greeter = new Greeter(m_client, testSessions(), m_state, m_power);
    m_greeter->dialog()->show();
}

void TestGreeter::cleanup()
{
    delete m_greeter;
    delete m_power;
    delete m_client;
    delete m_greetd;
    QFile::remove(m_state);
}

void TestGreeter::logOnAs(const QString &user, const QString &password)
{
    LogonDialog *dialog = m_greeter->dialog();
    dialog->userField()->setText(user);
    dialog->passwordField()->setText(password);
    QTest::keyClick(dialog->passwordField(), Qt::Key_Return);
}

void TestGreeter::framing()
{
    QByteArray data = GreetdClient::frame({ { "type", "success" } });
    data += GreetdClient::frame({ { "type", "cancel_session" } });
    QByteArray partial = data.left(data.size() - 3);
    QCOMPARE(GreetdClient::unframe(partial).size(), 1);
    QCOMPARE(
        partial.size(), data.size() - 3 - GreetdClient::frame({ { "type", "success" } }).size());
    partial += data.right(3);
    const QList<QJsonObject> rest = GreetdClient::unframe(partial);
    QCOMPARE(rest.size(), 1);
    QCOMPARE(rest.first().value("type").toString(), QString("cancel_session"));
    QVERIFY(partial.isEmpty());
}

void TestGreeter::logOn()
{
    QSignalSpy finished(m_greeter, &Greeter::finished);
    logOnAs("alice", "secret");
    QJsonObject request = m_greetd->next();
    QCOMPARE(request.value("type").toString(), QString("create_session"));
    QCOMPARE(request.value("username").toString(), QString("alice"));
    QVERIFY(!m_greeter->dialog()->okButton()->isEnabled());

    m_greetd->authMessage("secret", "Password: ");
    request = m_greetd->next();
    QCOMPARE(request.value("type").toString(), QString("post_auth_message_response"));
    QCOMPARE(request.value("response").toString(), QString("secret"));

    m_greetd->success();
    request = m_greetd->next();
    QCOMPARE(request.value("type").toString(), QString("start_session"));
    QCOMPARE(request.value("cmd").toArray().toVariantList(), QVariantList { "daytona-session" });
    const QStringList env = request.value("env").toVariant().toStringList();
    QVERIFY(env.contains("XDG_SESSION_TYPE=wayland"));
    QVERIFY(env.contains("XDG_SESSION_DESKTOP=daytona"));
    QVERIFY(env.contains("XDG_CURRENT_DESKTOP=Daytona"));
    QCOMPARE(finished.size(), 0);

    m_greetd->success();
    QVERIFY(finished.wait());
    const QSettings state(m_state, QSettings::IniFormat);
    QCOMPARE(state.value("Last/User").toString(), QString("alice"));
    QCOMPARE(state.value("Last/Session").toString(), QString("daytona"));
    // Never the password.
    QFile file(m_state);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(!file.readAll().contains("secret"));
}

void TestGreeter::wrongPassword()
{
    logOnAs("alice", "wrong");
    QCOMPARE(m_greetd->next().value("type").toString(), QString("create_session"));
    m_greetd->authMessage("secret", "Password: ");
    QCOMPARE(m_greetd->next().value("response").toString(), QString("wrong"));
    m_greetd->error("auth_error", "pam_authenticate: AUTH_ERR");

    // The session is cancelled, and the failure explained.
    QCOMPARE(m_greetd->next().value("type").toString(), QString("cancel_session"));
    auto *box = waitForOpen<QMessageBox>(*m_greeter);
    QVERIFY(box);
    QVERIFY(box->text().startsWith("The system could not log you on."));
    m_greetd->success();
    QTest::keyClick(box, Qt::Key_Return);

    // User name kept, password cleared, ready to try again.
    LogonDialog *dialog = m_greeter->dialog();
    QTRY_VERIFY(dialog->okButton()->isEnabled());
    QCOMPARE(dialog->userField()->text(), QString("alice"));
    QVERIFY(dialog->passwordField()->text().isEmpty());

    logOnAs("alice", "right");
    QCOMPARE(m_greetd->next().value("type").toString(), QString("create_session"));
    m_greetd->authMessage("secret", "Password: ");
    QCOMPARE(m_greetd->next().value("response").toString(), QString("right"));
}

void TestGreeter::expiredPassword()
{
    QSignalSpy finished(m_greeter, &Greeter::finished);
    logOnAs("bob", "old");
    m_greetd->next();
    m_greetd->authMessage("secret", "Password: ");
    QCOMPARE(m_greetd->next().value("response").toString(), QString("old"));

    // pam_unix's conversation for an expired password.
    m_greetd->authMessage(
        "info", "You are required to change your password immediately (administrator enforced).");
    auto *box = waitForOpen<QMessageBox>(*m_greeter);
    QVERIFY(box);
    QVERIFY(box->text().contains("change your password"));
    QTest::keyClick(box, Qt::Key_Return);
    QJsonObject request = m_greetd->next();
    QCOMPARE(request.value("type").toString(), QString("post_auth_message_response"));
    QVERIFY(request.value("response").isNull());

    m_greetd->authMessage("secret", "Current password: ");
    auto *prompt = waitForOpen<PromptDialog>(*m_greeter);
    QVERIFY(prompt);
    QCOMPARE(prompt->label()->text(), QString("Current password:"));
    QCOMPARE(prompt->field()->echoMode(), QLineEdit::Password);
    QTest::keyClicks(prompt->field(), "old");
    QTest::keyClick(prompt->field(), Qt::Key_Return);
    QCOMPARE(m_greetd->next().value("response").toString(), QString("old"));

    m_greetd->authMessage("secret", "New password: ");
    prompt = waitForOpen<PromptDialog>(*m_greeter);
    QVERIFY(prompt);
    QCOMPARE(prompt->label()->text(), QString("New password:"));
    QTest::keyClicks(prompt->field(), "new");
    QTest::keyClick(prompt->field(), Qt::Key_Return);
    QCOMPARE(m_greetd->next().value("response").toString(), QString("new"));

    m_greetd->authMessage("secret", "Retype new password: ");
    prompt = waitForOpen<PromptDialog>(*m_greeter);
    QVERIFY(prompt);
    QCOMPARE(prompt->label()->text(), QString("Retype new password:"));
    QTest::keyClicks(prompt->field(), "new");
    QTest::keyClick(prompt->field(), Qt::Key_Return);
    QCOMPARE(m_greetd->next().value("response").toString(), QString("new"));

    m_greetd->success();
    QCOMPARE(m_greetd->next().value("type").toString(), QString("start_session"));
    m_greetd->success();
    QVERIFY(finished.wait());
}

void TestGreeter::expiredPasswordRefused()
{
    // What greetd 0.10 does with an expired password: PAM's message, then
    // an account error instead of the change password prompts.
    logOnAs("bob", "old");
    m_greetd->next();
    m_greetd->authMessage("secret", "Password: ");
    m_greetd->next();
    m_greetd->authMessage(
        "error", "You are required to change your password immediately (administrator enforced).");
    auto *box = waitForOpen<QMessageBox>(*m_greeter);
    QVERIFY(box);
    QTest::keyClick(box, Qt::Key_Return);
    QVERIFY(m_greetd->next().value("response").isNull());
    m_greetd->error("auth_error", "pam_acct_mgmt: NEW_AUTHTOK_REQD");
    QCOMPARE(m_greetd->next().value("type").toString(), QString("cancel_session"));
    box = waitForOpen<QMessageBox>(*m_greeter);
    QVERIFY(box);
    QVERIFY(box->text().contains("expired"));
    m_greetd->success();
    QTest::keyClick(box, Qt::Key_Return);
    QTRY_VERIFY(m_greeter->dialog()->okButton()->isEnabled());
}

void TestGreeter::infoAndOneTimeCode()
{
    logOnAs("carol", "pw");
    m_greetd->next();
    m_greetd->authMessage("secret", "Password: ");
    QCOMPARE(m_greetd->next().value("response").toString(), QString("pw"));

    m_greetd->authMessage("error", "Your account expires in 2 days.");
    auto *box = waitForOpen<QMessageBox>(*m_greeter);
    QVERIFY(box);
    QCOMPARE(box->icon(), QMessageBox::Warning);
    QTest::keyClick(box, Qt::Key_Return);
    QVERIFY(m_greetd->next().value("response").isNull());

    // A visible prompt, such as a one-time code, shows what is typed.
    m_greetd->authMessage("visible", "Verification code: ");
    auto *prompt = waitForOpen<PromptDialog>(*m_greeter);
    QVERIFY(prompt);
    QCOMPARE(prompt->field()->echoMode(), QLineEdit::Normal);
    QCOMPARE(prompt->label()->text(), QString("Verification code:"));
    QTest::keyClicks(prompt->field(), "123456");
    QTest::keyClick(prompt->field(), Qt::Key_Return);
    QCOMPARE(m_greetd->next().value("response").toString(), QString("123456"));
}

void TestGreeter::cancelPrompt()
{
    logOnAs("dave", "pw");
    m_greetd->next();
    m_greetd->authMessage("secret", "Password: ");
    m_greetd->next();
    m_greetd->authMessage("visible", "Token: ");
    auto *prompt = waitForOpen<PromptDialog>(*m_greeter);
    QVERIFY(prompt);
    QTest::keyClick(prompt, Qt::Key_Escape);
    QCOMPARE(m_greetd->next().value("type").toString(), QString("cancel_session"));
    m_greetd->success();
    QTRY_VERIFY(m_greeter->dialog()->okButton()->isEnabled());
}

void TestGreeter::rememberedUserAndSession()
{
    delete m_greeter;
    {
        QSettings state(m_state, QSettings::IniFormat);
        state.setValue("Last/User", "erin");
        state.setValue("Last/Session", "other");
    }
    m_greeter = new Greeter(m_client, testSessions(), m_state, m_power);
    m_greeter->dialog()->show();
    QCOMPARE(m_greeter->dialog()->userField()->text(), QString("erin"));
    QCOMPARE(m_greeter->currentSession(), 1);
    QCOMPARE(m_greeter->dialog()->sessionLabel()->text(), QString("Other Desktop"));
    QTRY_VERIFY(m_greeter->dialog()->passwordField()->hasFocus());
}

void TestGreeter::chooseSession()
{
    QCOMPARE(m_greeter->currentSession(), 0);
    m_greeter->dialog()->sessionButton()->click();
    auto *dialog = waitForOpen<SessionDialog>(*m_greeter);
    QVERIFY(dialog);
    QCOMPARE(dialog->list()->count(), 2);
    QCOMPARE(dialog->list()->currentRow(), 0);
    dialog->list()->setCurrentRow(1);
    dialog->accept();
    QCOMPARE(m_greeter->currentSession(), 1);
    QCOMPARE(m_greeter->dialog()->sessionLabel()->text(), QString("Other Desktop"));

    logOnAs("frank", "pw");
    m_greetd->next();
    m_greetd->success();
    const QJsonObject request = m_greetd->next();
    QCOMPARE(request.value("type").toString(), QString("start_session"));
    QCOMPARE(request.value("cmd").toVariant().toStringList(),
        QStringList({ "other-session", "--flag" }));
    QVERIFY(request.value("env").toVariant().toStringList().contains("XDG_SESSION_DESKTOP=other"));
}

void TestGreeter::cancelClears()
{
    LogonDialog *dialog = m_greeter->dialog();
    dialog->userField()->setText("gina");
    dialog->passwordField()->setText("pw");
    QTest::keyClick(dialog->passwordField(), Qt::Key_Escape);
    QVERIFY(dialog->isVisible());
    QVERIFY(dialog->userField()->text().isEmpty());
    QVERIFY(dialog->passwordField()->text().isEmpty());
    QVERIFY(m_greetd->idle());
    // Nothing happens without a user name.
    QTest::keyClick(dialog->passwordField(), Qt::Key_Return);
    QVERIFY(m_greetd->idle());
}

void TestGreeter::shutDown()
{
    m_power->available = { true, true, false };
    m_greeter->dialog()->shutDownButton()->click();
    auto *dialog = waitForOpen<GreeterShutDownDialog>(*m_greeter);
    QVERIFY(dialog);
    QVERIFY(dialog->option(PowerAction::ShutDown)->isChecked());
    QVERIFY(dialog->option(PowerAction::StandBy)->isHidden());
    dialog->option(PowerAction::Restart)->setChecked(true);
    dialog->accept();
    QCOMPARE(m_power->performed, QList<PowerAction> { PowerAction::Restart });
}

void TestGreeter::sessionFiles()
{
    const QString data = QStringLiteral(DAYTONA_TEST_DATA "/greeter");
    const QList<DesktopSession> sessions
        = findSessions({ data + "/a", data + "/b" }, QStringLiteral("de_DE.UTF-8"));
    QStringList ids;
    for (const DesktopSession &session : sessions)
        ids << session.id;
    // Sorted by name; hidden, broken and missing TryExec left out; the first
    // directory wins.
    QCOMPARE(ids, QStringList({ "alpha", "daytona", "other" }));
    QCOMPARE(sessions.at(1).name, QString("Daytona (Deutsch)"));
    QCOMPARE(sessions.at(1).desktopNames, QStringList { "Daytona" });
    QCOMPARE(sessions.at(2).name, QString("Other Desktop"));
    QCOMPARE(sessions.at(2).command,
        QStringList({ "/usr/bin/other session", "--flag", "quoted \"arg\"", "100%" }));
    QVERIFY(sessions.at(2).environment().contains("XDG_CURRENT_DESKTOP=Other:Base"));
    QCOMPARE(sessions.at(0).name, QString("Alpha"));

    const QList<DesktopSession> english = findSessions({ data + "/a" }, QStringLiteral("en_GB"));
    QCOMPARE(english.first().name, QString("Daytona"));

    QCOMPARE(defaultSession(sessions, QString()), 1);
    QCOMPARE(defaultSession(sessions, "other"), 2);
    QCOMPARE(defaultSession(sessions, "removed"), 1);
    QCOMPARE(defaultSession({ sessions.at(2) }, QString()), 0);
    QCOMPARE(defaultSession({}, QString()), -1);
}

void TestGreeter::splitExec_data()
{
    QTest::addColumn<QString>("exec");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("plain") << "startplasma-wayland" << QStringList { "startplasma-wayland" };
    QTest::newRow("args") << "  sway  --unsupported-gpu "
                          << QStringList { "sway", "--unsupported-gpu" };
    QTest::newRow("quoted") << "\"/opt/my de/start\" -x"
                            << QStringList { "/opt/my de/start", "-x" };
    QTest::newRow("field codes") << "app %f %U" << QStringList { "app" };
    QTest::newRow("unterminated") << "app \"oops" << QStringList {};
    QTest::newRow("empty") << "" << QStringList {};
}

void TestGreeter::splitExec()
{
    QFETCH(QString, exec);
    QFETCH(QStringList, expected);
    QCOMPARE(Daytona::splitExec(exec), expected);
}

void TestGreeter::keyboardDefaults()
{
    const SystemKeyboard keyboard = parseKeyboardDefaults("# KEYBOARD CONFIGURATION FILE\n"
                                                          "XKBMODEL=\"pc105\"\n"
                                                          "XKBLAYOUT=\"gb,de\"\n"
                                                          "XKBVARIANT=\",nodeadkeys\"\n"
                                                          "XKBOPTIONS='grp:alt_shift_toggle'\n"
                                                          "BACKSPACE=\"guess\"\n");
    QCOMPARE(keyboard.model, QString("pc105"));
    QCOMPARE(keyboard.layout, QString("gb,de"));
    QCOMPARE(keyboard.variant, QString(",nodeadkeys"));
    QCOMPARE(keyboard.options, QString("grp:alt_shift_toggle"));
    QCOMPARE(labwcEnvironment(keyboard),
        QString("XKB_DEFAULT_MODEL=pc105\nXKB_DEFAULT_LAYOUT=gb,de\n"
                "XKB_DEFAULT_VARIANT=,nodeadkeys\nXKB_DEFAULT_OPTIONS=grp:alt_shift_toggle\n"));
    QVERIFY(labwcEnvironment(parseKeyboardDefaults("XKBLAYOUT=us\n")) == "XKB_DEFAULT_LAYOUT=us\n");
}

QTEST_MAIN(TestGreeter)
#include "tst_greeter.moc"
