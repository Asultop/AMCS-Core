#include "McApi.h"

#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>

namespace AMCS::Core::Api
{
McApi::McApi(Auth::McAccount *account, QObject *parent)
    : QObject(parent)
    , m_account(account)
    , m_profileApiUrl(QStringLiteral("https://api.minecraftservices.com/minecraft/profile"))
    , m_entitlementsApiUrl(QStringLiteral("https://api.minecraftservices.com/entitlements/mcstore"))
    , m_skinApiUrl(QStringLiteral("https://api.minecraftservices.com/minecraft/profile/skins"))
    , m_userAgent(QStringLiteral("AMCS/1.0"))
    , m_officialManifestUrl(QStringLiteral("https://launchermeta.mojang.com/mc/game/version_manifest.json"))
    , m_bmclapiManifestUrl(QStringLiteral("https://bmclapi2.bangbang93.com/mc/game/version_manifest.json"))
    , m_versionManifestPath(QStringLiteral("/mc/game/version_manifest.json"))
    , m_versionsFileName(QStringLiteral("versions.json"))
{
}

QString McApi::defaultVersionsFileName()
{
    return QStringLiteral("versions.json");
}

bool McApi::loadLocalVersions(const QString &filename, QVector<MCVersion> &outVersions,
                              QString *errorString)
{
    outVersions.clear();

    if (filename.isEmpty()) {
        if (errorString) {
            *errorString = QStringLiteral("Version file path is empty");
        }
        return false;
    }

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to open versions file: %1").arg(filename);
        }
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorString) {
            *errorString = error.errorString();
        }
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonArray array = root.value(QStringLiteral("versions")).toArray();
    for (const auto &val : array) {
        if (!val.isObject()) {
            continue;
        }

        const QJsonObject obj = val.toObject();
        MCVersion version;
        version.id = obj.value(QStringLiteral("id")).toString();
        version.actualVersionId = obj.value(QStringLiteral("actualVersionId")).toString();
        version.type = obj.value(QStringLiteral("type")).toString();
        version.url = obj.value(QStringLiteral("url")).toString();
        version.time = QDateTime::fromString(obj.value(QStringLiteral("time")).toString(), Qt::ISODate);
        version.releaseTime = QDateTime::fromString(obj.value(QStringLiteral("releaseTime")).toString(), Qt::ISODate);
        version.javaVersion = obj.value(QStringLiteral("javaVersion")).toString();
        version.preferredJavaPath = obj.value(QStringLiteral("preferredJavaPath")).toString();

        if (!version.id.isEmpty()) {
            outVersions.append(version);
        }
    }

    return true;
}

bool McApi::saveLocalVersions(const QString &filename, const QVector<MCVersion> &versions,
                              QString *errorString)
{
    if (filename.isEmpty()) {
        if (errorString) {
            *errorString = QStringLiteral("Version file path is empty");
        }
        return false;
    }

    const QString dirPath = QFileInfo(filename).absolutePath();
    if (!QDir().mkpath(dirPath)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to create dir: %1").arg(dirPath);
        }
        return false;
    }

    QJsonArray array;
    for (const auto &version : versions) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), version.id);
        obj.insert(QStringLiteral("actualVersionId"), version.actualVersionId);
        obj.insert(QStringLiteral("type"), version.type);
        obj.insert(QStringLiteral("url"), version.url);
        obj.insert(QStringLiteral("time"), version.time.toUTC().toString(Qt::ISODate));
        obj.insert(QStringLiteral("releaseTime"), version.releaseTime.toUTC().toString(Qt::ISODate));
        obj.insert(QStringLiteral("javaVersion"), version.javaVersion);
        obj.insert(QStringLiteral("preferredJavaPath"), version.preferredJavaPath);
        array.append(obj);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("versions"), array);

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to write versions file: %1").arg(filename);
        }
        return false;
    }

    const QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QString McApi::getProfileApiUrl() const
{
    return m_profileApiUrl;
}

QString McApi::getEntitlementsApiUrl() const
{
    return m_entitlementsApiUrl;
}

QString McApi::getSkinApiUrl() const
{
    return m_skinApiUrl;
}

QString McApi::getUserAgent() const
{
    return m_userAgent;
}

QString McApi::getOfficialManifestUrl() const
{
    return m_officialManifestUrl;
}

QString McApi::getBMCLApiManifestUrl() const
{
    return m_bmclapiManifestUrl;
}

QString McApi::getVersionManifestPath() const
{
    return m_versionManifestPath;
}

Auth::McAccount *McApi::createOfflineAccount(const QString &name, QObject *parent)
{
    return Auth::McAccount::createOffline(name, parent);
}

