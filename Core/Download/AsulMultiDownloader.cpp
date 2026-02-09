#include "AsulMultiDownloader.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QRandomGenerator>
#include <QDebug>
#include <atomic>

// ==================== AsulMultiDownloader 实现 ====================

  AsulMultiDownloader::AsulMultiDownloader(QObject *parent)
    : QObject(parent)
    , m_maxConcurrentDownloads(32)
    , m_largeFileThreshold(10 * 1024 * 1024)
    , m_segmentCount(4)
    , m_maxConnectionsPerHost(8)
    , m_downloadTimeout(30000)
    , m_autoRetry(true)
    , m_maxRetryCount(10)
    , m_activeDownloads(0)
    , m_taskIdCounter(0)
    , m_speedMonitoringEnabled(true)
    , m_speedThreshold(256 * 1024)
    , m_lastSpeedCheck(0)
    , m_lastBytesDownloaded(0)
    , m_monitorLastTime(0)
    , m_monitorLastBytes(0)
    , m_allFinishedEmitted(false)
    , m_networkManagerPoolSize(8)  // 优化：使用8个网络管理器，减少资源占用
{
    // 初始化网络管理器池
    for (int i = 0; i < m_networkManagerPoolSize; ++i) {
        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        m_networkManagers.append(manager);
    }

    m_statisticsTimer = new QTimer(this);
    connect(m_statisticsTimer, &QTimer::timeout, this, &AsulMultiDownloader::onUpdateStatistics);
    m_statisticsTimer->start(1000);  // 每秒更新一次统计信息

    // 启动监控线程，用于动态线程调度（参考PCL）
    // 优化：降低监控频率从20ms到1000ms，减少系统负载
    m_monitorTimer = new QTimer(this);
    connect(m_monitorTimer, &QTimer::timeout, this, &AsulMultiDownloader::onMonitorDownloads);
    m_monitorTimer->start(1000);  // 每秒检查一次，大幅降低频率

    // 初始化域名策略（参考PCL对特定域名的优化）
    m_noMultiThreadHosts << "bmclapi" << "github.com" << "modrinth.com"
                         << "optifine.net" << "curseforge.com";
}

AsulMultiDownloader::~AsulMultiDownloader()
{
    cancelAll();
}

// ==================== 配置接口实现 ====================

void AsulMultiDownloader::setMaxConcurrentDownloads(int count)
{
    QMutexLocker locker(&m_mutex);
    m_maxConcurrentDownloads = qMax(1, count);
    processQueue();
}

int AsulMultiDownloader::maxConcurrentDownloads() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxConcurrentDownloads;
}

void AsulMultiDownloader::setLargeFileThreshold(qint64 bytes)
{
    QMutexLocker locker(&m_mutex);
    m_largeFileThreshold = qMax(qint64(0), bytes);
}

qint64 AsulMultiDownloader::largeFileThreshold() const
{
    QMutexLocker locker(&m_mutex);
    return m_largeFileThreshold;
}

void AsulMultiDownloader::setSegmentCountForLargeFile(int count)
{
    QMutexLocker locker(&m_mutex);
    m_segmentCount = qMax(1, count);
}

int AsulMultiDownloader::segmentCountForLargeFile() const
{
    QMutexLocker locker(&m_mutex);
    return m_segmentCount;
}

void AsulMultiDownloader::setMaxConnectionsPerHost(int count)
{
    QMutexLocker locker(&m_mutex);
    m_maxConnectionsPerHost = qMax(1, count);
}

int AsulMultiDownloader::maxConnectionsPerHost() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxConnectionsPerHost;
}

void AsulMultiDownloader::setDownloadTimeout(int msecs)
{
    QMutexLocker locker(&m_mutex);
    m_downloadTimeout = qMax(1000, msecs);
}

int AsulMultiDownloader::downloadTimeout() const
{
    QMutexLocker locker(&m_mutex);
    return m_downloadTimeout;
}

void AsulMultiDownloader::setAutoRetry(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_autoRetry = enable;
}

bool AsulMultiDownloader::autoRetry() const
{
    QMutexLocker locker(&m_mutex);
    return m_autoRetry;
}

void AsulMultiDownloader::setMaxRetryCount(int count)
{
    QMutexLocker locker(&m_mutex);
    m_maxRetryCount = qMax(0, count);
}

int AsulMultiDownloader::maxRetryCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxRetryCount;
}

// ==================== PCL优化新增接口实现 ====================

void AsulMultiDownloader::setSpeedThreshold(qint64 bytesPerSecond)
{
    QMutexLocker locker(&m_mutex);
    m_speedThreshold = qMax(qint64(0), bytesPerSecond);
}

qint64 AsulMultiDownloader::speedThreshold() const
{
    QMutexLocker locker(&m_mutex);
    return m_speedThreshold;
}

void AsulMultiDownloader::setSpeedMonitoringEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_speedMonitoringEnabled = enable;
}

bool AsulMultiDownloader::speedMonitoringEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_speedMonitoringEnabled;
}

