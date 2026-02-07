#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QDateTime>
#include <QJsonObject>
#include <QStringList>

#include "../Auth/McAccount.h"

namespace AMCS::Core::Api
{
class McApi : public QObject
{
    Q_OBJECT

public:
    enum class VersionSource
    {
        Official,
        BMCLApi,
        Custom
    };

    struct Skin
    {
        QString id;
        QString url;
        QString state;
    };

    struct Cape
    {
        QString id;
        QString url;
        QString state;
        QString alias;
    };

    struct Profile
    {
        QString id;
        QString name;
        QString skinUrl;
        QString capeUrl;
        QVector<Skin> skins;
        QVector<Cape> capes;
    };

    struct MCVersion
    {
        QString id;
        QString type;
        QString url;
        QDateTime time;
        QDateTime releaseTime;
    };

    friend bool operator==(const MCVersion &lhs, const MCVersion &rhs)
    {
        return lhs.id == rhs.id
            && lhs.type == rhs.type
            && lhs.url == rhs.url
            && lhs.time == rhs.time
            && lhs.releaseTime == rhs.releaseTime;
    }

    friend bool operator!=(const MCVersion &lhs, const MCVersion &rhs)
    {
        return !(lhs == rhs);
    }

    explicit McApi(Auth::McAccount *account, QObject *parent = nullptr);

    static Auth::McAccount *createOfflineAccount(const QString &name, QObject *parent = nullptr);

    static QString defaultVersionsFileName();
    static bool loadLocalVersions(const QString &filename, QVector<MCVersion> &outVersions,
                                  QString *errorString = nullptr);
    static bool saveLocalVersions(const QString &filename, const QVector<MCVersion> &versions,
                                  QString *errorString = nullptr);

    void setVersionManifestCacheSeconds(int seconds);
    int versionManifestCacheSeconds() const;

    bool fetchProfile();
    bool checkHasGame();
    bool uploadSkin(const QString &filePath, bool isSlim = false);
    bool fetchMCVersion(QVector<MCVersion> &outVersions, VersionSource source = VersionSource::Official,
                        const QString &customBaseUrl = QString());
    bool getLatestMCVersion(QVector<MCVersion> &outLatest, VersionSource source = VersionSource::Official,
                            const QString &customBaseUrl = QString());

    Profile profile() const;
    QString accountName() const;
    QString accountUuid() const;
    QString skinUrl() const;
    QString capeUrl() const;
    QVector<Skin> allSkins() const;
    QVector<Cape> allCapes() const;
    bool hasGameLicense() const;
    QString lastError() const;

    Auth::McAccount *account() const;

private:
    bool fetchVersionManifest(QJsonObject &outManifest, VersionSource source, const QString &customBaseUrl);
    bool isManifestCacheValid(VersionSource source, const QString &customBaseUrl) const;
    QString buildManifestUrl(VersionSource source, const QString &customBaseUrl) const;

    QJsonObject getJson(const QUrl &url, int *httpStatus = nullptr);
    QJsonObject postJson(const QUrl &url, const QJsonObject &payload, int *httpStatus = nullptr);
    QJsonObject postMultipart(const QUrl &url, const QMap<QString, QByteArray> &fields, int *httpStatus = nullptr);

private:
    Auth::McAccount *m_account;
    QNetworkAccessManager m_nam;
    Profile m_profile;
    bool m_hasGameLicense = false;
    QString m_lastError;

    QJsonObject m_versionManifestCache;
    QDateTime m_versionManifestCacheAt;
    QString m_versionManifestCacheUrl;
    int m_versionManifestCacheSeconds = 300;
};
} // namespace AMCS::Core::Api
