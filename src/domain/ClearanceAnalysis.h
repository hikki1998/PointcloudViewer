#pragma once

#include <QList>

#include "pointcloud/PointCloudData.h"

struct ClearanceProfilePoint
{
    int pointIndex = -1;
    float chainage = 0.0f;
    PointRecord point;
};

struct ClearanceSegment
{
    int startPointIndex = -1;
    int endPointIndex = -1;
    float chainageStart = 0.0f;
    float chainageEnd = 0.0f;
    float horizontalDistance = 0.0f;
    float distance3d = 0.0f;
    float deltaZ = 0.0f;
    bool belowThreshold = false;
};

struct ClearanceAnalysisResult
{
    QList<ClearanceProfilePoint> profilePoints;
    QList<ClearanceSegment> segments;
    float totalHorizontalDistance = 0.0f;
    float totalDistance3d = 0.0f;
    float deltaZ = 0.0f;
    float minimumSegmentDistance = 0.0f;
    float maximumSegmentDistance = 0.0f;
    float minimumElevation = 0.0f;
    float maximumElevation = 0.0f;
    float threshold = 0.0f;
    int warningCount = 0;

    [[nodiscard]] bool isValid() const
    {
        return profilePoints.size() >= 2;
    }

    [[nodiscard]] bool thresholdEnabled() const
    {
        return threshold > 0.0f;
    }

    [[nodiscard]] bool hasWarnings() const
    {
        return warningCount > 0;
    }
};

ClearanceAnalysisResult analyzeClearancePath(const QList<PointRecord>& points, float threshold);
