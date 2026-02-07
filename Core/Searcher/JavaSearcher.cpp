#include "JavaSearcher.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <Windows.h>
#include "Everything.h"
#endif

namespace AMCS::Core::Searcher
{
namespace
{
QString extractJavaInfoLine(const QString &output)
{
    const QString trimmed = output.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    const QStringList lines = trimmed.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                            Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        return QString();
    }

    return lines.first().trimmed();
}

QString extractMajorVersion(const QString &infoLine)
{
    if (infoLine.isEmpty()) {
        return QString();
    }

    const QRegularExpression re(QStringLiteral("(\\d+)(?:\\.(\\d+))?"));
    const QRegularExpressionMatch match = re.match(infoLine);
    if (!match.hasMatch()) {
        return QString();
    }

    const QString major = match.captured(1);
    const QString minor = match.captured(2);
    if (major == QStringLiteral("1") && !minor.isEmpty()) {
        return minor;
    }

    return major;
}

Manager::JavaManager::JavaInfo detectJavaVersionInfo(const QString &javaPath)
{
    Manager::JavaManager::JavaInfo info;
    info.path = javaPath;
    if (javaPath.isEmpty()) {
        return info;
    }

    QProcess process;
    process.start(javaPath, {QStringLiteral("--version")});
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        return info;
    }

    const QString stdOut = QString::fromLocal8Bit(process.readAllStandardOutput());
    const QString stdErr = QString::fromLocal8Bit(process.readAllStandardError());
    QString infoLine = extractJavaInfoLine(stdOut);
    if (infoLine.isEmpty()) {
        infoLine = extractJavaInfoLine(stdErr);
    }

    info.info = infoLine;
    info.versionMajor = extractMajorVersion(infoLine);

    return info;
}

bool isUsefulInfo(const Manager::JavaManager::JavaInfo &info)
{
    if (info.versionMajor.isEmpty()) {
        return false;
    }

    if (info.info.contains(QStringLiteral("Error"), Qt::CaseInsensitive)) {
        return false;
    }

    return true;
}

QVector<QString> parseCommandPaths(const QString &output)
{
    QVector<QString> paths;
    const QString trimmed = output.trimmed();
    if (trimmed.isEmpty()) {
        return paths;
    }

    const QStringList lines = trimmed.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                            Qt::SkipEmptyParts);
    for (const auto &line : lines) {
        const QString cleaned = QDir::cleanPath(line.trimmed());
        if (!cleaned.isEmpty()) {
            paths.append(cleaned);
        }
    }

    return paths;
}

QVector<QString> findJavaByCommand(const QString &command, const QStringList &args)
{
    QProcess process;
    process.start(command, args);
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        return {};
    }

    const QString stdOut = QString::fromLocal8Bit(process.readAllStandardOutput());
    const QString stdErr = QString::fromLocal8Bit(process.readAllStandardError());
    QVector<QString> results = parseCommandPaths(stdOut);
    const QVector<QString> extra = parseCommandPaths(stdErr);
    results.reserve(results.size() + extra.size());
    for (const auto &entry : extra) {
        results.append(entry);
    }

    return results;
}

#ifdef Q_OS_WIN
QString errorFromEverything(DWORD code)
{
    switch (code) {
    case EVERYTHING_OK:
        return QString();
    case EVERYTHING_ERROR_MEMORY:
        return QStringLiteral("Everything error: out of memory");
    case EVERYTHING_ERROR_IPC:
        return QStringLiteral("Everything error: IPC not available. Is Everything running?");
    case EVERYTHING_ERROR_REGISTERCLASSEX:
        return QStringLiteral("Everything error: failed to register window class");
    default:
        return QStringLiteral("Everything error: unknown (%1)").arg(code);
    }
}

QVector<QString> findJavaByEverything(QString *error)
{
    Everything_SetSearchW(L"java.exe|javaw.exe");
    Everything_SetRequestFlags(EVERYTHING_REQUEST_FULL_PATH_AND_FILE_NAME);
    Everything_SetMatchPath(TRUE);
    Everything_SetMatchCase(FALSE);
    Everything_SetMax(512);

    if (!Everything_QueryW(TRUE)) {
        const QString err = errorFromEverything(Everything_GetLastError());
        if (error) {
            *error = err.isEmpty() ? QStringLiteral("Everything query failed") : err;
        }
        return {};
    }

    const DWORD count = Everything_GetNumResults();
    QVector<QString> paths;
    paths.reserve(static_cast<int>(count));

    for (DWORD i = 0; i < count; ++i) {
        wchar_t buffer[4096] = {0};
        const DWORD length = Everything_GetResultFullPathNameW(
            i,
            buffer,
            static_cast<DWORD>(sizeof(buffer) / sizeof(wchar_t)));
        if (length == 0) {
            continue;
        }

        const QString path = QString::fromWCharArray(buffer, static_cast<int>(length));
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }

    return paths;
}