void McApi::setVersionManifestCacheSeconds(int seconds)
{
    m_versionManifestCacheSeconds = qMax(0, seconds);
}

int McApi::versionManifestCacheSeconds() const
{
    return m_versionManifestCacheSeconds;
}

bool McApi::fetchProfile()
{
    m_lastError.clear();

    if (!m_account) {
        m_lastError = QStringLiteral("Account is null");
        return false;
    }

    const QString accessToken = m_account->mcAccessToken();
    if (accessToken.isEmpty()) {
        m_lastError = QStringLiteral("Minecraft access token empty");
        return false;
    }

    QUrl url(m_profileApiUrl);
    int status = 0;
    QJsonObject response = getJson(url, &status);

    if (status == 401) {
        m_lastError = QStringLiteral("Unauthorized: access token invalid");
        return false;
    }

    if (!response.contains("id") || !response.contains("name")) {
        m_lastError = QStringLiteral("Profile response missing id or name");
        return false;
    }

    m_profile.id = response.value("id").toString();
    m_profile.name = response.value("name").toString();

    if (m_account) {
        m_account->setUuid(m_profile.id);
        if (!m_profile.name.isEmpty()) {
            m_account->setAccountName(m_profile.name);
        }
    }

    m_profile.skinUrl.clear();
    m_profile.capeUrl.clear();
    m_profile.skins.clear();
    m_profile.capes.clear();

    const QJsonArray skins = response.value("skins").toArray();
    for (const auto &skinVal : skins) {
        const QJsonObject skinObj = skinVal.toObject();
        Skin skin;
        skin.id = skinObj.value("id").toString();
        skin.url = skinObj.value("url").toString();
        skin.state = skinObj.value("state").toString();
        m_profile.skins.append(skin);

        if (skin.state == QLatin1String("ACTIVE")) {
            m_profile.skinUrl = skin.url;
        }
    }

    const QJsonArray capes = response.value("capes").toArray();
    for (const auto &capeVal : capes) {
        const QJsonObject capeObj = capeVal.toObject();
        Cape cape;
        cape.id = capeObj.value("id").toString();
        cape.url = capeObj.value("url").toString();
        cape.state = capeObj.value("state").toString();
        cape.alias = capeObj.value("alias").toString();
        m_profile.capes.append(cape);

        if (cape.state == QLatin1String("ACTIVE")) {
            m_profile.capeUrl = cape.url;
        }
    }

    return true;
}

McApi::Profile McApi::profile() const
{
    return m_profile;
}

QString McApi::accountName() const
{
    return m_profile.name;
}

QString McApi::accountUuid() const
{
    return m_profile.id;
}

QString McApi::skinUrl() const
{
    return m_profile.skinUrl;
}

QString McApi::capeUrl() const
{
    return m_profile.capeUrl;
}

QVector<McApi::Skin> McApi::allSkins() const
{
    return m_profile.skins;
}

QVector<McApi::Cape> McApi::allCapes() const
{
    return m_profile.capes;
}

bool McApi::hasGameLicense() const
{
    return m_hasGameLicense;
}

QString McApi::lastError() const
{
    return m_lastError;
}

Auth::McAccount *McApi::account() const
{
    return m_account;
}

bool McApi::checkHasGame()
{
    m_lastError.clear();

    if (!m_account) {
        m_lastError = QStringLiteral("Account is null");
        return false;
    }

    const QString accessToken = m_account->mcAccessToken();
    if (accessToken.isEmpty()) {
        m_lastError = QStringLiteral("Minecraft access token empty");
        return false;
    }

    QUrl url(m_entitlementsApiUrl);
    int status = 0;
    QJsonObject response = getJson(url, &status);
    if (status == 401) {
        m_lastError = QStringLiteral("Unauthorized: access token invalid");
        return false;
    }

    if (response.contains("error")) {
        m_lastError = response.value("error").toString();
        return false;
    }

    m_hasGameLicense = response.value("items").toArray().count() > 0 ||
                       response.value("signature").toString() != QString();
    return true;
}

