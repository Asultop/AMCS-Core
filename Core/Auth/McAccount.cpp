#include "McAccount.h"

#include "../CoreSettings.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

namespace AMCS::Core::Auth
{
McAccount::McAccount(QObject *parent)
    : QObject(parent)
    , m_clientId(QStringLiteral("0932d3fd-f68f-4dcb-9911-0aa8c71a3c69"))
    , m_tenantId(QStringLiteral("consumers"))
    , m_tokenUrl(QStringLiteral("https://login.microsoftonline.com/%1/oauth2/v2.0/token"))
    , m_scope(QStringLiteral("XboxLive.signin offline_access"))
    , m_deviceCodeUrl(QStringLiteral("https://login.microsoftonline.com/%1/oauth2/v2.0/devicecode"))
    , m_xblAuthUrl(QStringLiteral("https://user.auth.xboxlive.com/user/authenticate"))
    , m_xblSiteName(QStringLiteral("user.auth.xboxlive.com"))
    , m_xblRelyingParty(QStringLiteral("http://auth.xboxlive.com"))
    , m_xstsAuthUrl(QStringLiteral("https://xsts.auth.xboxlive.com/xsts/authorize"))
    , m_xstsRelyingParty(QStringLiteral("rp://api.minecraftservices.com/"))
    , m_mcLoginUrl(QStringLiteral("https://api.minecraftservices.com/authentication/login_with_xbox"))
{
}

QString McAccount::getClientId() const
{
    return m_clientId;
}

QString McAccount::getTenantId() const
{
    return m_tenantId;
}

QString McAccount::getTokenUrl() const
{
    return m_tokenUrl;
}

QString McAccount::getScope() const
{
    return m_scope;
}

QString McAccount::getDeviceCodeUrl() const
{
    return m_deviceCodeUrl;
}

QString McAccount::getXblAuthUrl() const
{
    return m_xblAuthUrl;
}

QString McAccount::getXblSiteName() const
{
    return m_xblSiteName;
}

QString McAccount::getXblRelyingParty() const
{
    return m_xblRelyingParty;
}

QString McAccount::getXstsAuthUrl() const
{
    return m_xstsAuthUrl;
}

QString McAccount::getXstsRelyingParty() const
{
    return m_xstsRelyingParty;
}

QString McAccount::getMcLoginUrl() const
{
    return m_mcLoginUrl;
}

McAccount *McAccount::createOffline(const QString &name, QObject *parent)
{
    auto *account = new McAccount(parent);
    account->m_type = AccountType::Offline;
    account->m_accountName = name;
    account->m_uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    account->m_tokens = Tokens();
    return account;
}

bool McAccount::login(int maxPollSeconds, int pollIntervalSeconds)
{
    m_lastError.clear();

    if (m_type == AccountType::Offline) {
        m_lastError = QStringLiteral("Offline account cannot login");
        return false;
    }

    QJsonObject deviceCodeResponse;
    if (!requestDeviceCode(deviceCodeResponse)) {
        return false;
    }

    emit deviceCodeReceived(deviceCodeResponse.value("message").toString(),
                            deviceCodeResponse.value("verification_uri").toString(),
                            deviceCodeResponse.value("user_code").toString());

    QJsonObject tokenResponse;
    if (!pollToken(deviceCodeResponse, tokenResponse, maxPollSeconds, pollIntervalSeconds)) {
        return false;
    }

    m_tokens.msaAccessToken = tokenResponse.value("access_token").toString();
    m_tokens.msaRefreshToken = tokenResponse.value("refresh_token").toString();
    const int expiresIn = tokenResponse.value("expires_in").toInt(0);
    m_tokens.msaExpiresAt = QDateTime::currentDateTimeUtc().addSecs(expiresIn);

    QJsonObject xblResponse;
    if (!xboxLiveAuthenticate(m_tokens.msaAccessToken, xblResponse)) {
        return false;
    }

    m_tokens.xblToken = xblResponse.value("Token").toString();
    const QJsonArray xuiArray = xblResponse.value("DisplayClaims").toObject().value("xui").toArray();
    if (xuiArray.isEmpty()) {
        m_lastError = QStringLiteral("XBL response missing xui");
        return false;
    }
    m_tokens.uhs = xuiArray.first().toObject().value("uhs").toString();

    QJsonObject xstsResponse;
    if (!xstsAuthorize(m_tokens.xblToken, xstsResponse)) {
        return false;
    }

    m_tokens.xstsToken = xstsResponse.value("Token").toString();
    const QJsonArray xstsXuiArray = xstsResponse.value("DisplayClaims").toObject().value("xui").toArray();
    if (!xstsXuiArray.isEmpty()) {
        m_tokens.uhs = xstsXuiArray.first().toObject().value("uhs").toString();
    }

    QJsonObject mcResponse;
    if (!minecraftLogin(m_tokens.uhs, m_tokens.xstsToken, mcResponse)) {
        return false;
    }

    m_tokens.mcAccessToken = mcResponse.value("access_token").toString();
    if (m_tokens.mcAccessToken.isEmpty()) {
        m_lastError = QStringLiteral("Minecraft access token missing");
        return false;
    }

    return true;
}

bool McAccount::refresh()
{
    m_lastError.clear();

    if (m_type == AccountType::Offline) {
        m_lastError = QStringLiteral("Offline account cannot refresh");
        return false;
    }

    if (m_tokens.msaRefreshToken.isEmpty()) {
        m_lastError = QStringLiteral("Refresh token empty");
        return false;
    }

    const QUrl url(m_tokenUrl.arg(m_tenantId));
    QMap<QString, QString> form;
    form.insert(QStringLiteral("client_id"), m_clientId);
    form.insert(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.insert(QStringLiteral("refresh_token"), m_tokens.msaRefreshToken);
    form.insert(QStringLiteral("scope"), m_scope);

    QJsonObject tokenResponse = postForm(url, form);
    if (tokenResponse.contains("error")) {
        m_lastError = tokenResponse.value("error_description").toString(tokenResponse.value("error").toString());
        return false;
    }

    m_tokens.msaAccessToken = tokenResponse.value("access_token").toString();
    const QString newRefresh = tokenResponse.value("refresh_token").toString();
    if (!newRefresh.isEmpty()) {
        m_tokens.msaRefreshToken = newRefresh;
    }

    const int expiresIn = tokenResponse.value("expires_in").toInt(0);
    m_tokens.msaExpiresAt = QDateTime::currentDateTimeUtc().addSecs(expiresIn);

    QJsonObject xblResponse;
    if (!xboxLiveAuthenticate(m_tokens.msaAccessToken, xblResponse)) {
        return false;
    }

    m_tokens.xblToken = xblResponse.value("Token").toString();
    const QJsonArray xuiArray = xblResponse.value("DisplayClaims").toObject().value("xui").toArray();
    if (xuiArray.isEmpty()) {
        m_lastError = QStringLiteral("XBL response missing xui");
        return false;
    }
    m_tokens.uhs = xuiArray.first().toObject().value("uhs").toString();

    QJsonObject xstsResponse;
    if (!xstsAuthorize(m_tokens.xblToken, xstsResponse)) {
        return false;
    }

    m_tokens.xstsToken = xstsResponse.value("Token").toString();

    QJsonObject mcResponse;
    if (!minecraftLogin(m_tokens.uhs, m_tokens.xstsToken, mcResponse)) {
        return false;
    }

    m_tokens.mcAccessToken = mcResponse.value("access_token").toString();
    if (m_tokens.mcAccessToken.isEmpty()) {
        m_lastError = QStringLiteral("Minecraft access token missing");
        return false;
    }

    return true;
}

QString McAccount::lastError() const
{
    return m_lastError;
}

McAccount::Tokens McAccount::tokens() const
{
    return m_tokens;
}

QString McAccount::mcAccessToken() const
{
    return m_tokens.mcAccessToken;
}

QString McAccount::msaRefreshToken() const
{
    return m_tokens.msaRefreshToken;
}

QDateTime McAccount::expiresAt() const
{
    return m_tokens.msaExpiresAt;
}

QString McAccount::accountName() const
{
    return m_accountName;
}

QString McAccount::uuid() const
{
    return m_uuid;
}

bool McAccount::isOffline() const
{
    return m_type == AccountType::Offline;
}

QString McAccount::userType() const
{
    return m_type == AccountType::Offline ? QStringLiteral("legacy") : QStringLiteral("msa");
}

void McAccount::setAccountName(const QString &name)
{
    m_accountName = name;
}

void McAccount::setUuid(const QString &uuid)
{
    m_uuid = uuid;
}

void McAccount::setOffline(bool offline)
{
    m_type = offline ? AccountType::Offline : AccountType::Online;
}

QJsonObject McAccount::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("msaAccessToken"), m_tokens.msaAccessToken);
    obj.insert(QStringLiteral("msaRefreshToken"), m_tokens.msaRefreshToken);
    obj.insert(QStringLiteral("xblToken"), m_tokens.xblToken);
    obj.insert(QStringLiteral("xstsToken"), m_tokens.xstsToken);
    obj.insert(QStringLiteral("mcAccessToken"), m_tokens.mcAccessToken);
    obj.insert(QStringLiteral("uhs"), m_tokens.uhs);
    obj.insert(QStringLiteral("msaExpiresAt"), m_tokens.msaExpiresAt.toUTC().toString(Qt::ISODate));
    obj.insert(QStringLiteral("accountName"), m_accountName);
    obj.insert(QStringLiteral("uuid"), m_uuid);
    obj.insert(QStringLiteral("accountType"), m_type == AccountType::Offline ? QStringLiteral("offline") : QStringLiteral("online"));
    return obj;
}

