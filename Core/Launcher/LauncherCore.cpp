#include "LauncherCore.h"

#include "../CoreSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSysInfo>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <QProcess>
#include <QMap>

#if defined(Q_OS_WIN)
#include <QtCore/private/qzipreader_p.h>
#endif

#include "../Download/AsulMultiDownloader.h"

namespace AMCS::Core::Launcher
{
namespace
{
static QString currentOsName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MAC)
    return QStringLiteral("osx");
#else
    return QStringLiteral("linux");
#endif
}

static QString currentArchToken()
{
    const QString arch = QSysInfo::currentCpuArchitecture().toLower();
    if (arch.contains("arm64") || arch.contains("aarch64")) {
        return QStringLiteral("arm64");
    }
    if (arch.contains("64")) {
        return QStringLiteral("64");
    }
    return QStringLiteral("32");
}

static bool ruleAllows(const QJsonArray &rules)
{
    if (rules.isEmpty()) {
        return true;
    }

    const QString osName = currentOsName();
    const QString archToken = currentArchToken();
    bool allowed = false;

    for (const auto &ruleVal : rules) {
        const QJsonObject rule = ruleVal.toObject();
        const QString action = rule.value(QStringLiteral("action")).toString(QStringLiteral("allow"));
        bool matches = true;

        if (rule.contains(QStringLiteral("os"))) {
            const QJsonObject osObj = rule.value(QStringLiteral("os")).toObject();
            const QString name = osObj.value(QStringLiteral("name")).toString();
            const QString arch = osObj.value(QStringLiteral("arch")).toString();
            if (!name.isEmpty()) {
                matches = (name == osName);
            }
            if (matches && !arch.isEmpty()) {
                matches = arch.contains(archToken);
            }
        }

        if (matches && rule.contains(QStringLiteral("features"))) {
            const QJsonObject features = rule.value(QStringLiteral("features")).toObject();
            for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
                const bool required = it.value().toBool();
                const bool supported = false;
                if (required != supported) {
                    matches = false;
                    break;
                }
            }
        }

        if (matches) {
            allowed = (action == QLatin1String("allow"));
        }
    }

    return allowed;
}

static QString resolveNativeClassifier(const QJsonObject &libraryObj)
{
    if (!libraryObj.contains(QStringLiteral("natives"))) {
        return QString();
    }

    const QJsonObject natives = libraryObj.value(QStringLiteral("natives")).toObject();
    const QString osName = currentOsName();
    QString key = natives.value(osName).toString();
    if (key.isEmpty()) {
        return QString();
    }

    key.replace(QStringLiteral("${arch}"), currentArchToken());
    return key;
}

static QString libraryClassifierFromName(const QString &name)
{
    const QStringList parts = name.split(QLatin1Char(':'));
    if (parts.size() >= 4) {
        return parts.at(3);
    }
    return QString();
}

static bool classifierMatchesOsAndArch(const QString &classifier)
{
    if (classifier.isEmpty()) {
        return true;
    }

    const QString lower = classifier.toLower();
    const QString osName = currentOsName();
    bool osMatch = true;

    if (lower.contains(QStringLiteral("windows"))) {
        osMatch = (osName == QLatin1String("windows"));
    } else if (lower.contains(QStringLiteral("osx")) || lower.contains(QStringLiteral("macos"))) {
        osMatch = (osName == QLatin1String("osx"));
    } else if (lower.contains(QStringLiteral("linux"))) {
        osMatch = (osName == QLatin1String("linux"));
    }

    if (!osMatch) {
        return false;
    }

    const QString archToken = currentArchToken();
    if (lower.contains(QStringLiteral("arm64")) || lower.contains(QStringLiteral("aarch_64"))
        || lower.contains(QStringLiteral("aarch64"))) {
        return archToken == QLatin1String("arm64");
    }
    if (lower.contains(QStringLiteral("x86_64")) || lower.contains(QStringLiteral("amd64"))
        || lower.contains(QStringLiteral("64"))) {
        return archToken == QLatin1String("64");
    }
    if (lower.contains(QStringLiteral("x86")) || lower.contains(QStringLiteral("32"))) {
        return archToken == QLatin1String("32");
    }

    return true;
}

static bool isNewFormatNativeArtifact(const QString &artifactPath, const QString &classifier)
{
    if (classifier.isEmpty()) {
        return false;
    }

    const QString lowerClassifier = classifier.toLower();
    const QString fileName = QFileInfo(artifactPath).fileName().toLower();
    if (lowerClassifier.contains(QStringLiteral("native")) || fileName.contains(QStringLiteral("-native"))
        || fileName.contains(QStringLiteral("-natives"))) {
        return true;
    }

    return false;
}