void AsulMultiDownloader::addNoMultiThreadHost(const QString &host)
{
    QMutexLocker locker(&m_mutex);
    if (!m_noMultiThreadHosts.contains(host)) {
        m_noMultiThreadHosts.append(host);
    }
}

void AsulMultiDownloader::removeNoMultiThreadHost(const QString &host)
{
    QMutexLocker locker(&m_mutex);
    m_noMultiThreadHosts.removeAll(host);
}

void AsulMultiDownloader::clearNoMultiThreadHosts()
{
    QMutexLocker locker(&m_mutex);
    m_noMultiThreadHosts.clear();
}

QStringList AsulMultiDownloader::noMultiThreadHosts() const
{
    QMutexLocker locker(&m_mutex);
    return m_noMultiThreadHosts;
}

// ==================== 下载控制接口实现 ====================

QString AsulMultiDownloader::addDownload(const QUrl &url, const QString &savePath, int priority, qint64 knownFileSize)
{
    QMutexLocker locker(&m_mutex);

    QString taskId = generateTaskId();

    // 创建下载任务
    auto task = std::make_shared<DownloadTask>(taskId, url, savePath, priority, this);
    task->setTimeout(m_downloadTimeout);

    // 设置已知文件大小（跳过HEAD请求优化）
    if (knownFileSize > 0) {
        task->setKnownFileSize(knownFileSize);
    }

    // 设置分段数
    task->setSegmentCount(m_segmentCount);

    // 连接信号
    connect(task.get(), &DownloadTask::started, this, &AsulMultiDownloader::downloadStarted);
    connect(task.get(), &DownloadTask::progress, this, &AsulMultiDownloader::onTaskProgress);
    connect(task.get(), &DownloadTask::finished, this, &AsulMultiDownloader::onTaskFinished);
    connect(task.get(), &DownloadTask::failed, this, &AsulMultiDownloader::onTaskFailed);
    connect(task.get(), &DownloadTask::paused, this, &AsulMultiDownloader::downloadPaused);
    connect(task.get(), &DownloadTask::resumed, this, &AsulMultiDownloader::downloadResumed);

    m_tasks[taskId] = task;
    m_taskStatus[taskId] = DownloadStatus::Queued;
    m_taskRetryCount[taskId] = 0;
    m_taskQueue.enqueue(taskId);

    emit downloadAdded(taskId, url);

    // 尝试处理队列
    processQueue();

    return taskId;
}

QStringList AsulMultiDownloader::addDownloads(const QList<QUrl> &urls, const QStringList &savePaths, int priority)
{
    QStringList taskIds;

    int count = qMin(urls.size(), savePaths.size());
    for (int i = 0; i < count; ++i) {
        QString taskId = addDownload(urls[i], savePaths[i], priority);
        taskIds.append(taskId);
    }

    return taskIds;
}

void AsulMultiDownloader::pauseDownload(const QString &taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_tasks.contains(taskId)) {
        return;
    }

    auto task = m_tasks[taskId];
    DownloadStatus status = m_taskStatus[taskId];

    if (status == DownloadStatus::Downloading) {
        task->pause();
        m_taskStatus[taskId] = DownloadStatus::Paused;
        updateHostConnections(task->url().host(), -1);
        m_activeDownloads--;
        processQueue();
    } else if (status == DownloadStatus::Queued) {
        m_taskStatus[taskId] = DownloadStatus::Paused;
        m_taskQueue.removeOne(taskId);
    }
}

void AsulMultiDownloader::resumeDownload(const QString &taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_tasks.contains(taskId)) {
        return;
    }

    DownloadStatus status = m_taskStatus[taskId];

    if (status == DownloadStatus::Paused) {
        m_taskStatus[taskId] = DownloadStatus::Queued;
        m_taskQueue.enqueue(taskId);
        processQueue();
    }
}

void AsulMultiDownloader::cancelDownload(const QString &taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_tasks.contains(taskId)) {
        return;
    }

    auto task = m_tasks[taskId];
    DownloadStatus status = m_taskStatus[taskId];

    if (status == DownloadStatus::Downloading) {
        task->cancel();
        m_taskStatus[taskId] = DownloadStatus::Canceled;
        updateHostConnections(task->url().host(), -1);
        m_activeDownloads--;
        processQueue();
    } else if (status == DownloadStatus::Queued) {
        m_taskStatus[taskId] = DownloadStatus::Canceled;
        m_taskQueue.removeOne(taskId);
    } else {
        m_taskStatus[taskId] = DownloadStatus::Canceled;
    }

    emit downloadCanceled(taskId);
}

void AsulMultiDownloader::pauseAll()
{
    QMutexLocker locker(&m_mutex);

    QStringList taskIds = m_tasks.keys();
    for (const QString &taskId : taskIds) {
        locker.unlock();
        pauseDownload(taskId);
        locker.relock();
    }
}

void AsulMultiDownloader::resumeAll()
{
    QMutexLocker locker(&m_mutex);

    QStringList taskIds = m_tasks.keys();
    for (const QString &taskId : taskIds) {
        locker.unlock();
        resumeDownload(taskId);
        locker.relock();
    }
}