bool isJavaExecutablePath(const QString &path)
{
    return path.endsWith(QStringLiteral("java.exe"), Qt::CaseInsensitive)
        || path.endsWith(QStringLiteral("javaw.exe"), Qt::CaseInsensitive);
}

QString resolveJavaPathForVersion(const QString &path)
{
    if (!path.endsWith(QStringLiteral("javaw.exe"), Qt::CaseInsensitive)) {
        return path;
    }

    const QString candidate = QDir(QFileInfo(path).absolutePath())
                                  .absoluteFilePath(QStringLiteral("java.exe"));
    if (QFileInfo::exists(candidate)) {
        return candidate;
    }

    return path;
}
#endif

void appendJavaInfoFromPaths(const QVector<QString> &paths,
                             QSet<QString> &unique,
                             QVector<QString> &results,
                             QVector<Manager::JavaManager::JavaInfo> &infos)
{
    for (const auto &path : paths) {
        if (path.isEmpty()) {
            continue;
        }

        const QString cleaned = QDir::cleanPath(path);
        if (unique.contains(cleaned)) {
            continue;
        }

#ifdef Q_OS_WIN
        if (!isJavaExecutablePath(cleaned)) {
            continue;
        }
#endif

        if (!QFileInfo::exists(cleaned)) {
            continue;
        }

#ifdef Q_OS_WIN
        const QString javaPathForVersion = resolveJavaPathForVersion(cleaned);
#else
        const QString javaPathForVersion = cleaned;
#endif

        Manager::JavaManager::JavaInfo info = detectJavaVersionInfo(javaPathForVersion);
        info.path = cleaned;
        if (!isUsefulInfo(info)) {
            continue;
        }

        unique.insert(cleaned);
        results.append(cleaned);
        infos.append(info);
    }
}
}

JavaSearcher::JavaSearcher(Manager::JavaManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
{
    if (!m_manager) {
        m_manager = Manager::JavaManager::getInstance();
    }

    if (m_manager) {
        connect(this, &JavaSearcher::javaPathsFound, m_manager, &Manager::JavaManager::updateJavaPaths);
        connect(this, &JavaSearcher::javaInfosFound, m_manager, &Manager::JavaManager::updateJavaInfos);
    }
}

bool JavaSearcher::searchForJava(SearchMode mode)
{
#ifdef Q_OS_WIN
    QSet<QString> unique;
    QVector<QString> results;
    QVector<Manager::JavaManager::JavaInfo> infos;
    if (mode == SearchMode::Deep) {
        QString error;
        const QVector<QString> everythingPaths = findJavaByEverything(&error);
        if (!error.isEmpty()) {
            emit searchFailed(error);
            return false;
        }

        appendJavaInfoFromPaths(everythingPaths, unique, results, infos);
    }

    if (mode == SearchMode::SystemOnly || infos.isEmpty()) {
        const QVector<QString> whereJava = findJavaByCommand(QStringLiteral("where"),
                                                             {QStringLiteral("java")});
        const QVector<QString> whereJavaw = findJavaByCommand(QStringLiteral("where"),
                                                              {QStringLiteral("javaw")});
        appendJavaInfoFromPaths(whereJava, unique, results, infos);
        appendJavaInfoFromPaths(whereJavaw, unique, results, infos);
    }

    emit javaPathsFound(results);
    emit javaInfosFound(infos);
    return true;
#elif defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    QSet<QString> unique;
    QVector<QString> results;
    QVector<Manager::JavaManager::JavaInfo> infos;
    const QVector<QString> whichPaths = findJavaByCommand(QStringLiteral("which"),
                                                          {QStringLiteral("-a"), QStringLiteral("java")});
    appendJavaInfoFromPaths(whichPaths, unique, results, infos);

    emit javaPathsFound(results);
    emit javaInfosFound(infos);
    return true;
#else
    emit searchFailed(QStringLiteral("Java search is not supported on this platform"));
    return false;
#endif
}
} // namespace AMCS::Core::Searcher
