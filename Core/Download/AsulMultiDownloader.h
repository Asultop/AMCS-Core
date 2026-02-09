#ifndef ASULMULTIDOWNLOADER_H
#define ASULMULTIDOWNLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QQueue>
#include <QHash>
#include <QMutex>
#include <QThread>
#include <QThreadPool>
#include <QRunnable>
#include <QUrl>
#include <QTimer>
#include <QDateTime>
#include <memory>

// 前向声明
class DownloadTask;
class SegmentDownloader;

/**
 * @brief 下载任务信息结构
 */
struct DownloadInfo {
    QString taskId;              // 任务唯一标识
    QUrl url;                    // 下载URL
    QString savePath;            // 保存路径
    qint64 fileSize;             // 文件总大小（-1表示未知）
    qint64 downloadedSize;       // 已下载大小
    QDateTime startTime;         // 开始时间
    QDateTime endTime;           // 结束时间
    int priority;                // 优先级（数字越大优先级越高）
    bool supportRange;           // 是否支持断点续传
    int segmentCount;            // 分段数量
    QString errorString;         // 错误信息

    DownloadInfo()
        : fileSize(-1), downloadedSize(0), priority(0),
          supportRange(false), segmentCount(1) {}
};

/**
 * @brief 下载任务状态枚举
 */
enum class DownloadStatus {
    Queued,          // 队列中等待
    Downloading,     // 下载中
    Paused,          // 已暂停
    Completed,       // 已完成
    Failed,          // 失败
    Canceled         // 已取消
};

/**
 * @brief 下载统计信息
 */
struct DownloadStatistics {
    int activeDownloads;         // 活动下载数
    int queuedDownloads;         // 队列中的下载数
    qint64 totalDownloadSpeed;   // 总下载速度（字节/秒）
    qint64 totalDownloaded;      // 总已下载字节数
    int completedTasks;          // 已完成任务数
    int failedTasks;             // 失败任务数

    DownloadStatistics()
        : activeDownloads(0), queuedDownloads(0), totalDownloadSpeed(0),
          totalDownloaded(0), completedTasks(0), failedTasks(0) {}
};

/**
 * @brief AsulMultiDownloader - 高性能多线程下载管理器
 *
 * 支持大文件分段下载、小文件并发下载，智能线程调度
 * 面向Qt 6.7.3标准开发
 */
