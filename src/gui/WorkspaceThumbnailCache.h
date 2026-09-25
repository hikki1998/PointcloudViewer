#pragma once

#include <QImage>
#include <QString>

class WorkspaceThumbnailCache
{
public:
    enum class Kind
    {
        Project,
        Data
    };

    static QImage imageFor(const QString& filePath, Kind kind);
    static bool save(const QString& filePath, Kind kind, const QImage& source);
    static QImage placeholder(const QString& filePath, Kind kind, const QSize& size = QSize(320, 180));

private:
    static QString cacheFilePath(const QString& filePath, Kind kind);
};