void AsulMultiDownloader::cancelAll()
{
    QMutexLocker locker(&m_mutex);

    QStringList taskIds = m_tasks.keys();
    for (const QString &taskId : taskIds) {
        locker.unlock();
        cancelDownload(taskId);
        locker.relock();
    }
}

void AsulMultiDownloader::clearFinishedTasks()
{
    QMutexLocker locker(&m_mutex);

    QStringList toRemove;
    for (auto it = m_taskStatus.begin(); it != m_taskStatus.end(); ++it) {
        if (it.value() == DownloadStatus::Completed ||
            it.value() == DownloadStatus::Failed ||
            it.value() == DownloadStatus::Canceled) {
            toRemove.append(it.key());
        }
    }

    for (const QString &taskId : toRemove) {
        m_tasks.remove(taskId);
        m_taskStatus.remove(taskId);
        m_taskRetryCount.remove(taskId);
    }
}

// ==================== 查询接口实现 ====================

DownloadInfo AsulMultiDownloader::getDownloadInfo(const QString &taskId) const
{
    QMutexLocker locker(&m_mutex);

    DownloadInfo info;

    if (!m_tasks.contains(taskId)) {
        return info;
    }

    auto task = m_tasks[taskId];
    info.taskId = taskId;
    info.url = task->url();
    info.savePath = task->savePath();
    info.fileSize = task->fileSize();
    info.downloadedSize = task->downloadedSize();
    info.priority = task->priority();
    info.supportRange = task->supportRange();
    info.segmentCount = task->segmentCount();
    info.errorString = task->errorString();

    return info;
}

DownloadStatus AsulMultiDownloader::getDownloadStatus(const QString &taskId) const
{
    QMutexLocker locker(&m_mutex);

    if (!m_taskStatus.contains(taskId)) {
        return DownloadStatus::Failed;
    }

    return m_taskStatus[taskId];
}

double AsulMultiDownloader::getDownloadProgress(const QString &taskId) const
{
    QMutexLocker locker(&m_mutex);

    if (!m_tasks.contains(taskId)) {
        return 0.0;
    }

    auto task = m_tasks[taskId];
    qint64 fileSize = task->fileSize();

    if (fileSize <= 0) {
        return 0.0;
    }

    return (task->downloadedSize() * 100.0) / fileSize;
}

qint64 AsulMultiDownloader::getDownloadSpeed(const QString &taskId) const
{
    // 这个方法需要额外的速度跟踪机制，暂时返回0
    // 在实际应用中，可以维护一个时间窗口内的下载量来计算速度
    Q_UNUSED(taskId);
    return 0;
}

QStringList AsulMultiDownloader::getAllTaskIds() const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.keys();
}

DownloadStatistics AsulMultiDownloader::getStatistics() const
{
    QMutexLocker locker(&m_mutex);
    return m_statistics;
}

// ==================== 私有槽函数实现 ====================

void AsulMultiDownloader::onTaskFinished(const QString &taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_tasks.contains(taskId)) {
        return;
    }

    // 状态守卫：仅处理 Downloading 状态的任务，防止重复处理
    if (m_taskStatus[taskId] != DownloadStatus::Downloading) {
        return;
    }

    auto task = m_tasks[taskId];
    m_taskStatus[taskId] = DownloadStatus::Completed;
    updateHostConnections(task->url().host(), -1);
    m_activeDownloads--;
    m_statistics.completedTasks++;
    m_taskLastProgress.remove(taskId);

    emit downloadFinished(taskId, task->savePath());

    processQueue();

    // 检查是否所有任务都完成了
    checkAndEmitAllFinished();
}

void AsulMultiDownloader::onTaskFailed(const QString &taskId, const QString &error)
{
    QMutexLocker locker(&m_mutex);

    if (!m_tasks.contains(taskId)) {
        return;
    }

    // 状态守卫：仅处理 Downloading 状态的任务，防止重复处理
    if (m_taskStatus[taskId] != DownloadStatus::Downloading) {
        return;
    }

    auto task = m_tasks[taskId];
    updateHostConnections(task->url().host(), -1);
    m_activeDownloads--;

    // 检查是否需要重试
    if (m_autoRetry && m_taskRetryCount[taskId] < m_maxRetryCount) {
        m_taskRetryCount[taskId]++;
        m_taskStatus[taskId] = DownloadStatus::Queued;
        m_taskQueue.enqueue(taskId);

        emit downloadRetrying(taskId, m_taskRetryCount[taskId]);

        processQueue();
    } else {
        m_taskStatus[taskId] = DownloadStatus::Failed;
        m_statistics.failedTasks++;

        emit downloadFailed(taskId, error);

        processQueue();

        // 检查是否所有任务都完成了（包括失败的任务）
        checkAndEmitAllFinished();
    }
}

void AsulMultiDownloader::onTaskProgress(const QString &taskId, qint64 received, qint64 total)
{
    // 更新卡住检测时间戳
    {
        QMutexLocker locker(&m_mutex);
        m_taskLastProgress[taskId] = QDateTime::currentMSecsSinceEpoch();
    }
    emit downloadProgress(taskId, received, total);
}