class AsulMultiDownloader : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit AsulMultiDownloader(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~AsulMultiDownloader();

    // ==================== 配置接口 ====================

    /**
     * @brief 设置最大并发下载线程数
     * @param count 线程数（默认8）
     */
    void setMaxConcurrentDownloads(int count);

    /**
     * @brief 获取最大并发下载线程数
     * @return 当前设置的最大线程数
     */
    int maxConcurrentDownloads() const;

    /**
     * @brief 设置大文件阈值（超过此大小将启用分段下载）
     * @param bytes 字节数（默认10MB）
     */
    void setLargeFileThreshold(qint64 bytes);

    /**
     * @brief 获取大文件阈值
     * @return 当前阈值（字节）
     */
    qint64 largeFileThreshold() const;

    /**
     * @brief 设置大文件分段数
     * @param count 分段数（默认4）
     */
    void setSegmentCountForLargeFile(int count);

    /**
     * @brief 获取大文件分段数
     * @return 当前分段数
     */
    int segmentCountForLargeFile() const;

    /**
     * @brief 设置每个Host的最大连接数
     * @param count 连接数（默认6）
     */
    void setMaxConnectionsPerHost(int count);

    /**
     * @brief 获取每个Host的最大连接数
     * @return 当前最大连接数
     */
    int maxConnectionsPerHost() const;

    /**
     * @brief 设置下载超时时间
     * @param msecs 毫秒数（默认15000）
     */
    void setDownloadTimeout(int msecs);

    /**
     * @brief 获取下载超时时间
     * @return 当前超时时间（毫秒）
     */
    int downloadTimeout() const;

    /**
     * @brief 设置是否启用自动重试
     * @param enable 是否启用（默认true）
     */
    void setAutoRetry(bool enable);

    /**
     * @brief 获取是否启用自动重试
     * @return 当前设置
     */
    bool autoRetry() const;

    /**
     * @brief 设置最大重试次数
     * @param count 重试次数（默认5）
     */
    void setMaxRetryCount(int count);

    /**
     * @brief 获取最大重试次数
     * @return 当前重试次数
     */
    int maxRetryCount() const;

    /**
     * @brief 设置速度监控阈值（PCL优化）
     * @param bytesPerSecond 速度阈值，低于此速度时尝试增加线程（默认256KB/s）
     */
    void setSpeedThreshold(qint64 bytesPerSecond);

    /**
     * @brief 获取速度监控阈值
     * @return 当前速度阈值（字节/秒）
     */
    qint64 speedThreshold() const;

    /**
     * @brief 设置是否启用速度监控和动态线程调度（PCL优化）
     * @param enable 是否启用（默认true）
     */
    void setSpeedMonitoringEnabled(bool enable);

    /**
     * @brief 获取是否启用速度监控
     * @return 当前设置
     */
    bool speedMonitoringEnabled() const;

    /**
     * @brief 添加禁用多线程的域名（PCL优化）
     * @param host 域名（如 "github.com", "modrinth.com"）
     */
    void addNoMultiThreadHost(const QString &host);

    /**
     * @brief 移除禁用多线程的域名
     * @param host 域名
     */
    void removeNoMultiThreadHost(const QString &host);

    /**
     * @brief 清空禁用多线程的域名列表
     */
    void clearNoMultiThreadHosts();

    /**
     * @brief 获取禁用多线程的域名列表
     * @return 域名列表
     */
    QStringList noMultiThreadHosts() const;

    // ==================== 下载控制接口 ====================

    /**
     * @brief 添加下载任务
     * @param url 下载URL
     * @param savePath 保存路径
     * @param priority 优先级（默认0）
     * @return 任务ID
     */
    QString addDownload(const QUrl &url, const QString &savePath, int priority = 0, qint64 knownFileSize = -1);

    /**
     * @brief 批量添加下载任务
     * @param urls URL列表
     * @param savePaths 保存路径列表
     * @param priority 优先级（默认0）
     * @return 任务ID列表
     */
    QStringList addDownloads(const QList<QUrl> &urls, const QStringList &savePaths, int priority = 0);

    /**
     * @brief 暂停下载任务
     * @param taskId 任务ID
     */
    void pauseDownload(const QString &taskId);

    /**
     * @brief 恢复下载任务
     * @param taskId 任务ID
     */
    void resumeDownload(const QString &taskId);

    /**
     * @brief 取消下载任务
     * @param taskId 任务ID
     */
    void cancelDownload(const QString &taskId);

    /**
     * @brief 暂停所有下载
     */
    void pauseAll();

    /**
     * @brief 恢复所有下载
     */
    void resumeAll();

    /**
     * @brief 取消所有下载
     */
    void cancelAll();

    /**
     * @brief 清除已完成和失败的任务
     */
    void clearFinishedTasks();

    // ==================== 查询接口 ====================

    /**
     * @brief 获取任务信息
     * @param taskId 任务ID
     * @return 下载信息
     */
    DownloadInfo getDownloadInfo(const QString &taskId) const;

    /**
     * @brief 获取任务状态
     * @param taskId 任务ID
     * @return 下载状态
     */
    DownloadStatus getDownloadStatus(const QString &taskId) const;

    /**
     * @brief 获取任务下载进度
     * @param taskId 任务ID
     * @return 进度百分比（0-100）
     */
    double getDownloadProgress(const QString &taskId) const;

    /**
     * @brief 获取任务下载速度
     * @param taskId 任务ID
     * @return 速度（字节/秒）
     */
    qint64 getDownloadSpeed(const QString &taskId) const;

    /**
     * @brief 获取所有任务ID列表
     * @return 任务ID列表
     */
    QStringList getAllTaskIds() const;

    /**
     * @brief 获取统计信息
     * @return 下载统计信息
     */
    DownloadStatistics getStatistics() const;

signals:
    // ==================== 任务相关信号 ====================

    /**
     * @brief 任务添加信号
     * @param taskId 任务ID
     * @param url 下载URL
     */
    void downloadAdded(const QString &taskId, const QUrl &url);

    /**
     * @brief 任务开始信号
     * @param taskId 任务ID
     */
    void downloadStarted(const QString &taskId);

    /**
     * @brief 任务进度更新信号
     * @param taskId 任务ID
     * @param bytesReceived 已接收字节数
     * @param bytesTotal 总字节数
     */
    void downloadProgress(const QString &taskId, qint64 bytesReceived, qint64 bytesTotal);

    /**
     * @brief 任务速度更新信号
     * @param taskId 任务ID
     * @param bytesPerSecond 下载速度（字节/秒）
     */
    void downloadSpeedChanged(const QString &taskId, qint64 bytesPerSecond);

    /**
     * @brief 任务暂停信号
     * @param taskId 任务ID
     */
    void downloadPaused(const QString &taskId);

    /**
     * @brief 任务恢复信号
     * @param taskId 任务ID
     */
    void downloadResumed(const QString &taskId);

    /**
     * @brief 任务完成信号
     * @param taskId 任务ID
     * @param savePath 保存路径
     */
    void downloadFinished(const QString &taskId, const QString &savePath);

    /**
     * @brief 任务失败信号
     * @param taskId 任务ID
     * @param errorString 错误信息
     */
    void downloadFailed(const QString &taskId, const QString &errorString);

    /**
     * @brief 任务取消信号
     * @param taskId 任务ID
     */
    void downloadCanceled(const QString &taskId);

    /**
     * @brief 任务重试信号
     * @param taskId 任务ID
     * @param retryCount 当前重试次数
     */
    void downloadRetrying(const QString &taskId, int retryCount);

    // ==================== 全局统计信号 ====================

    /**
     * @brief 统计信息更新信号
     * @param stats 统计信息
     */
    void statisticsChanged(const DownloadStatistics &stats);

    /**
     * @brief 所有任务完成信号
     */
    void allDownloadsFinished();

private slots:
    void onTaskFinished(const QString &taskId);
    void onTaskFailed(const QString &taskId, const QString &error);
    void onTaskProgress(const QString &taskId, qint64 received, qint64 total);
    void onUpdateStatistics();
    void onMonitorDownloads();  // 新增：监控线程，参考PCL

    // Allow DownloadTask to access private methods
    friend class DownloadTask;
    friend class SegmentDownloader;

private:
    // 内部方法
    void processQueue();
    void startDownloadTask(std::shared_ptr<DownloadTask> task);
    QString generateTaskId();
    void updateHostConnections(const QString &host, int delta);
    int getHostConnections(const QString &host) const;
    bool canStartDownload(const QString &host) const;
    bool shouldDisableMultiThread(const QUrl &url) const;  // 新增：域名策略检查
    qint64 calculateCurrentSpeed();  // 新增：计算当前速度
    void checkAndEmitAllFinished();  // 检查是否所有任务完成并发射信号
    QNetworkAccessManager* getNetworkManager();  // 获取共享的网络管理器

    // 配置参数
    int m_maxConcurrentDownloads;
    qint64 m_largeFileThreshold;
    int m_segmentCount;
    int m_maxConnectionsPerHost;
    int m_downloadTimeout;
    bool m_autoRetry;
    int m_maxRetryCount;

    // PCL优化参数
    bool m_speedMonitoringEnabled;       // 速度监控开关
    qint64 m_speedThreshold;             // 速度阈值（256KB/s）
    qint64 m_lastSpeedCheck;             // 上次速度检查时间（用于UI统计）
    qint64 m_lastBytesDownloaded;        // 上次下载字节数（用于UI统计）

    // 独立的监控线程速度计算状态
    qint64 m_monitorLastTime;            // 上次监控检查时间
    qint64 m_monitorLastBytes;           // 上次监控下载字节数

    QStringList m_noMultiThreadHosts;    // 禁用多线程的域名列表

    // 任务管理
    QHash<QString, std::shared_ptr<DownloadTask>> m_tasks;
    QQueue<QString> m_taskQueue;
    QHash<QString, DownloadStatus> m_taskStatus;
    QHash<QString, int> m_taskRetryCount;
    QHash<QString, qint64> m_taskLastProgress;  // taskId -> 最后进度更新时间戳（用于卡住检测）

    // 线程和网络管理
    QHash<QString, int> m_hostConnections;  // Host -> 当前连接数
    int m_activeDownloads;
    QList<QNetworkAccessManager*> m_networkManagers;  // 共享的网络管理器池
    int m_networkManagerPoolSize;                      // 网络管理器池大小

    // 统计信息
    DownloadStatistics m_statistics;
    QTimer *m_statisticsTimer;
    QTimer *m_monitorTimer;              // 新增：监控定时器
    bool m_allFinishedEmitted;           // 防止重复发射完成信号

    // 线程安全
    mutable QMutex m_mutex;

    // 任务ID计数器
    quint64 m_taskIdCounter;
};

