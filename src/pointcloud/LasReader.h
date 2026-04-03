#pragma once

#include <cstddef>
#include <functional>

#include <QString>

#include "pointcloud/PointCloudData.h"

struct LasFileMetadata
{
    std::size_t pointCount = 0;
    PointRecord minBounds;
    PointRecord maxBounds;
    bool hasColor = false;
    bool hasIntensity = false;
    bool hasClassification = false;
    bool hasReturnInfo = false;
    bool hasGpsTime = false;
    QString projectionText;
};

struct LasReadProgress
{
    std::size_t pointsRead = 0;
    std::size_t totalPoints = 0;
};

class LasReader
{
public:
    LasReader() = default;

    using ProgressCallback = std::function<void(const LasReadProgress&)>;
    using PointCallback = std::function<void(const PointRecord&)>;
    using CancellationCallback = std::function<bool()>;

    bool readMetadata(
        const QString& filePath,
        LasFileMetadata* metadata,
        QString* errorMessage = nullptr) const;

    bool read(
        const QString& filePath,
        PointCloudData* output,
        QString* errorMessage = nullptr,
        LasFileMetadata* metadata = nullptr,
        ProgressCallback progressCallback = ProgressCallback(),
        CancellationCallback cancellationCallback = CancellationCallback()) const;

    bool readPoints(
        const QString& filePath,
        PointCallback pointCallback,
        QString* errorMessage = nullptr,
        LasFileMetadata* metadata = nullptr,
        ProgressCallback progressCallback = ProgressCallback(),
        CancellationCallback cancellationCallback = CancellationCallback()) const;

    bool readPreview(
        const QString& filePath,
        PointCloudData* output,
        std::size_t targetPreviewPoints = 160000,
        std::size_t maxScanPoints = 320000,
        QString* errorMessage = nullptr,
        CancellationCallback cancellationCallback = CancellationCallback()) const;
};