bool McApi::uploadSkin(const QString &filePath, bool isSlim)
{
    m_lastError.clear();

    if (!m_account) {
        m_lastError = QStringLiteral("Account is null");
        return false;
    }

    if (filePath.isEmpty()) {
        m_lastError = QStringLiteral("File path empty");
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("Failed to open file: ") + filePath;
        return false;
    }

    QByteArray skinData = file.readAll();
    file.close();

    QUrl url(m_skinApiUrl);

    QMap<QString, QByteArray> fields;
    fields.insert(QStringLiteral("variant"), isSlim ? "slim" : "classic");

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);

    const QString accessToken = m_account ? m_account->mcAccessToken() : QString();
    if (!accessToken.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + accessToken.toUtf8());
    }

    QString boundary = QStringLiteral("----AMCS");
    QByteArray body;
    body.append("--" + boundary.toUtf8() + "\r\n");
    body.append("Content-Disposition: form-data; name=\"variant\"\r\n\r\n");
    body.append(isSlim ? "slim" : "classic");
    body.append("\r\n--" + boundary.toUtf8() + "\r\n");
    body.append("Content-Disposition: form-data; name=\"file\"; filename=\"skin.png\"\r\n");
    body.append("Content-Type: image/png\r\n\r\n");
    body.append(skinData);
    body.append("\r\n--" + boundary.toUtf8() + "--\r\n");

    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QString("multipart/form-data; boundary=%1").arg(boundary));

    QNetworkReply *reply = m_nam.post(request, body);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->readAll();
    reply->deleteLater();

    if (status != 200) {
        m_lastError = QStringLiteral("Upload failed, status: %1").arg(status);
        return false;
    }

    return true;
}

bool McApi::fetchMCVersion(QVector<MCVersion> &outVersions, VersionSource source, const QString &customBaseUrl)
{
    m_lastError.clear();
    outVersions.clear();

    QJsonObject response;
    if (!fetchVersionManifest(response, source, customBaseUrl)) {
        return false;
    }

    if (!response.contains("versions") || !response.value("versions").isArray()) {
        m_lastError = QStringLiteral("Version manifest missing versions");
        return false;
    }

    const QJsonArray versions = response.value("versions").toArray();
    for (const auto &item : versions) {
        const QJsonObject obj = item.toObject();
        const QString id = obj.value("id").toString();
        if (id.isEmpty()) {
            continue;
        }

        MCVersion ver;
        ver.id = id;
        ver.type = obj.value("type").toString();
        ver.url = obj.value("url").toString();
        ver.time = QDateTime::fromString(obj.value("time").toString(), Qt::ISODate).toUTC();
        ver.releaseTime = QDateTime::fromString(obj.value("releaseTime").toString(), Qt::ISODate).toUTC();
        outVersions.append(ver);
    }

    if (outVersions.isEmpty()) {
        m_lastError = QStringLiteral("Version list empty");
        return false;
    }

    return true;
}

bool McApi::getLatestMCVersion(QVector<MCVersion> &outLatest, VersionSource source,
                              const QString &customBaseUrl)
{
    m_lastError.clear();
    outLatest.clear();

    QJsonObject response;
    if (!fetchVersionManifest(response, source, customBaseUrl)) {
        return false;
    }

    if (!response.contains("latest") || !response.value("latest").isObject()) {
        m_lastError = QStringLiteral("Version manifest missing latest");
        return false;
    }

    if (!response.contains("versions") || !response.value("versions").isArray()) {
        m_lastError = QStringLiteral("Version manifest missing versions");
        return false;
    }

    const QJsonObject latestObj = response.value("latest").toObject();
    const QString latestRelease = latestObj.value("release").toString();
    const QString latestSnapshot = latestObj.value("snapshot").toString();

    MCVersion releaseVersion;
    MCVersion snapshotVersion;
    bool foundRelease = false;
    bool foundSnapshot = false;

    const QJsonArray versions = response.value("versions").toArray();
    for (const auto &item : versions) {
        const QJsonObject obj = item.toObject();
        const QString id = obj.value("id").toString();
        if (id.isEmpty()) {
            continue;
        }

        if (!foundRelease && id == latestRelease) {
            releaseVersion.id = id;
            releaseVersion.type = obj.value("type").toString();
            releaseVersion.url = obj.value("url").toString();
            releaseVersion.time = QDateTime::fromString(obj.value("time").toString(), Qt::ISODate).toUTC();
            releaseVersion.releaseTime = QDateTime::fromString(obj.value("releaseTime").toString(), Qt::ISODate).toUTC();
            foundRelease = true;
        }

        if (!foundSnapshot && id == latestSnapshot) {
            snapshotVersion.id = id;
            snapshotVersion.type = obj.value("type").toString();
            snapshotVersion.url = obj.value("url").toString();
            snapshotVersion.time = QDateTime::fromString(obj.value("time").toString(), Qt::ISODate).toUTC();
            snapshotVersion.releaseTime = QDateTime::fromString(obj.value("releaseTime").toString(), Qt::ISODate).toUTC();
            foundSnapshot = true;
        }

        if (foundRelease && foundSnapshot) {
            break;
        }
    }

    if (!foundRelease || !foundSnapshot) {
        m_lastError = QStringLiteral("Latest release or snapshot not found");
        return false;
    }

    outLatest.append(releaseVersion);
    outLatest.append(snapshotVersion);
    return true;
}

