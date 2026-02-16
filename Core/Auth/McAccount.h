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

    QString getClientId() const;
    QString getTenantId() const;
    QString getTokenUrl() const;
    QString getScope() const;
    QString getDeviceCodeUrl() const;
    QString getXblAuthUrl() const;
    QString getXblSiteName() const;
    QString getXblRelyingParty() const;
    QString getXstsAuthUrl() const;
    QString getXstsRelyingParty() const;
    QString getMcLoginUrl() const;

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

    const QString m_clientId;
    const QString m_tenantId;
    const QString m_tokenUrl;
    const QString m_scope;
    const QString m_deviceCodeUrl;
    const QString m_xblAuthUrl;
    const QString m_xblSiteName;
    const QString m_xblRelyingParty;
    const QString m_xstsAuthUrl;
    const QString m_xstsRelyingParty;
    const QString m_mcLoginUrl;
};
} // namespace AMCS::Core::Auth