static QUrl applyMirrorUrl(const QUrl &url, Api::McApi::VersionSource source)
{
    if (source != Api::McApi::VersionSource::BMCLApi) {
        return url;
    }

    QString replaced = url.toString();
    replaced.replace("https://resources.download.minecraft.net", "https://bmclapi2.bangbang93.com/assets");
    replaced.replace("http://resources.download.minecraft.net", "https://bmclapi2.bangbang93.com/assets");
    replaced.replace("https://libraries.minecraft.net", "https://bmclapi2.bangbang93.com/maven");
    replaced.replace("http://libraries.minecraft.net", "https://bmclapi2.bangbang93.com/maven");
    replaced.replace("https://launchermeta.mojang.com/", "https://bmclapi2.bangbang93.com/");
    replaced.replace("http://launchermeta.mojang.com/", "https://bmclapi2.bangbang93.com/");
    replaced.replace("https://launcher.mojang.com/", "https://bmclapi2.bangbang93.com/");
    replaced.replace("http://launcher.mojang.com/", "https://bmclapi2.bangbang93.com/");
    return QUrl(replaced);
}

static QUrl assetUrlFromHash(const QString &hash)
{
    if (hash.length() < 2) {
        return QUrl();
    }

    const QString prefix = hash.left(2);
    const QString url = QStringLiteral("https://resources.download.minecraft.net/%1/%2").arg(prefix, hash);
    return QUrl(url);
}

static QUrl buildBmclapiVersionUrl(const QString &versionId, const QString &category)
{
    if (versionId.isEmpty() || category.isEmpty()) {
        return QUrl();
    }

    return QUrl(QStringLiteral("https://bmclapi2.bangbang93.com/version/%1/%2")
                    .arg(versionId, category));
}

static bool loadJsonFile(const QString &filePath, QJsonObject *outJson, QString *errorString)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to open JSON: %1").arg(filePath);
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorString) {
            *errorString = parseError.errorString();
        }
        return false;
    }

    if (outJson) {
        *outJson = doc.object();
    }
    return true;
}

static bool downloadFileSync(const QUrl &url, const QString &savePath, QString *errorString)
{
    QFileInfo fileInfo(savePath);
    if (fileInfo.exists() && fileInfo.size() > 0) {
        return true;
    }

    if (!url.isValid() || url.isEmpty()) {
        if (errorString) {
            *errorString = QStringLiteral("Download URL is empty");
        }
        return false;
    }

    QDir dir(fileInfo.absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to create dir: %1").arg(fileInfo.absolutePath());
        }
        return false;
    }

    AMCS::Core::Download::AsulMultiDownloader downloader;
    downloader.setMaxConcurrentDownloads(4);
    downloader.setMaxConnectionsPerHost(4);
    downloader.setLargeFileThreshold(512 * 1024);
    downloader.setSegmentCountForLargeFile(2);

    bool failed = false;
    QString failure;
    QEventLoop loop;

    QObject::connect(&downloader, &AMCS::Core::Download::AsulMultiDownloader::downloadFailed,
                     [&](const QString &, const QString &error) {
                         failed = true;
                         failure = error;
                     });

    QObject::connect(&downloader, &AMCS::Core::Download::AsulMultiDownloader::allDownloadsFinished, &loop, &QEventLoop::quit);

    downloader.addDownload(url, savePath, 10);
    loop.exec();

    if (failed) {
        if (errorString) {
            *errorString = failure;
        }
        return false;
    }

    return QFileInfo(savePath).exists();
}

static bool shouldSkipZipEntry(const QString &path)
{
    const QString clean = QDir::cleanPath(path).replace('\\', '/');
    if (clean.startsWith(QStringLiteral("META-INF/"), Qt::CaseInsensitive)) {
        return true;
    }
    if (clean.startsWith(QStringLiteral("../")) || clean.startsWith(QStringLiteral("..\\"))) {
        return true;
    }
    if (clean.startsWith('/') || clean.contains(":/")) {
        return true;
    }
    return false;
}