bool McAccount::fromJson(const QJsonObject &obj)
{
    if (obj.isEmpty()) {
        return false;
    }

    m_tokens.msaAccessToken = obj.value(QStringLiteral("msaAccessToken")).toString();
    m_tokens.msaRefreshToken = obj.value(QStringLiteral("msaRefreshToken")).toString();
    m_tokens.xblToken = obj.value(QStringLiteral("xblToken")).toString();
    m_tokens.xstsToken = obj.value(QStringLiteral("xstsToken")).toString();
    m_tokens.mcAccessToken = obj.value(QStringLiteral("mcAccessToken")).toString();
    m_tokens.uhs = obj.value(QStringLiteral("uhs")).toString();

    const QString expiresAt = obj.value(QStringLiteral("msaExpiresAt")).toString();
    if (!expiresAt.isEmpty()) {
        m_tokens.msaExpiresAt = QDateTime::fromString(expiresAt, Qt::ISODate).toUTC();
    }

    m_accountName = obj.value(QStringLiteral("accountName")).toString();
    m_uuid = obj.value(QStringLiteral("uuid")).toString();

    const QString type = obj.value(QStringLiteral("accountType")).toString();
    m_type = (type == QLatin1String("offline")) ? AccountType::Offline : AccountType::Online;

    return m_type == AccountType::Offline || !m_tokens.msaRefreshToken.isEmpty();
}