void AsulMultiDownloader::onUpdateStatistics()
{
    QMutexLocker locker(&m_mutex);

    m_statistics.activeDownloads = m_activeDownloads;
    m_statistics.queuedDownloads = m_taskQueue.size();

    // 计算总下载量：已完成任务用fileSize（精确），进行中任务用downloadedSize（实时）
    qint64 totalDownloaded = 0;
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        DownloadStatus status = m_taskStatus.value(it.key(), DownloadStatus::Queued);
        if (status == DownloadStatus::Completed) {
            // 已完成任务：用fileSize确保不因重试重置而回退
            totalDownloaded += it.value()->fileSize() > 0 ? it.value()->fileSize() : it.value()->downloadedSize();
        } else if (status == DownloadStatus::Downloading) {
            // 进行中任务：用实时进度
            totalDownloaded += it.value()->downloadedSize();
        }
        // Queued/Failed/Paused的任务不计入当前下载量
    }
    m_statistics.totalDownloaded = totalDownloaded;

    // 计算下载速度
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (m_lastSpeedCheck > 0) {
        qint64 timeDiff = currentTime - m_lastSpeedCheck;
        if (timeDiff >= 1000) {  // 至少1秒
            qint64 bytesDiff = totalDownloaded - m_lastBytesDownloaded;
            if (bytesDiff < 0) bytesDiff = 0;  // 防止负速度（重试时downloadedSize重置）
            // 计算速度（字节/秒）
            m_statistics.totalDownloadSpeed = (bytesDiff * 1000) / timeDiff;

            // 限制最大速度显示（避免显示不合理的高速度）
            if (m_statistics.totalDownloadSpeed > 1024 * 1024 * 1024) {
                m_statistics.totalDownloadSpeed = 0;
            }

            m_lastSpeedCheck = currentTime;
            m_lastBytesDownloaded = totalDownloaded;
        }
    } else {
        m_lastSpeedCheck = currentTime;
        m_lastBytesDownloaded = totalDownloaded;
        m_statistics.totalDownloadSpeed = 0;
    }

    emit statisticsChanged(m_statistics);
}

// ==================== 私有方法实现 ====================

void AsulMultiDownloader::processQueue()
{
    // 注意：调用此方法时应已持有锁

    while (!m_taskQueue.isEmpty() && m_activeDownloads < m_maxConcurrentDownloads) {
        QString taskId = m_taskQueue.dequeue();

        if (!m_tasks.contains(taskId)) {
            continue;
        }

        auto task = m_tasks[taskId];
        QString host = task->url().host();

        // 检查是否可以启动下载（考虑Host连接限制）
        if (!canStartDownload(host)) {
            // 重新入队，等待下次处理
            m_taskQueue.enqueue(taskId);
            break;
        }

        startDownloadTask(task);
    }
}

void AsulMultiDownloader::startDownloadTask(std::shared_ptr<DownloadTask> task)
{
    QString taskId = task->taskId();
    QString host = task->url().host();

    m_taskStatus[taskId] = DownloadStatus::Downloading;
    m_taskLastProgress[taskId] = QDateTime::currentMSecsSinceEpoch();  // 初始化卡住检测时间戳
    updateHostConnections(host, 1);
    m_activeDownloads++;

    // 启动任务
    task->start();
}