static bool extractZipToDir(const QString &zipPath, const QString &destDir, QString *errorString)
{
#if defined(Q_OS_WIN)
    QZipReader reader(zipPath);
    if (!reader.exists()) {
        if (errorString) {
            *errorString = QStringLiteral("Zip not found: %1").arg(zipPath);
        }
        return false;
    }

    const QString baseDir = QDir(destDir).absolutePath();
    int extractedCount = 0;
    const auto entries = reader.fileInfoList();
    qInfo().noquote() << "[natives] zip entries:" << entries.size() << zipPath;
    for (const auto &entry : entries) {
        if (entry.isDir) {
            continue;
        }
        if (entry.isSymLink) {
            continue;
        }
        if (shouldSkipZipEntry(entry.filePath)) {
            continue;
        }

        const QString fileName = QFileInfo(entry.filePath).fileName();
        if (fileName.isEmpty()) {
            continue;
        }

        const QString outPath = QDir(baseDir).absoluteFilePath(fileName);

        QFile outFile(outPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (errorString) {
                *errorString = QStringLiteral("Failed to write: %1").arg(outPath);
            }
            return false;
        }
        outFile.write(reader.fileData(entry.filePath));
        outFile.close();
        extractedCount += 1;
    }

    reader.close();
    if (extractedCount == 0) {
        if (errorString) {
            *errorString = QStringLiteral("No native files extracted from: %1").arg(zipPath);
        }
        return false;
    }
    return true;
#else
    if (zipPath.isEmpty() || destDir.isEmpty()) {
        if (errorString) {
            *errorString = QStringLiteral("Zip path or destination is empty");
        }
        return false;
    }

    if (!QDir().mkpath(destDir)) {
        if (errorString) {
            *errorString = QStringLiteral("Failed to create dir: %1").arg(destDir);
        }
        return false;
    }

    QProcess process;
    const QStringList args = {
        QStringLiteral("-o"),
        QStringLiteral("-j"),
        zipPath,
        QStringLiteral("-d"),
        destDir
    };
    process.start(QStringLiteral("unzip"), args);
    if (!process.waitForFinished(15000)) {
        process.kill();
        process.waitForFinished(1000);
        if (errorString) {
            *errorString = QStringLiteral("unzip timeout: %1").arg(zipPath);
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorString) {
            const QString err = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *errorString = err.isEmpty()
                ? QStringLiteral("unzip failed: %1").arg(zipPath)
                : err;
        }
        return false;
    }

    return true;
#endif
}

static QJsonObject mergeVersionJson(const QJsonObject &parent, const QJsonObject &child)
{
    QJsonObject merged = parent;
    for (auto it = child.constBegin(); it != child.constEnd(); ++it) {
        const QString key = it.key();
        if (key == QLatin1String("libraries") && it->isArray() && parent.value(key).isArray()) {
            QJsonArray mergedLibs = parent.value(key).toArray();
            const QJsonArray childLibs = it->toArray();
            for (const auto &val : childLibs) {
                mergedLibs.append(val);
            }
            merged.insert(key, mergedLibs);
            continue;
        }

        if (key == QLatin1String("arguments") && it->isObject() && parent.value(key).isObject()) {
            QJsonObject mergedArgs = parent.value(key).toObject();
            const QJsonObject childArgs = it->toObject();
            if (childArgs.contains(QStringLiteral("jvm"))) {
                mergedArgs.insert(QStringLiteral("jvm"), childArgs.value(QStringLiteral("jvm")));
            }
            if (childArgs.contains(QStringLiteral("game"))) {
                mergedArgs.insert(QStringLiteral("game"), childArgs.value(QStringLiteral("game")));
            }
            merged.insert(key, mergedArgs);
            continue;
        }

        merged.insert(key, *it);
    }
    return merged;
}

static bool loadMergedVersionJson(const QString &versionsDir, const QString &versionId, QJsonObject *outJson, QString *error)
{
    const QString versionDir = QDir(versionsDir).absoluteFilePath(versionId);
    const QString versionJsonPath = QDir(versionDir).absoluteFilePath(versionId + QStringLiteral(".json"));

    QJsonObject current;
    if (!loadJsonFile(versionJsonPath, &current, error)) {
        return false;
    }

    const QString inheritsFrom = current.value(QStringLiteral("inheritsFrom")).toString();
    if (inheritsFrom.isEmpty()) {
        *outJson = current;
        return true;
    }

    QJsonObject parent;
    if (!loadMergedVersionJson(versionsDir, inheritsFrom, &parent, error)) {
        return false;
    }

    *outJson = mergeVersionJson(parent, current);
    return true;
}

static QStringList splitArgs(const QString &args)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return QProcess::splitCommand(args);
#else
    return args.split(' ', Qt::SkipEmptyParts);
#endif
}

static QString replaceTokens(const QString &input, const QMap<QString, QString> &vars)
{
    QString out = input;
    for (auto it = vars.cbegin(); it != vars.cend(); ++it) {
        out.replace(QStringLiteral("${%1}").arg(it.key()), it.value());
    }
    return out;
}

static QStringList buildArgsFromJsonArray(const QJsonArray &arr, const QMap<QString, QString> &vars)
{
    QStringList args;
    for (const auto &val : arr) {
        if (val.isString()) {
            args.append(replaceTokens(val.toString(), vars));
            continue;
        }
        if (!val.isObject()) {
            continue;
        }

        const QJsonObject obj = val.toObject();
        const QJsonArray rules = obj.value(QStringLiteral("rules")).toArray();
        if (!rules.isEmpty() && !ruleAllows(rules)) {
            continue;
        }

        const QJsonValue value = obj.value(QStringLiteral("value"));
        if (value.isString()) {
            args.append(replaceTokens(value.toString(), vars));
        } else if (value.isArray()) {
            const QJsonArray values = value.toArray();
            for (const auto &v : values) {
                if (v.isString()) {
                    args.append(replaceTokens(v.toString(), vars));
                }
            }
        }
    }
    return args;
}