bool McAccount::requestDeviceCode(QJsonObject &outResponse)
{
    const QUrl url(m_deviceCodeUrl.arg(m_tenantId));

    QMap<QString, QString> form;
    form.insert(QStringLiteral("client_id"), m_clientId);
    form.insert(QStringLiteral("scope"), m_scope);

    outResponse = postForm(url, form);
    if (outResponse.contains("error")) {
        m_lastError = outResponse.value("error_description").toString(outResponse.value("error").toString());
        return false;
    }

    if (!outResponse.contains("device_code")) {
        m_lastError = QStringLiteral("Device code missing");
        return false;
    }

    return true;
}

bool McAccount::pollToken(const QJsonObject &deviceCodeResponse, QJsonObject &outTokenResponse,
                          int maxPollSeconds, int pollIntervalSeconds)
{
    const QString deviceCode = deviceCodeResponse.value("device_code").toString();
    const int interval = deviceCodeResponse.value("interval").toInt(5);
    const int expiresIn = deviceCodeResponse.value("expires_in").toInt(900);
    const int effectiveMaxSeconds = (maxPollSeconds > 0 && maxPollSeconds < expiresIn)
        ? maxPollSeconds
        : expiresIn;
    const QDateTime expiresAt = QDateTime::currentDateTimeUtc().addSecs(effectiveMaxSeconds);

    const QUrl url(m_tokenUrl.arg(m_tenantId));

    int currentInterval = interval;
    if (pollIntervalSeconds > 0) {
        currentInterval = qMax(interval, pollIntervalSeconds);
    }
    while (QDateTime::currentDateTimeUtc() < expiresAt) {
        waitSeconds(currentInterval);

        QMap<QString, QString> form;
        form.insert(QStringLiteral("client_id"), m_clientId);
        form.insert(QStringLiteral("grant_type"), QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));
        form.insert(QStringLiteral("device_code"), deviceCode);

        outTokenResponse = postForm(url, form);
        if (!outTokenResponse.contains("error")) {
            return true;
        }

        const QString errorCode = outTokenResponse.value("error").toString();
        if (errorCode == QLatin1String("authorization_pending")) {
            continue;
        }
        if (errorCode == QLatin1String("slow_down")) {
            currentInterval += 2;
            continue;
        }

        m_lastError = outTokenResponse.value("error_description").toString(errorCode);
        return false;
    }

    m_lastError = QStringLiteral("Device code expired");
    return false;
}