QString AsulMultiDownloader::generateTaskId()
{
    m_taskIdCounter++;
    return QString("task_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(m_taskIdCounter);
}

void AsulMultiDownloader::updateHostConnections(const QString &host, int delta)
{
    if (!m_hostConnections.contains(host)) {
        m_hostConnections[host] = 0;
    }

    m_hostConnections[host] += delta;

    if (m_hostConnections[host] <= 0) {
        m_hostConnections.remove(host);
    }
}

int AsulMultiDownloader::getHostConnections(const QString &host) const
{
    return m_hostConnections.value(host, 0);
}

bool AsulMultiDownloader::canStartDownload(const QString &host) const
{
    return getHostConnections(host) < m_maxConnectionsPerHost;
}

// ==================== DownloadTask 实现 ====================

DownloadTask::DownloadTask(const QString &taskId, const QUrl &url,
                         const QString &savePath, int priority, QObject *parent)
    : QObject(parent)
    , m_taskId(taskId)
    , m_url(url)
    , m_savePath(savePath)
    , m_priority(priority)
    , m_timeout(30000)
    , m_fileSize(-1)
    , m_downloadedSize(0)
    , m_supportRange(false)
    , m_segmentCount(1)
    , m_networkManager(nullptr)
    , m_reply(nullptr)
    , m_file(nullptr)
    , m_ownsNetworkManager(false)
    , m_completedSegments(0)
    , m_isPaused(false)
    , m_isCanceled(false)
    , m_isFinished(false)
{
    // 从父对象（AsulMultiDownloader）获取共享的网络管理器
    AsulMultiDownloader *downloader = qobject_cast<AsulMultiDownloader*>(parent);
    if (downloader) {
        m_networkManager = downloader->getNetworkManager();
        m_ownsNetworkManager = false;
    } else {
        // 如果无法获取，创建自己的（兜底方案）
        m_networkManager = new QNetworkAccessManager(this);
        m_ownsNetworkManager = true;
    }
}

DownloadTask::~DownloadTask()
{
    cancel();

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    // 注意：不需要显式释放网络管理器
    // - 如果是从池中借用的（m_ownsNetworkManager == false），池会一直持有这些管理器
    // - 如果是自己创建的（m_ownsNetworkManager == true），Qt的父子关系会自动删除
}

void DownloadTask::start()
{
    QMutexLocker locker(&m_mutex);

    if (m_isPaused || m_isCanceled) {
        return;
    }

    // === 清理上一次尝试的残留状态（重试安全）===
    if (m_reply) {
        m_reply->disconnect();  // 先断开所有信号，防止abort()同步触发finished信号导致死锁
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    for (auto segment : m_segments) {
        segment->cancel();
        segment->deleteLater();
    }
    m_segments.clear();
    m_segmentProgress.clear();
    m_completedSegments = 0;

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    m_downloadedSize = 0;
    m_errorString.clear();
    // === 清理完成 ===

    // 确保保存目录存在
    QFileInfo fileInfo(m_savePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    emit started(m_taskId);

    // 优化：如果已知文件大小且小于分段阈值，直接单线程下载，跳过HEAD请求
    AsulMultiDownloader *downloader = qobject_cast<AsulMultiDownloader*>(parent());
    if (m_fileSize > 0 && downloader && m_fileSize <= downloader->m_largeFileThreshold) {
        // 小文件，已知大小，无需HEAD探测，直接下载
        m_supportRange = false;
        m_segmentCount = 1;
        locker.unlock();
        startSingleDownload();
        return;
    }

    // 大文件或未知时发送HEAD请求获取文件大小和是否支持Range
    QNetworkRequest request(m_url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(m_timeout);

    m_reply = m_networkManager->head(request);
    // 注意：只连接 finished 信号。不连接 errorOccurred，避免 error+finished 双重触发
    // onHeadFinished 内部已检查 m_reply->error() 来处理错误
    connect(m_reply, &QNetworkReply::finished, this, &DownloadTask::onHeadFinished);
}

void DownloadTask::pause()
{
    QMutexLocker locker(&m_mutex);

    m_isPaused = true;

    if (m_reply) {
        m_reply->disconnect();  // 先断开信号，防止abort()同步触发finished信号导致死锁
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    // 取消所有分段下载
    for (auto segment : m_segments) {
        segment->cancel();
        segment->deleteLater();
    }
    m_segments.clear();

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    emit paused(m_taskId);
}

void DownloadTask::resume()
{
    QMutexLocker locker(&m_mutex);

    m_isPaused = false;

    // 重新开始下载
    locker.unlock();
    start();
}

void DownloadTask::cancel()
{
    QMutexLocker locker(&m_mutex);

    m_isCanceled = true;

    if (m_reply) {
        m_reply->disconnect();  // 先断开信号，防止abort()同步触发finished信号导致死锁
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    // 取消所有分段下载
    for (auto segment : m_segments) {
        segment->cancel();
        segment->deleteLater();
    }
    m_segments.clear();

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

void DownloadTask::resetForRetry()
{
    QMutexLocker locker(&m_mutex);

    m_isCanceled = false;
    m_isPaused = false;
    m_isFinished = false;
    m_errorString.clear();
    m_downloadedSize = 0;
    m_completedSegments = 0;
    m_segmentProgress.clear();

    // 清理网络请求和文件
    if (m_reply) {
        m_reply->disconnect();
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    for (auto segment : m_segments) {
        segment->cancel();
        segment->deleteLater();
    }
    m_segments.clear();

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    // 删除已下载的部分文件，避免文件损坏
    QFile::remove(m_savePath);
    for (int i = 0; i < m_segmentCount; ++i) {
        QFile::remove(m_savePath + QString(".part%1").arg(i));
    }
}

void DownloadTask::onHeadFinished()
{
    if (!m_reply) {
        return;
    }

    QMutexLocker locker(&m_mutex);

    if (m_reply->error() != QNetworkReply::NoError) {
        m_errorString = m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;
        locker.unlock();
        emit failed(m_taskId, m_errorString);
        return;
    }

    // 获取文件大小
    m_fileSize = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();

    // 检查是否支持Range
    QString acceptRanges = m_reply->rawHeader("Accept-Ranges");
    m_supportRange = (acceptRanges.toLower() == "bytes");

    m_reply->deleteLater();
    m_reply = nullptr;

    // 根据文件大小决定下载策略
    AsulMultiDownloader *downloader = qobject_cast<AsulMultiDownloader*>(parent());

    // PCL优化：检查域名策略
    bool disableMultiThread = downloader && downloader->shouldDisableMultiThread(m_url);

    if (downloader && m_fileSize > downloader->largeFileThreshold() && m_supportRange && !disableMultiThread) {
        // 大文件，使用分段下载（除非域名在禁用列表中）
        locker.unlock();
        startSegmentedDownload();
    } else {
        // 小文件、不支持Range或域名禁用多线程，使用单线程下载
        m_segmentCount = 1;
        locker.unlock();
        startSingleDownload();
    }
}

void DownloadTask::startSingleDownload()
{
    QMutexLocker locker(&m_mutex);

    if (m_isPaused || m_isCanceled) {
        return;
    }

    // 打开文件，使用 Truncate 确保从头开始写入
    m_file = new QFile(m_savePath);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_errorString = QString("Cannot open file: %1").arg(m_savePath);
        delete m_file;
        m_file = nullptr;
        locker.unlock();
        emit failed(m_taskId, m_errorString);
        return;
    }

    // 开始下载
    QNetworkRequest request(m_url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(m_timeout);

    m_reply = m_networkManager->get(request);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &DownloadTask::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &DownloadTask::onDownloadFinished);
    // 注意：不连接 errorOccurred，避免 error+finished 双重触发导致回复指针错乱
    // onDownloadFinished 内部已检查 m_reply->error() 来处理错误
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_file && m_reply) {
            m_file->write(m_reply->readAll());
        }
    });
}

void DownloadTask::startSegmentedDownload()
{
    QMutexLocker locker(&m_mutex);

    if (m_isPaused || m_isCanceled) {
        return;
    }

    if (m_fileSize <= 0) {
        // 文件大小未知，回退到单线程下载
        locker.unlock();
        startSingleDownload();
        return;
    }

    // 计算每个分段的大小
    qint64 segmentSize = m_fileSize / m_segmentCount;
    m_segmentProgress.resize(m_segmentCount);
    m_segmentProgress.fill(0);

    // 创建分段下载器
    for (int i = 0; i < m_segmentCount; ++i) {
        qint64 start = i * segmentSize;
        qint64 end = (i == m_segmentCount - 1) ? m_fileSize - 1 : (start + segmentSize - 1);

        QString segmentPath = m_savePath + QString(".part%1").arg(i);

        auto segment = new SegmentDownloader(i, m_url, segmentPath, start, end, m_timeout, this);
        connect(segment, &SegmentDownloader::finished, this, &DownloadTask::onSegmentFinished);
        connect(segment, &SegmentDownloader::error, this, &DownloadTask::onSegmentError);
        connect(segment, &SegmentDownloader::progress, this, &DownloadTask::onSegmentProgress);

        m_segments.append(segment);
    }

    // 启动所有分段下载
    for (auto segment : m_segments) {
        segment->start();
    }
}

void DownloadTask::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    QMutexLocker locker(&m_mutex);
    m_downloadedSize = bytesReceived;

    if (bytesTotal > 0) {
        m_fileSize = bytesTotal;
    }

    locker.unlock();
    emit progress(m_taskId, bytesReceived, bytesTotal);
}

void DownloadTask::onDownloadFinished()
{
    if (!m_reply) {
        return;
    }

    QMutexLocker locker(&m_mutex);

    if (m_reply->error() != QNetworkReply::NoError) {
        m_errorString = m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;

        if (m_file) {
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }

        locker.unlock();
        emit failed(m_taskId, m_errorString);
        return;
    }

    // 写入剩余数据
    if (m_file && m_reply) {
        m_file->write(m_reply->readAll());
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    m_reply->deleteLater();
    m_reply = nullptr;

    // 修复：任务完成时用实际文件大小更新downloadedSize，确保统计准确
    if (m_fileSize > 0) {
        m_downloadedSize = m_fileSize;
    } else {
        // 文件大小未知时，读取实际写入的文件大小
        QFileInfo fi(m_savePath);
        if (fi.exists()) {
            m_downloadedSize = fi.size();
            m_fileSize = fi.size();
        }
    }

    m_isFinished = true;

    locker.unlock();
    emit finished(m_taskId);
}

void DownloadTask::onDownloadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);

    if (!m_reply) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_errorString = m_reply->errorString();

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    locker.unlock();
    emit failed(m_taskId, m_errorString);
}

void DownloadTask::onSegmentFinished(int segmentIndex)
{
    QMutexLocker locker(&m_mutex);

    m_completedSegments++;

    if (m_completedSegments == m_segmentCount) {
        // 所有分段下载完成，合并文件
        locker.unlock();
        mergeSegments();
    }
}

void DownloadTask::onSegmentError(int segmentIndex, const QString &error)
{
    QMutexLocker locker(&m_mutex);

    m_errorString = QString("Segment %1 download failed: %2").arg(segmentIndex).arg(error);

    // 取消所有其他分段
    for (auto segment : m_segments) {
        segment->cancel();
    }

    locker.unlock();
    emit failed(m_taskId, m_errorString);
}

void DownloadTask::onSegmentProgress(int segmentIndex, qint64 bytesReceived, qint64 bytesTotal)
{
    QMutexLocker locker(&m_mutex);

    if (segmentIndex >= 0 && segmentIndex < m_segmentProgress.size()) {
        m_segmentProgress[segmentIndex] = bytesReceived;
    }

    // 计算总进度
    qint64 totalReceived = 0;
    for (qint64 progress : m_segmentProgress) {
        totalReceived += progress;
    }

    m_downloadedSize = totalReceived;

    locker.unlock();
    emit progress(m_taskId, totalReceived, m_fileSize);
}

void DownloadTask::mergeSegments()
{
    QMutexLocker locker(&m_mutex);

    // 打开目标文件
    QFile outFile(m_savePath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        m_errorString = QString("Cannot create target file: %1").arg(m_savePath);
        locker.unlock();
        emit failed(m_taskId, m_errorString);
        return;
    }

    // 依次读取并写入每个分段文件
    for (int i = 0; i < m_segmentCount; ++i) {
        QString segmentPath = m_savePath + QString(".part%1").arg(i);
        QFile segmentFile(segmentPath);

        if (!segmentFile.open(QIODevice::ReadOnly)) {
            m_errorString = QString("Cannot open segment file: %1").arg(segmentPath);
            outFile.close();
            locker.unlock();
            emit failed(m_taskId, m_errorString);
            return;
        }

        // 读取并写入数据
        while (!segmentFile.atEnd()) {
            QByteArray data = segmentFile.read(8192);
            outFile.write(data);
        }

        segmentFile.close();
        segmentFile.remove();  // 删除分段文件
    }

    outFile.close();

    // 修复：分段下载完成时也更新downloadedSize
    if (m_fileSize > 0) {
        m_downloadedSize = m_fileSize;
    }

    m_isFinished = true;

    locker.unlock();
    emit finished(m_taskId);
}

// ==================== SegmentDownloader 实现 ====================

SegmentDownloader::SegmentDownloader(int index, const QUrl &url, const QString &filePath,
                                   qint64 start, qint64 end, int timeout, QObject *parent)
    : QObject(parent)
    , m_index(index)
    , m_url(url)
    , m_filePath(filePath)
    , m_start(start)
    , m_end(end)
    , m_bytesReceived(0)
    , m_timeout(timeout)
    , m_networkManager(nullptr)
    , m_reply(nullptr)
    , m_file(nullptr)
    , m_ownsNetworkManager(false)
    , m_isCanceled(false)
{
    // 尝试从DownloadTask的父对象（AsulMultiDownloader）获取共享的网络管理器
    DownloadTask *task = qobject_cast<DownloadTask*>(parent);
    if (task) {
        AsulMultiDownloader *downloader = qobject_cast<AsulMultiDownloader*>(task->parent());
        if (downloader) {
            m_networkManager = downloader->getNetworkManager();
            m_ownsNetworkManager = false;
        }
    }

    // 如果无法获取，创建自己的（兜底方案）
    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
        m_ownsNetworkManager = true;
    }
}

SegmentDownloader::~SegmentDownloader()
{
    cancel();

    // 注意：不需要显式释放网络管理器
    // - 如果是从池中借用的（m_ownsNetworkManager == false），池会一直持有这些管理器
    // - 如果是自己创建的（m_ownsNetworkManager == true），Qt的父子关系会自动删除
}

void SegmentDownloader::start()
{
    if (m_isCanceled) {
        return;
    }

    // 打开文件，使用 Truncate 确保从头开始写入
    m_file = new QFile(m_filePath);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QString error = QString("Cannot open file: %1").arg(m_filePath);
        delete m_file;
        m_file = nullptr;
        emit this->error(m_index, error);
        return;
    }

    // 创建请求，设置Range头
    QNetworkRequest request(m_url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(m_timeout);
    request.setRawHeader("Range", QString("bytes=%1-%2").arg(m_start).arg(m_end).toUtf8());

    m_reply = m_networkManager->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &SegmentDownloader::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &SegmentDownloader::onFinished);
    // 注意：不连接 errorOccurred，避免 error+finished 双重触发
    // onFinished 内部已检查 m_reply->error() 来处理错误
}