static bool hasJvmArg(const QStringList &args, const QString &prefix)
{
    for (const auto &arg : args) {
        if (arg.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

static QString classpathSeparator()
{
#if defined(Q_OS_WIN)
    return QStringLiteral(";");
#else
    return QStringLiteral(":");
#endif
}
} // namespace

LauncherCore::LauncherCore(QObject *parent)
    : QObject(parent)
{
}

bool LauncherCore::installMCVersion(const Api::McApi::MCVersion &version,
                                    const QString &dest,
                                    Api::McApi::VersionSource source)
{
    m_lastError.clear();

    if (version.id.isEmpty() || version.url.isEmpty()) {
        m_lastError = QStringLiteral("MCVersion id or url is empty");
        return false;
    }

    const QString baseDir = QDir(dest).absolutePath();
    const QString versionsDir = QDir(baseDir).absoluteFilePath(QStringLiteral("versions"));
    const QString librariesDir = QDir(baseDir).absoluteFilePath(QStringLiteral("libraries"));
    const QString assetsDir = QDir(baseDir).absoluteFilePath(QStringLiteral("assets"));
    const QString indexesDir = QDir(assetsDir).absoluteFilePath(QStringLiteral("indexes"));
    const QString objectsDir = QDir(assetsDir).absoluteFilePath(QStringLiteral("objects"));

    if (!QDir().mkpath(versionsDir) || !QDir().mkpath(librariesDir)
        || !QDir().mkpath(indexesDir) || !QDir().mkpath(objectsDir)) {
        m_lastError = QStringLiteral("Failed to create base directories");
        return false;
    }

    const QString versionDir = QDir(versionsDir).absoluteFilePath(version.id);
    const QString versionJsonPath = QDir(versionDir).absoluteFilePath(version.id + QStringLiteral(".json"));

    QUrl versionJsonUrl;
    if (source == Api::McApi::VersionSource::BMCLApi) {
        versionJsonUrl = buildBmclapiVersionUrl(version.id, QStringLiteral("json"));
    } else {
        versionJsonUrl = applyMirrorUrl(QUrl(version.url), source);
    }
    if (!downloadFileSync(versionJsonUrl, versionJsonPath, &m_lastError)) {
        return false;
    }

    QJsonObject versionJson;
    if (!loadJsonFile(versionJsonPath, &versionJson, &m_lastError)) {
        return false;
    }

    const QJsonObject assetIndexObj = versionJson.value(QStringLiteral("assetIndex")).toObject();
    const QString assetIndexId = assetIndexObj.value(QStringLiteral("id")).toString();
    const QString assetIndexUrl = assetIndexObj.value(QStringLiteral("url")).toString();
    if (assetIndexId.isEmpty() || assetIndexUrl.isEmpty()) {
        m_lastError = QStringLiteral("version.json missing assetIndex");
        return false;
    }

    const QString assetIndexPath = QDir(indexesDir).absoluteFilePath(assetIndexId + QStringLiteral(".json"));
    if (!downloadFileSync(applyMirrorUrl(QUrl(assetIndexUrl), source), assetIndexPath, &m_lastError)) {
        return false;
    }

    QJsonObject assetIndexJson;
    if (!loadJsonFile(assetIndexPath, &assetIndexJson, &m_lastError)) {
        return false;
    }

    AMCS::Core::Download::AsulMultiDownloader assetsDownloader;
    AMCS::Core::Download::AsulMultiDownloader librariesDownloader;
    AMCS::Core::Download::AsulMultiDownloader versionDownloader;

    librariesDownloader.setMaxConcurrentDownloads(64);
    librariesDownloader.setMaxConnectionsPerHost(64);
    librariesDownloader.setLargeFileThreshold(5LL * 1024 * 1024);
    librariesDownloader.setSegmentCountForLargeFile(4);

    assetsDownloader.setMaxConcurrentDownloads(512);
    assetsDownloader.setMaxConnectionsPerHost(512);
    assetsDownloader.setLargeFileThreshold(1LL * 1024 * 1024);
    assetsDownloader.setSegmentCountForLargeFile(4);

    versionDownloader.setLargeFileThreshold(10LL * 1024 * 1024);
    versionDownloader.setSegmentCountForLargeFile(8);

    emit installPhaseChanged(QStringLiteral("download"));

    QSet<QString> nativeJarPaths;

    int totalTasks = 0;
    int completedTasks = 0;
    int failedTasks = 0;
    qint64 plannedTotal = 0;

    const QString jarPath = QDir(versionDir).absoluteFilePath(version.id + QStringLiteral(".jar"));
    const QJsonObject downloads = versionJson.value(QStringLiteral("downloads")).toObject();
    const QJsonObject clientObj = downloads.value(QStringLiteral("client")).toObject();
    QString clientUrl;
    if (source == Api::McApi::VersionSource::BMCLApi) {
        clientUrl = buildBmclapiVersionUrl(version.id, QStringLiteral("client")).toString();
    } else {
        clientUrl = applyMirrorUrl(QUrl(clientObj.value(QStringLiteral("url")).toString()), source).toString();
    }
    if (!clientUrl.isEmpty()) {
        const qint64 size = clientObj.value(QStringLiteral("size")).toVariant().toLongLong();
        QFileInfo fileInfo(jarPath);
        if (!fileInfo.exists() || (size > 0 && fileInfo.size() != size)) {
            versionDownloader.addDownload(QUrl(clientUrl), jarPath, 10, size);
            totalTasks += 1;
            if (size > 0) {
                plannedTotal += size;
            }
        }
    }

    const QJsonArray libraries = versionJson.value(QStringLiteral("libraries")).toArray();
    int nativeLibCount = 0;
    int nativeMatchCount = 0;
    int nativeLogCount = 0;
    for (const auto &libVal : libraries) {
        const QJsonObject libObj = libVal.toObject();
        const QJsonArray rules = libObj.value(QStringLiteral("rules")).toArray();
        if (!ruleAllows(rules)) {
            continue;
        }

        const QJsonObject libDownloads = libObj.value(QStringLiteral("downloads")).toObject();
        const QJsonObject artifact = libDownloads.value(QStringLiteral("artifact")).toObject();
        const QString artifactPath = artifact.value(QStringLiteral("path")).toString();
        const QString artifactUrl = applyMirrorUrl(QUrl(artifact.value(QStringLiteral("url")).toString()), source).toString();
        const qint64 artifactSize = artifact.value(QStringLiteral("size")).toVariant().toLongLong();
        if (!artifactPath.isEmpty() && !artifactUrl.isEmpty()) {
            const QString savePath = QDir(librariesDir).absoluteFilePath(artifactPath);
            QFileInfo fileInfo(savePath);
            if (!fileInfo.exists() || (artifactSize > 0 && fileInfo.size() != artifactSize)) {
                librariesDownloader.addDownload(QUrl(artifactUrl), savePath, 5, artifactSize);
                totalTasks += 1;
                if (artifactSize > 0) {
                    plannedTotal += artifactSize;
                }
            }
        }

        const QString nativeKey = resolveNativeClassifier(libObj);
        if (!nativeKey.isEmpty()) {
            nativeLibCount += 1;
            const QJsonObject classifiers = libDownloads.value(QStringLiteral("classifiers")).toObject();
            const QJsonObject nativeObj = classifiers.value(nativeKey).toObject();
            if (nativeObj.isEmpty() && nativeLogCount < 10) {
                qInfo().noquote() << "[natives] missing classifier" << nativeKey
                                 << "lib" << libObj.value(QStringLiteral("name")).toString();
                nativeLogCount += 1;
            }
            const QString nativePath = nativeObj.value(QStringLiteral("path")).toString();
            const QString nativeUrl = applyMirrorUrl(QUrl(nativeObj.value(QStringLiteral("url")).toString()), source).toString();
            const qint64 nativeSize = nativeObj.value(QStringLiteral("size")).toVariant().toLongLong();
            if (!nativePath.isEmpty() && !nativeUrl.isEmpty()) {
                const QString savePath = QDir(librariesDir).absoluteFilePath(nativePath);
                QFileInfo fileInfo(savePath);
                if (!fileInfo.exists() || (nativeSize > 0 && fileInfo.size() != nativeSize)) {
                    librariesDownloader.addDownload(QUrl(nativeUrl), savePath, 5, nativeSize);
                    totalTasks += 1;
                    if (nativeSize > 0) {
                        plannedTotal += nativeSize;
                    }
                }
                nativeJarPaths.insert(savePath);
                nativeMatchCount += 1;
            }
        } else {
            const QString libName = libObj.value(QStringLiteral("name")).toString();
            const QString classifier = libraryClassifierFromName(libName);
            if (isNewFormatNativeArtifact(artifactPath, classifier)) {
                nativeLibCount += 1;
                if (classifierMatchesOsAndArch(classifier)) {
                    if (!artifactPath.isEmpty()) {
                        const QString savePath = QDir(librariesDir).absoluteFilePath(artifactPath);
                        nativeJarPaths.insert(savePath);
                        nativeMatchCount += 1;
                    }
                } else if (nativeLogCount < 10) {
                    qInfo().noquote() << "[natives] skip classifier" << classifier
                                      << "lib" << libName;
                    nativeLogCount += 1;
                }
            }
        }
    }
    qInfo().noquote() << "[natives] libraries with natives:" << nativeLibCount
                      << "matched:" << nativeMatchCount;

    const QJsonObject objects = assetIndexJson.value(QStringLiteral("objects")).toObject();
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const QJsonObject obj = it.value().toObject();
        const QString hash = obj.value(QStringLiteral("hash")).toString();
        if (hash.isEmpty()) {
            continue;
        }
        const QUrl url = assetUrlFromHash(hash);
        const QString prefix = hash.left(2);
        const QString savePath = QDir(objectsDir).absoluteFilePath(prefix + QStringLiteral("/") + hash);
        const qint64 size = obj.value(QStringLiteral("size")).toVariant().toLongLong();
        QFileInfo fileInfo(savePath);
        if (!fileInfo.exists() || (size > 0 && fileInfo.size() != size)) {
            const QString mirrorUrl = applyMirrorUrl(url, source).toString();
            assetsDownloader.addDownload(QUrl(mirrorUrl), savePath, 0, size);
            totalTasks += 1;
            if (size > 0) {
                plannedTotal += size;
            }
        }
    }

    bool librariesFinished = false;
    bool assetsFinished = false;
    bool versionFinished = false;
    bool failed = false;

    QEventLoop loop;

    auto tryFinishAll = [&]() {
        if (librariesFinished && assetsFinished && versionFinished) {
            loop.quit();
        }
    };

    QObject::connect(&librariesDownloader, &AMCS::Core::Download::AsulMultiDownloader::downloadFailed,
                     [&](const QString &, const QString &error) {
                         failed = true;
                         m_lastError = error;
                         failedTasks += 1;
                     });
    QObject::connect(&assetsDownloader, &AMCS::Core::Download::AsulMultiDownloader::downloadFailed,
                     [&](const QString &, const QString &error) {
                         failed = true;
                         m_lastError = error;
                         failedTasks += 1;
                     });
    QObject::connect(&versionDownloader, &AMCS::Core::Download::AsulMultiDownloader::downloadFailed,
                     [&](const QString &, const QString &error) {
                         failed = true;
                         m_lastError = error;
                         failedTasks += 1;
                     });

    QObject::connect(&librariesDownloader, &AMCS::Core::Download::AsulMultiDownloader::downloadFinished,
                     [&](const QString &, const QString &) { completedTasks += 1; });
    QObject::connect(&assetsDownloader, &AMCS::Core::Download::AsulMultiDownloader::downloadFinished,
                     [&](const QString &, const QString &) { completedTasks += 1; });
    QObject::connect(&versionDownloader, &AMCS::Core::Download::AsulMultiDownloader::downloadFinished,
                     [&](const QString &, const QString &) { completedTasks += 1; });

    QObject::connect(&librariesDownloader, &AMCS::Core::Download::AsulMultiDownloader::allDownloadsFinished, [&]() {
        librariesFinished = true;
        tryFinishAll();
    });
    QObject::connect(&assetsDownloader, &AMCS::Core::Download::AsulMultiDownloader::allDownloadsFinished, [&]() {
        assetsFinished = true;
        tryFinishAll();
    });
    QObject::connect(&versionDownloader, &AMCS::Core::Download::AsulMultiDownloader::allDownloadsFinished, [&]() {
        versionFinished = true;
        tryFinishAll();
    });

    if (librariesDownloader.getAllTaskIds().isEmpty()) {
        librariesFinished = true;
    }
    if (assetsDownloader.getAllTaskIds().isEmpty()) {
        assetsFinished = true;
    }
    if (versionDownloader.getAllTaskIds().isEmpty()) {
        versionFinished = true;
    }

    QTimer progressTimer;
    InstallProgress progress;
    progress.totalTasks = totalTasks;
    progress.totalBytes = plannedTotal;

    QObject::connect(&progressTimer, &QTimer::timeout, [&]() {
        auto libStats = librariesDownloader.getStatistics();
        auto assetStats = assetsDownloader.getStatistics();
        auto verStats = versionDownloader.getStatistics();
        progress.downloadedBytes = libStats.totalDownloaded + assetStats.totalDownloaded + verStats.totalDownloaded;
        progress.speedBytes = libStats.totalDownloadSpeed + assetStats.totalDownloadSpeed + verStats.totalDownloadSpeed;
        progress.completedTasks = completedTasks;
        progress.failedTasks = failedTasks;
        emit installProgressUpdated(progress);
    });

    progressTimer.start(500);

    if (!librariesFinished || !assetsFinished || !versionFinished) {
        loop.exec();
    }

    progressTimer.stop();

    if (failed) {
        return false;
    }

    emit installPhaseChanged(QStringLiteral("natives"));

    const QString nativeVersionId = versionJson.value(QStringLiteral("id")).toString(version.id);
    const QString nativeDestDir = QDir(versionDir).absoluteFilePath(nativeVersionId + QStringLiteral("-natives"));
    if (!QDir().mkpath(nativeDestDir)) {
        m_lastError = QStringLiteral("Failed to create natives dir");
        return false;
    }
    qInfo().noquote() << "[natives] matched jars:" << nativeJarPaths.size();
    for (const auto &nativeJar : nativeJarPaths) {
        qInfo().noquote() << "[natives] jar:" << nativeJar;
    }
    if (nativeJarPaths.isEmpty()) {
        m_lastError = QStringLiteral("No native libraries matched rules");
        return false;
    }
    for (const auto &nativeJar : nativeJarPaths) {
        QFileInfo info(nativeJar);
        if (!info.exists()) {
            continue;
        }
        QString error;
        if (!extractZipToDir(nativeJar, nativeDestDir, &error)) {
            m_lastError = error;
            return false;
        }
    }

    auto *settings = CoreSettings::getInstance();
    const QString versionsFilePath = settings->versionsFilePath();
    if (!versionsFilePath.isEmpty()) {
        QVector<Api::McApi::MCVersion> versions = settings->getLocalVersions();
        bool replaced = false;
        for (auto &entry : versions) {
            if (entry.id == version.id) {
                entry = version;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            versions.append(version);
        }

        settings->setLocalVersions(versions);
        settings->versionManager()->setLocalVersions(versions);

        const QString dirPath = QFileInfo(versionsFilePath).absolutePath();
        if (!QDir().mkpath(dirPath)) {
            m_lastError = QStringLiteral("Failed to create dir: %1").arg(dirPath);
            return false;
        }

        QString error;
        if (!Api::McApi::saveLocalVersions(versionsFilePath, versions, &error)) {
            m_lastError = error;
            return false;
        }
    }

    emit installPhaseChanged(QStringLiteral("done"));
    return true;
}

bool LauncherCore::runMCVersion(const Api::McApi::MCVersion &version,
                                const Auth::McAccount &account,
                                const QString &baseDir,
                                const LaunchOptions &options,
                                QProcess **outProcess)
{
    m_lastError.clear();

    const QString base = QDir(baseDir).absolutePath();
    const QString versionsDir = QDir(base).absoluteFilePath(QStringLiteral("versions"));
    const QString librariesDir = QDir(base).absoluteFilePath(QStringLiteral("libraries"));
    const QString assetsDir = options.assetsDir.isEmpty()
        ? QDir(base).absoluteFilePath(QStringLiteral("assets"))
        : options.assetsDir;

    QJsonObject merged;
    if (!loadMergedVersionJson(versionsDir, version.id, &merged, &m_lastError)) {
        return false;
    }

    const QString versionId = merged.value(QStringLiteral("id")).toString(version.id);
    const QString versionDir = QDir(versionsDir).absoluteFilePath(versionId);
    const QString jarId = merged.value(QStringLiteral("jar")).toString(versionId);
    const QString jarPath = QDir(versionsDir).absoluteFilePath(jarId + QStringLiteral("/") + jarId + QStringLiteral(".jar"));

    if (!QFileInfo::exists(jarPath)) {
        m_lastError = QStringLiteral("Client jar missing: %1").arg(jarPath);
        return false;
    }

    const QJsonObject assetIndexObj = merged.value(QStringLiteral("assetIndex")).toObject();
    const QString assetIndexId = assetIndexObj.value(QStringLiteral("id")).toString();
    if (assetIndexId.isEmpty()) {
        m_lastError = QStringLiteral("Asset index missing");
        return false;
    }

    const auto effectiveLaunchMode = options.launchMode.value_or(
        AMCS::Core::CoreSettings::getInstance()->getLaunchMode());

    QString gameDir;
    if (!options.gameDir.isEmpty()) {
        gameDir = options.gameDir;
    } else if (effectiveLaunchMode == AMCS::Core::CoreSettings::LaunchMode::Isolated) {
        gameDir = versionDir;
    } else {
        gameDir = base;
    }
    const QString nativesDir = QDir(versionDir).absoluteFilePath(versionId + QStringLiteral("-natives"));

    QStringList classpathEntries;
    const QJsonArray libraries = merged.value(QStringLiteral("libraries")).toArray();
    for (const auto &libVal : libraries) {
        const QJsonObject libObj = libVal.toObject();
        const QJsonArray rules = libObj.value(QStringLiteral("rules")).toArray();
        if (!ruleAllows(rules)) {
            continue;
        }

        const QJsonObject libDownloads = libObj.value(QStringLiteral("downloads")).toObject();
        const QJsonObject artifact = libDownloads.value(QStringLiteral("artifact")).toObject();
        const QString artifactPath = artifact.value(QStringLiteral("path")).toString();
        if (artifactPath.isEmpty()) {
            continue;
        }

        if (!libObj.contains(QStringLiteral("natives"))) {
            const QString libName = libObj.value(QStringLiteral("name")).toString();
            const QString classifier = libraryClassifierFromName(libName);
            if (isNewFormatNativeArtifact(artifactPath, classifier)) {
                continue;
            }
        }

        const QString libPath = QDir(librariesDir).absoluteFilePath(artifactPath);
        classpathEntries.append(QDir::toNativeSeparators(libPath));
    }

    classpathEntries.append(QDir::toNativeSeparators(jarPath));
    const QString classpath = classpathEntries.join(classpathSeparator());

    QMap<QString, QString> vars;
    vars.insert(QStringLiteral("auth_player_name"), account.accountName());
    vars.insert(QStringLiteral("version_name"), versionId);
    vars.insert(QStringLiteral("game_directory"), QDir::toNativeSeparators(gameDir));
    vars.insert(QStringLiteral("assets_root"), QDir::toNativeSeparators(assetsDir));
    vars.insert(QStringLiteral("assets_index_name"), assetIndexId);
    vars.insert(QStringLiteral("auth_uuid"), account.uuid());
    vars.insert(QStringLiteral("auth_access_token"), account.isOffline() ? QStringLiteral("0") : account.mcAccessToken());
    vars.insert(QStringLiteral("user_type"), account.userType());
    vars.insert(QStringLiteral("version_type"), options.versionTypeOverride.isEmpty() ? version.type : options.versionTypeOverride);
    vars.insert(QStringLiteral("natives_directory"), QDir::toNativeSeparators(nativesDir));
    vars.insert(QStringLiteral("classpath"), classpath);
    vars.insert(QStringLiteral("launcher_name"), options.launcherName);
    vars.insert(QStringLiteral("launcher_version"), options.launcherVersion);
    vars.insert(QStringLiteral("user_properties"), options.userProperties);

    QStringList jvmArgs;
    QStringList gameArgs;

    if (merged.contains(QStringLiteral("arguments")) && merged.value(QStringLiteral("arguments")).isObject()) {
        const QJsonObject argsObj = merged.value(QStringLiteral("arguments")).toObject();
        if (argsObj.value(QStringLiteral("jvm")).isArray()) {
            jvmArgs = buildArgsFromJsonArray(argsObj.value(QStringLiteral("jvm")).toArray(), vars);
        }
        if (argsObj.value(QStringLiteral("game")).isArray()) {
            gameArgs = buildArgsFromJsonArray(argsObj.value(QStringLiteral("game")).toArray(), vars);
        }
    } else {
        const QString legacyArgs = merged.value(QStringLiteral("minecraftArguments")).toString();
        if (!legacyArgs.isEmpty()) {
            gameArgs = splitArgs(replaceTokens(legacyArgs, vars));
        }
    }

    if (!hasJvmArg(jvmArgs, QStringLiteral("-Djava.library.path="))) {
        jvmArgs.append(QStringLiteral("-Djava.library.path=%1").arg(QDir::toNativeSeparators(nativesDir)));
    }

    if (!hasJvmArg(jvmArgs, QStringLiteral("-cp")) && !hasJvmArg(jvmArgs, QStringLiteral("-classpath"))) {
        jvmArgs.append(QStringLiteral("-cp"));
        jvmArgs.append(classpath);
    }

    if (options.minMemoryMb > 0 && !hasJvmArg(jvmArgs, QStringLiteral("-Xms"))) {
        jvmArgs.append(QStringLiteral("-Xms%1m").arg(options.minMemoryMb));
    }
    if (options.maxMemoryMb > 0 && !hasJvmArg(jvmArgs, QStringLiteral("-Xmx"))) {
        jvmArgs.append(QStringLiteral("-Xmx%1m").arg(options.maxMemoryMb));
    }

    for (const auto &arg : options.jvmArgs) {
        jvmArgs.append(replaceTokens(arg, vars));
    }

    if (options.fullscreen) {
        gameArgs.append(QStringLiteral("--fullscreen"));
    }
    if (options.width > 0) {
        gameArgs.append(QStringLiteral("--width"));
        gameArgs.append(QString::number(options.width));
    }
    if (options.height > 0) {
        gameArgs.append(QStringLiteral("--height"));
        gameArgs.append(QString::number(options.height));
    }

    for (const auto &arg : options.gameArgs) {
        gameArgs.append(replaceTokens(arg, vars));
    }

    const QString mainClass = merged.value(QStringLiteral("mainClass")).toString();
    if (mainClass.isEmpty()) {
        m_lastError = QStringLiteral("mainClass missing");
        return false;
    }

    QStringList finalArgs;
    finalArgs.append(jvmArgs);
    finalArgs.append(mainClass);
    finalArgs.append(gameArgs);

    const QString javaPath = options.javaPath.isEmpty() ? QStringLiteral("java") : options.javaPath;

    auto *process = new QProcess(this);
    process->setProgram(javaPath);
    process->setArguments(finalArgs);
    process->setWorkingDirectory(gameDir);
    process->start();

    if (!process->waitForStarted()) {
        m_lastError = QStringLiteral("Failed to start java process");
        process->deleteLater();
        return false;
    }

    if (outProcess) {
        *outProcess = process;
    }

    return true;
}

bool LauncherCore::isVersionInstalled(const Api::McApi::MCVersion &version, const QString &baseDir) const
{
    const QString base = QDir(baseDir).absolutePath();
    const QString versionDir = QDir(base).absoluteFilePath(QStringLiteral("versions/%1").arg(version.id));
    const QString versionJsonPath = QDir(versionDir).absoluteFilePath(version.id + QStringLiteral(".json"));
    const QString jarPath = QDir(versionDir).absoluteFilePath(version.id + QStringLiteral(".jar"));
    return QFileInfo::exists(versionJsonPath) && QFileInfo::exists(jarPath);
}

QString LauncherCore::lastError() const
{
    return m_lastError;
}
} // namespace AMCS::Core::Launcher
