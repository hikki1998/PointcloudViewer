#pragma once

#include <QString>

class PointCloudData;

class LasReader
{
public:
    LasReader() = default;

    bool read(const QString& filePath, PointCloudData* output, QString* errorMessage = nullptr) const;
};
