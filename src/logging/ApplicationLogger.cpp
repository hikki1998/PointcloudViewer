#include "logging/ApplicationLogger.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

#include <algorithm>

namespace lasviewer::logging
{
namespace
{
constexpr qint64 kMaxDailyLogFileBytes = 5LL * 1024LL * 1024LL;
constexpr int kMaxDailyLogArchiveCount = 5;

QString csvEscape(const QString& text)
{
    QString escaped = text;
    escaped.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString rotatedLogPath(const QString& basePath, int index)
{
    return QStringLiteral("%1.%2").arg(basePath).arg(index);
}

void rotateDailyLogIfNeeded(const QString& basePath)
{
    const QFileInfo fileInfo(basePath);
    if (!fileInfo.exists() || fileInfo.size() < kMaxDailyLogFileBytes) {
        return;
    }

    const QString oldestArchivePath = rotatedLogPath(basePath, kMaxDailyLogArchiveCount);
    if (QFile::exists(oldestArchivePath)) {
        QFile::remove(oldestArchivePath);
    }

    for (int index = kMaxDailyLogArchiveCount - 1; index >= 1; --index) {
        const QString sourcePath = rotatedLogPath(basePath, index);
        if (!QFile::exists(sourcePath)) {
            continue;
        }

        const QString targetPath = rotatedLogPath(basePath, index + 1);
        QFile::remove(targetPath);
        QFile::rename(sourcePath, targetPath);
    }

    const QString firstArchivePath = rotatedLogPath(basePath, 1);
    QFile::remove(firstArchivePath);
    QFile::rename(basePath, firstArchivePath);
}
}

ApplicationLogger& ApplicationLogger::instance()
{
    static ApplicationLogger logger;
    return logger;
}

ApplicationLogger::ApplicationLogger()
    : QObject(nullptr)
{
}

void ApplicationLogger::log(LogLevel level, const QString& module, const QString& message)
{
    QString normalizedMessage = message;
    normalizedMessage.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalizedMessage.replace(QStringLiteral("\r"), QStringLiteral("\n"));
    if (normalizedMessage.trimmed().isEmpty()) {
        return;
    }

    const QString normalizedModule = module.trimmed().isEmpty()
        ? QStringLiteral("APP")
        : module.trimmed();

    LogEntry entry;
    entry.timestampMs = QDateTime::currentMSecsSinceEpoch();
    entry.level = level;
    entry.module = normalizedModule;
    entry.message = normalizedMessage;

    {
        QMutexLocker locker(&mutex_);
        entries_.append(entry);
        if (entries_.size() > maxEntries_) {
            const int dropCount = entries_.size() - maxEntries_;
            entries_.remove(0, dropCount);
        }
    }

    appendToDailyFile(entry);
    emit entryAdded();
}

QVector<LogEntry> ApplicationLogger::entries() const
{
    QMutexLocker locker(&mutex_);
    return entries_;
}

void ApplicationLogger::clear()
{
    {
        QMutexLocker locker(&mutex_);
        entries_.clear();
    }

    emit entriesCleared();
}

void ApplicationLogger::setMaxEntries(int maxEntries)
{
    const int sanitizedMaxEntries = std::max(100, maxEntries);

    QMutexLocker locker(&mutex_);
    maxEntries_ = sanitizedMaxEntries;
    if (entries_.size() > maxEntries_) {
        const int dropCount = entries_.size() - maxEntries_;
        entries_.remove(0, dropCount);
    }
}

int ApplicationLogger::maxEntries() const
{
    QMutexLocker locker(&mutex_);
    return maxEntries_;
}

bool ApplicationLogger::exportEntries(const QString& filePath, bool asCsv, QString* errorMessage) const
{
    const QVector<LogEntry> snapshot = entries();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    if (asCsv) {
        stream << QStringLiteral("timestamp,level,module,message\n");
        for (const LogEntry& entry : snapshot) {
            const QString timestamp = QDateTime::fromMSecsSinceEpoch(entry.timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
            stream << csvEscape(timestamp)
                   << ','
                   << csvEscape(levelToken(entry.level))
                   << ','
                   << csvEscape(entry.module)
                   << ','
                   << csvEscape(entry.message)
                   << '\n';
        }
    } else {
        for (const LogEntry& entry : snapshot) {
            const QString timestamp = QDateTime::fromMSecsSinceEpoch(entry.timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
            QString normalizedMessage = entry.message;
            stream << '[' << timestamp << "] [" << levelToken(entry.level) << "] [" << entry.module << "] "
                   << normalizedMessage.replace('\n', QStringLiteral("\\n")) << '\n';
        }
    }

    file.close();
    return true;
}

void ApplicationLogger::appendToDailyFile(const LogEntry& entry) const
{
    const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (appDataRoot.isEmpty()) {
        return;
    }

    QDir appDataDir(appDataRoot);
    if (!appDataDir.mkpath(QStringLiteral("logs"))) {
        return;
    }

    const QString filePath = appDataDir.filePath(
        QStringLiteral("logs/%1.log").arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));

    rotateDailyLogIfNeeded(filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        return;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    const QString timestamp = QDateTime::fromMSecsSinceEpoch(entry.timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    stream << '[' << timestamp << "] [" << levelToken(entry.level) << "] [" << entry.module << "] "
           << QString(entry.message).replace('\n', QStringLiteral("\\n")) << '\n';
}

QString ApplicationLogger::levelToken(LogLevel level)
{
    switch (level) {
    case LogLevel::Warning:
        return QStringLiteral("WARN");
    case LogLevel::Error:
        return QStringLiteral("ERROR");
    case LogLevel::Info:
    default:
        return QStringLiteral("INFO");
    }
}

} // namespace lasviewer::logging
