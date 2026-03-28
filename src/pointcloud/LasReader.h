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

    bool read(
        const QString& filePath,
        PointCloudData* output,
        QString* errorMessage = nullptr,
        LasFileMetadata* metadata = nullptr,
        ProgressCallback progressCallback = ProgressCallback()) const;

    bool readPreview(
        const QString& filePath,
        PointCloudData* output,
        std::size_t targetPreviewPoints = 160000,
        std::size_t maxScanPoints = 320000,
        QString* errorMessage = nullptr) const;
};
