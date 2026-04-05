#pragma once

#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

namespace lasviewer::logging
{

enum class LogLevel
{
    Info,
    Warning,
    Error
};

struct LogEntry
{
    qint64 timestampMs = 0;
    LogLevel level = LogLevel::Info;
    QString module;
    QString message;
};

class ApplicationLogger final : public QObject
{
    Q_OBJECT

public:
    static ApplicationLogger& instance();

    void log(LogLevel level, const QString& module, const QString& message);
    QVector<LogEntry> entries() const;
    void clear();
    void setMaxEntries(int maxEntries);
    int maxEntries() const;
    bool exportEntries(const QString& filePath, bool asCsv, QString* errorMessage = nullptr) const;

signals:
    void entryAdded();
    void entriesCleared();

private:
    ApplicationLogger();

    void appendToDailyFile(const LogEntry& entry) const;
    static QString levelToken(LogLevel level);

    mutable QMutex mutex_;
    QVector<LogEntry> entries_;
    int maxEntries_ = 2000;
};

} // namespace lasviewer::logging