// ==================== 内部类定义 ====================

/**
 * @brief 下载任务类（内部使用）
 */
class DownloadTask : public QObject
{
    Q_OBJECT

public:
    explicit DownloadTask(const QString &taskId, const QUrl &url,
                         const QString &savePath, int priority, QObject *parent = nullptr);
    ~DownloadTask();

    void start();
    void pause();
    void resume();
    void cancel();
    void resetForRetry();  // 新增：重置状态以允许重试（用于STALL检测）

    QString taskId() const { return m_taskId; }
    QUrl url() const { return m_url; }
    QString savePath() const { return m_savePath; }
    int priority() const { return m_priority; }
    qint64 fileSize() const { return m_fileSize; }
    qint64 downloadedSize() const { return m_downloadedSize; }
    bool supportRange() const { return m_supportRange; }
    int segmentCount() const { return m_segmentCount; }
    QString errorString() const { return m_errorString; }
    bool isFinished() const { return m_isFinished; }

    void setSegmentCount(int count) { m_segmentCount = count; }
    void setTimeout(int msecs) { m_timeout = msecs; }
    void setKnownFileSize(qint64 size) { m_fileSize = size; }

signals:
    void started(const QString &taskId);
    void progress(const QString &taskId, qint64 bytesReceived, qint64 bytesTotal);
    void finished(const QString &taskId);
    void failed(const QString &taskId, const QString &error);
    void paused(const QString &taskId);
    void resumed(const QString &taskId);

private slots:
    void onHeadFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadError(QNetworkReply::NetworkError error);
    void onSegmentFinished(int segmentIndex);
    void onSegmentError(int segmentIndex, const QString &error);
    void onSegmentProgress(int segmentIndex, qint64 bytesReceived, qint64 bytesTotal);

private:
    void startSingleDownload();
    void startSegmentedDownload();
    void mergeSegments();