void SegmentDownloader::cancel()
{
    m_isCanceled = true;

    if (m_reply) {
        m_reply->disconnect();  // 先断开信号，防止abort()同步触发finished信号导致死锁
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

void SegmentDownloader::onReadyRead()
{
    if (!m_file || !m_reply) {
        return;
    }

    QByteArray data = m_reply->readAll();
    m_file->write(data);
    m_bytesReceived += data.size();

    qint64 total = m_end - m_start + 1;
    emit progress(m_index, m_bytesReceived, total);
}

void SegmentDownloader::onFinished()
{
    if (!m_reply) {
        return;
    }

    if (m_reply->error() != QNetworkReply::NoError) {
        QString errorString = m_reply->errorString();

        if (m_file) {
            m_file->close();
            delete m_file;
            m_file = nullptr;
            QFile::remove(m_filePath);
        }

    m_reply->deleteLater();
        m_reply = nullptr;

        emit error(m_index, errorString);
        return;
    }

    // 写入剩余数据
    if (m_file && m_reply) {
        QByteArray data = m_reply->readAll();
        m_file->write(data);
        m_bytesReceived += data.size();
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    m_reply->deleteLater();
    m_reply = nullptr;

    emit finished(m_index);
}

void SegmentDownloader::onError(QNetworkReply::NetworkError errorCode)
{
    Q_UNUSED(errorCode);

    if (!m_reply) {
        return;
    }

    QString errorString = m_reply->errorString();

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
        QFile::remove(m_filePath);
    }

    emit error(m_index, errorString);
}

// ==================== PCL优化：监控和动态调度实现 ====================

void AsulMultiDownloader::onMonitorDownloads()
{
    QMutexLocker locker(&m_mutex);

    if (!m_speedMonitoringEnabled) {
        return;
    }

    // === 卡住任务检测 ===
    // 如果有 Downloading 状态的任务超过 stallTimeoutMs 无进度更新，强制取消并重试
    const qint64 stallTimeoutMs = 60000;  // 60秒无进度即判定为卡住
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList stalledTasks;
    for (auto it = m_taskStatus.begin(); it != m_taskStatus.end(); ++it) {
        if (it.value() == DownloadStatus::Downloading) {
            const QString &taskId = it.key();
            qint64 lastActive = m_taskLastProgress.value(taskId, 0);
            if (lastActive == 0) {
                // 首次记录
                m_taskLastProgress[taskId] = now;
            } else if (now - lastActive > stallTimeoutMs) {
                stalledTasks.append(taskId);
            }
        }
    }

    for (const QString &taskId : stalledTasks) {
        if (!m_tasks.contains(taskId)) continue;
        auto task = m_tasks[taskId];

        // 检查任务是否真的还在下载中（防止竞态条件：任务可能在检测期间完成）
        if (task->isFinished()) {
            m_taskLastProgress.remove(taskId);
            continue;
        }

        qDebug() << QString("[STALL] Task %1 stalled for >%2s, forcing retry: %3")
                    .arg(taskId).arg(stallTimeoutMs / 1000).arg(task->url().toString());

        // 强制取消当前网络请求并重置状态以允许重试
        task->resetForRetry();
        updateHostConnections(task->url().host(), -1);
        m_activeDownloads--;
        m_taskLastProgress.remove(taskId);

        // 重试或标记失败
        if (m_taskRetryCount[taskId] < m_maxRetryCount) {
            m_taskRetryCount[taskId]++;
            m_taskStatus[taskId] = DownloadStatus::Queued;
            m_taskQueue.enqueue(taskId);
        } else {
            m_taskStatus[taskId] = DownloadStatus::Failed;
            m_statistics.failedTasks++;
            emit downloadFailed(taskId, QString("Task stalled: no progress for %1 seconds").arg(stallTimeoutMs / 1000));
            checkAndEmitAllFinished();
        }
    }

    // 尝试处理队列中的任务
    if (m_activeDownloads < m_maxConcurrentDownloads && !m_taskQueue.isEmpty()) {
        processQueue();
    }
}

qint64 AsulMultiDownloader::calculateCurrentSpeed()
{
    // 计算从上次检查到现在的平均速度
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 currentBytes = m_statistics.totalDownloaded;

    if (m_monitorLastTime == 0) {
        m_monitorLastTime = currentTime;
        m_monitorLastBytes = currentBytes;
        return 0;
    }

    qint64 timeDiff = currentTime - m_monitorLastTime;
    if (timeDiff < 100) {  // 至少间隔100ms
        return 0;
    }

    qint64 bytesDiff = currentBytes - m_monitorLastBytes;
    if (bytesDiff < 0) bytesDiff = 0; // 防止回绕

    qint64 speed = (bytesDiff * 1000) / timeDiff;  // 字节/秒

    // 更新上次检查的值（每秒更新一次）
    if (timeDiff >= 1000) {
        m_monitorLastTime = currentTime;
        m_monitorLastBytes = currentBytes;
    }

    return speed;
}

bool AsulMultiDownloader::shouldDisableMultiThread(const QUrl &url) const
{
    QString host = url.host().toLower();

    // 检查是否在禁用多线程的域名列表中
    for (const QString &pattern : m_noMultiThreadHosts) {
        if (host.contains(pattern.toLower())) {
            return true;
        }
    }

    return false;
}

QNetworkAccessManager* AsulMultiDownloader::getNetworkManager()
{
    // Fix: Remove mutex lock to avoid recursive locking deadlock
    // m_networkManagers is read-only after constructor, so this is safe

    if (m_networkManagers.isEmpty()) {
        return nullptr;
    }

    // 使用thread-safe的轮询分配
    // 原子计数器确保线程安全，modulo防止溢出
    static std::atomic<int> index{0};
    int currentIndex = index.fetch_add(1) % m_networkManagers.size();

    return m_networkManagers[currentIndex];
}

void AsulMultiDownloader::checkAndEmitAllFinished()
{
    // 注意：调用此方法时应已持有锁

    // 检查是否所有任务都完成了（包括成功和失败的任务）
    bool allFinished = true;
    for (auto status : m_taskStatus) {
        if (status == DownloadStatus::Queued || status == DownloadStatus::Downloading) {
            allFinished = false;
            break;
        }
    }

    if (allFinished && m_taskQueue.isEmpty() && !m_allFinishedEmitted) {
        m_allFinishedEmitted = true;

        // Stop timers to allow event loop to exit
        if (m_statisticsTimer) {
            m_statisticsTimer->stop();
        }
        if (m_monitorTimer) {
            m_monitorTimer->stop();
        }

        emit allDownloadsFinished();
    }
}
