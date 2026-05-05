#pragma once

#include "pointcloud/PointCloudData.h"

#include <QString>

class LasWriter
{
public:
    bool write(
        const QString& filePath,
        const PointCloudData& data,
        QString* errorMessage = nullptr) const;
};