    QString m_taskId;
    QUrl m_url;
    QString m_savePath;
    int m_priority;
    int m_timeout;

    qint64 m_fileSize;
    qint64 m_downloadedSize;
    bool m_supportRange;
    int m_segmentCount;
    QString m_errorString;

    QNetworkAccessManager *m_networkManager;  // 从池中借用的网络管理器
    QNetworkReply *m_reply;
    QFile *m_file;
    bool m_ownsNetworkManager;  // 是否拥有网络管理器（需要释放）

    // 分段下载相关
    QList<SegmentDownloader*> m_segments;
    QList<qint64> m_segmentProgress;
    int m_completedSegments;

    bool m_isPaused;
    bool m_isCanceled;
    bool m_isFinished;

    QMutex m_mutex;

    friend class AsulMultiDownloader;  // 允许管理器访问私有成员（卡住检测等）
};

/**
 * @brief 分段下载器类（内部使用）
 */
class SegmentDownloader : public QObject
{
    Q_OBJECT

public:
    explicit SegmentDownloader(int index, const QUrl &url, const QString &filePath,
                              qint64 start, qint64 end, int timeout, QObject *parent = nullptr);
    ~SegmentDownloader();

    void start();
    void cancel();
    int index() const { return m_index; }
    qint64 bytesReceived() const { return m_bytesReceived; }

signals:
    void finished(int index);
    void error(int index, const QString &errorString);
    void progress(int index, qint64 bytesReceived, qint64 bytesTotal);

private slots:
    void onReadyRead();
    void onFinished();
    void onError(QNetworkReply::NetworkError error);

private:
    int m_index;
    QUrl m_url;
    QString m_filePath;
    qint64 m_start;
    qint64 m_end;
    qint64 m_bytesReceived;
    int m_timeout;

    QNetworkAccessManager *m_networkManager;  // 从池中借用的网络管理器
    QNetworkReply *m_reply;
    QFile *m_file;
    bool m_ownsNetworkManager;  // 是否拥有网络管理器（需要释放）

    bool m_isCanceled;
};

namespace AMCS::Core::Download
{
using ::AsulMultiDownloader;
using ::DownloadInfo;
using ::DownloadStatistics;
using ::DownloadStatus;
} // namespace AMCS::Core::Download

#endif // ASULMULTIDOWNLOADER_H
