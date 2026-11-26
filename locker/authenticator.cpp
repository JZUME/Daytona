// SPDX-License-Identifier: GPL-3.0-or-later

#include "authenticator.h"

#include <QFileInfo>
#include <QLoggingCategory>
#include <cstdlib>
#include <cstring>
#include <security/pam_appl.h>

Q_LOGGING_CATEGORY(lcAuth, "daytona.lock.auth", QtInfoMsg)

namespace Daytona {

namespace {

    void wipe(QByteArray &secret)
    {
        // volatile, so the compiler does not drop the writes.
        volatile char *data = secret.data();
        for (qsizetype i = 0; i < secret.size(); ++i)
            data[i] = 0;
        secret.clear();
    }

    // Answers every prompt PAM asks to be typed with the password; shows
    // nothing else (the dialog has its own message for a wrong password).
    int conversation(int count, const pam_message **messages, pam_response **responses, void *data)
    {
        if (count <= 0)
            return PAM_CONV_ERR;
        const auto *password = static_cast<const QByteArray *>(data);
        auto *replies = static_cast<pam_response *>(std::calloc(count, sizeof(pam_response)));
        if (!replies)
            return PAM_BUF_ERR;
        for (int i = 0; i < count; ++i) {
            switch (messages[i]->msg_style) {
            case PAM_PROMPT_ECHO_OFF:
            case PAM_PROMPT_ECHO_ON:
                replies[i].resp = ::strdup(password->constData());
                if (!replies[i].resp) {
                    for (int j = 0; j < i; ++j)
                        std::free(replies[j].resp);
                    std::free(replies);
                    return PAM_BUF_ERR;
                }
                break;
            case PAM_ERROR_MSG:
            case PAM_TEXT_INFO:
                qCInfo(lcAuth) << "PAM:" << messages[i]->msg;
                break;
            default:
                break;
            }
        }
        *responses = replies;
        return PAM_SUCCESS;
    }

} // namespace

QByteArray pamService()
{
    for (const char *dir : { "/etc/pam.d/", "/usr/lib/pam.d/" }) {
        if (QFileInfo::exists(QString::fromLatin1(dir) + QStringLiteral("daytona")))
            return QByteArrayLiteral("daytona");
    }
    qCWarning(lcAuth) << "/etc/pam.d/daytona is not installed: checking passwords as \"login\"";
    return QByteArrayLiteral("login");
}

bool authenticate(const QByteArray &service, const QByteArray &user, QByteArray password)
{
    const pam_conv conv { conversation, &password };
    pam_handle_t *handle = nullptr;
    int result = ::pam_start(service.constData(), user.constData(), &conv, &handle);
    if (result != PAM_SUCCESS) {
        qCWarning(lcAuth) << "Cannot start PAM:" << ::pam_strerror(handle, result);
        wipe(password);
        return false;
    }
    result = ::pam_authenticate(handle, 0);
    wipe(password);
    if (result == PAM_SUCCESS) {
        // Renews credentials such as Kerberos tickets, as unlocking does.
        ::pam_setcred(handle, PAM_REFRESH_CRED);
    } else {
        qCInfo(lcAuth) << "Unlock refused:" << ::pam_strerror(handle, result);
    }
    ::pam_end(handle, result);
    return result == PAM_SUCCESS;
}

} // namespace Daytona
