#pragma once

#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QUrl>

namespace AMCS::Core::Auth
{
class McAccount : public QObject
{
    Q_OBJECT

public:
    enum class AccountType
    {
        Online,
        Offline
    };

    struct Tokens
    {
        QString msaAccessToken;
        QString msaRefreshToken;
        QString xblToken;
        QString xstsToken;
        QString mcAccessToken;
        QString uhs;
        QDateTime msaExpiresAt;
    };

    explicit McAccount(QObject *parent = nullptr);

    static McAccount *createOffline(const QString &name, QObject *parent = nullptr);

    bool login(int maxPollSeconds = 0, int pollIntervalSeconds = 0);
    bool refresh();

    QString lastError() const;
    Tokens tokens() const;

    QString mcAccessToken() const;
    QString msaRefreshToken() const;
    QDateTime expiresAt() const;
    QString accountName() const;
    QString uuid() const;
    bool isOffline() const;
    QString userType() const;

    void setAccountName(const QString &name);
    void setUuid(const QString &uuid);
    void setOffline(bool offline);

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &obj);

signals:
    void deviceCodeReceived(const QString &message,
                            const QString &verificationUri,
                            const QString &userCode);

private:
    bool requestDeviceCode(QJsonObject &outResponse);
    bool pollToken(const QJsonObject &deviceCodeResponse, QJsonObject &outTokenResponse,
                   int maxPollSeconds, int pollIntervalSeconds);
    bool xboxLiveAuthenticate(const QString &msaAccessToken, QJsonObject &outXblResponse);
    bool xstsAuthorize(const QString &xblToken, QJsonObject &outXstsResponse);
    bool minecraftLogin(const QString &uhs, const QString &xstsToken, QJsonObject &outMcResponse);

    QJsonObject postForm(const QUrl &url, const QMap<QString, QString> &form, int *httpStatus = nullptr);
    QJsonObject postJson(const QUrl &url, const QJsonObject &payload, int *httpStatus = nullptr);
    void waitSeconds(int seconds) const;

private:
    QNetworkAccessManager m_nam;
    Tokens m_tokens;
    QString m_lastError;
    QString m_accountName;
    QString m_uuid;
    AccountType m_type = AccountType::Online;

    const QString m_clientId = QStringLiteral("0932d3fd-f68f-4dcb-9911-0aa8c71a3c69");
    const QString m_tenantId = QStringLiteral("consumers");
};
} // namespace AMCS::Core::Auth