bool McAccount::xboxLiveAuthenticate(const QString &msaAccessToken, QJsonObject &outXblResponse)
{
    const QUrl url(m_xblAuthUrl);

    QJsonObject props;
    props.insert(QStringLiteral("AuthMethod"), QStringLiteral("RPS"));
    props.insert(QStringLiteral("SiteName"), m_xblSiteName);
    props.insert(QStringLiteral("RpsTicket"), QStringLiteral("d=%1").arg(msaAccessToken));

    QJsonObject payload;
    payload.insert(QStringLiteral("Properties"), props);
    payload.insert(QStringLiteral("RelyingParty"), m_xblRelyingParty);
    payload.insert(QStringLiteral("TokenType"), QStringLiteral("JWT"));

    outXblResponse = postJson(url, payload);
    if (outXblResponse.contains("error")) {
        m_lastError = QStringLiteral("XBL auth failed");
        return false;
    }

    if (!outXblResponse.contains("Token")) {
        m_lastError = QStringLiteral("XBL token missing");
        return false;
    }

    return true;
}

bool McAccount::xstsAuthorize(const QString &xblToken, QJsonObject &outXstsResponse)
{
    const QUrl url(m_xstsAuthUrl);

    QJsonObject props;
    props.insert(QStringLiteral("SandboxId"), QStringLiteral("RETAIL"));
    props.insert(QStringLiteral("UserTokens"), QJsonArray { xblToken });

    QJsonObject payload;
    payload.insert(QStringLiteral("Properties"), props);
    payload.insert(QStringLiteral("RelyingParty"), m_xstsRelyingParty);
    payload.insert(QStringLiteral("TokenType"), QStringLiteral("JWT"));

    outXstsResponse = postJson(url, payload);
    if (outXstsResponse.contains("error")) {
        m_lastError = QStringLiteral("XSTS auth failed");
        return false;
    }

    if (!outXstsResponse.contains("Token")) {
        m_lastError = QStringLiteral("XSTS token missing");
        return false;
    }

    return true;
}

bool McAccount::minecraftLogin(const QString &uhs, const QString &xstsToken, QJsonObject &outMcResponse)
{
    const QUrl url(m_mcLoginUrl);

    QJsonObject payload;
    payload.insert(QStringLiteral("identityToken"), QStringLiteral("XBL3.0 x=%1;%2").arg(uhs, xstsToken));

    outMcResponse = postJson(url, payload);
    if (outMcResponse.contains("error")) {
        m_lastError = QStringLiteral("Minecraft login failed");
        return false;
    }

    return true;
}

QJsonObject McAccount::postForm(const QUrl &url, const QMap<QString, QString> &form, int *httpStatus)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery query;
    for (auto it = form.cbegin(); it != form.cend(); ++it) {
        query.addQueryItem(it.key(), it.value());
    }

    QNetworkReply *reply = m_nam.post(request, query.query(QUrl::FullyEncoded).toUtf8());

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (httpStatus) {
        *httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QJsonObject errorObj;
        errorObj.insert(QStringLiteral("error"), QStringLiteral("parse_error"));
        errorObj.insert(QStringLiteral("error_description"), parseError.errorString());
        return errorObj;
    }

    return doc.object();
}

QJsonObject McAccount::postJson(const QUrl &url, const QJsonObject &payload, int *httpStatus)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_nam.post(request, body);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (httpStatus) {
        *httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QJsonObject errorObj;
        errorObj.insert(QStringLiteral("error"), QStringLiteral("parse_error"));
        errorObj.insert(QStringLiteral("error_description"), parseError.errorString());
        return errorObj;
    }

    return doc.object();
}

void McAccount::waitSeconds(int seconds) const
{
    QEventLoop loop;
    QTimer::singleShot(seconds * 1000, &loop, &QEventLoop::quit);
    loop.exec();
}
} // namespace AMCS::Core::Auth