bool McApi::fetchVersionManifest(QJsonObject &outManifest, VersionSource source, const QString &customBaseUrl)
{
    const QString urlString = buildManifestUrl(source, customBaseUrl);
    if (urlString.isEmpty()) {
        m_lastError = QStringLiteral("Custom source empty");
        return false;
    }

    if (isManifestCacheValid(source, customBaseUrl)) {
        outManifest = m_versionManifestCache;
        return true;
    }

    const QUrl url(urlString);
    int status = 0;
    const QJsonObject response = getJson(url, &status);
    if (response.contains("error")) {
        const QString err = response.value("error_description").toString(response.value("error").toString());
        m_lastError = err.isEmpty() ? QStringLiteral("Fetch manifest failed") : err;
        return false;
    }

    m_versionManifestCache = response;
    m_versionManifestCacheAt = QDateTime::currentDateTimeUtc();
    m_versionManifestCacheUrl = urlString;
    outManifest = response;
    return true;
}

bool McApi::isManifestCacheValid(VersionSource source, const QString &customBaseUrl) const
{
    if (m_versionManifestCache.isEmpty()) {
        return false;
    }

    if (m_versionManifestCacheSeconds <= 0) {
        return false;
    }

    const QString urlString = buildManifestUrl(source, customBaseUrl);
    if (urlString.isEmpty() || urlString != m_versionManifestCacheUrl) {
        return false;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    return m_versionManifestCacheAt.isValid() &&
           m_versionManifestCacheAt.secsTo(now) <= m_versionManifestCacheSeconds;
}

QString McApi::buildManifestUrl(VersionSource source, const QString &customBaseUrl) const
{
    if (source == VersionSource::Official) {
        return m_officialManifestUrl;
    }

    if (source == VersionSource::BMCLApi) {
        return m_bmclapiManifestUrl;
    }

    const QString trimmed = customBaseUrl.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    if (trimmed.endsWith(QLatin1String(".json"))) {
        return trimmed;
    }

    QString base = trimmed;
    if (base.endsWith('/')) {
        base.chop(1);
    }
    return base + m_versionManifestPath;
}

QJsonObject McApi::getJson(const QUrl &url, int *httpStatus)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);

    const QString accessToken = m_account ? m_account->mcAccessToken() : QString();
    if (!accessToken.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + accessToken.toUtf8());
    }

    QNetworkReply *reply = m_nam.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus) {
        *httpStatus = status;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QJsonObject errorObj;
        errorObj.insert(QStringLiteral("error"), QStringLiteral("network_error"));
        errorObj.insert(QStringLiteral("error_description"), reply->errorString());
        reply->deleteLater();
        return errorObj;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        QJsonObject errorObj;
        errorObj.insert(QStringLiteral("error"), QStringLiteral("parse_error"));
        errorObj.insert(QStringLiteral("error_description"), parseError.errorString());
        return errorObj;
    }

    if (!doc.isObject()) {
        QJsonObject errorObj;
        errorObj.insert(QStringLiteral("error"), QStringLiteral("invalid_response"));
        errorObj.insert(QStringLiteral("error_description"), QStringLiteral("Response is not a JSON object"));
        return errorObj;
    }

    return doc.object();
}

QJsonObject McApi::postJson(const QUrl &url, const QJsonObject &payload, int *httpStatus)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);

    const QString accessToken = m_account ? m_account->mcAccessToken() : QString();
    if (!accessToken.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + accessToken.toUtf8());
    }

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
        errorObj.insert(QStringLiteral("error"), QStringLiteral("invalid_response"));
        errorObj.insert(QStringLiteral("error_description"), QStringLiteral("Non-JSON response"));
        return errorObj;
    }

    return doc.object();
}

QJsonObject McApi::postMultipart(const QUrl &url, const QMap<QString, QByteArray> &fields, int *httpStatus)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);

    const QString accessToken = m_account ? m_account->mcAccessToken() : QString();
    if (!accessToken.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + accessToken.toUtf8());
    }

    QString boundary = QStringLiteral("----AMCS");
    QByteArray body;
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        body.append("--" + boundary.toUtf8() + "\r\n");
        body.append("Content-Disposition: form-data; name=\"" + it.key().toUtf8() + "\"\r\n\r\n");
        body.append(it.value());
        body.append("\r\n");
    }
    body.append("--" + boundary.toUtf8() + "--\r\n");

    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QString("multipart/form-data; boundary=%1").arg(boundary));

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
        errorObj.insert(QStringLiteral("error"), QStringLiteral("invalid_response"));
        errorObj.insert(QStringLiteral("error_description"), QStringLiteral("Non-JSON response"));
        return errorObj;
    }

    return doc.object();
}
} // namespace AMCS::Core::Api
