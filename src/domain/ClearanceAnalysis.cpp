#include "domain/ClearanceAnalysis.h"

#include <algorithm>
#include <cmath>

namespace
{
float segmentHorizontalDistance(const PointRecord& startPoint, const PointRecord& endPoint)
{
    const double dx = static_cast<double>(endPoint.x) - static_cast<double>(startPoint.x);
    const double dy = static_cast<double>(endPoint.y) - static_cast<double>(startPoint.y);
    return static_cast<float>(std::sqrt(dx * dx + dy * dy));
}

float segmentDistance3d(const PointRecord& startPoint, const PointRecord& endPoint)
{
    const double dx = static_cast<double>(endPoint.x) - static_cast<double>(startPoint.x);
    const double dy = static_cast<double>(endPoint.y) - static_cast<double>(startPoint.y);
    const double dz = static_cast<double>(endPoint.z) - static_cast<double>(startPoint.z);
    return static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
}
}

ClearanceAnalysisResult analyzeClearancePath(const QList<PointRecord>& points, float threshold)
{
    ClearanceAnalysisResult result;
    result.threshold = std::max(0.0f, threshold);
    if (points.isEmpty()) {
        return result;
    }

    result.minimumElevation = points.constFirst().z;
    result.maximumElevation = points.constFirst().z;
    result.profilePoints.reserve(points.size());
    result.segments.reserve(std::max(0, points.size() - 1));

    float chainage = 0.0f;
    for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
        const PointRecord& point = points.at(pointIndex);

        ClearanceProfilePoint profilePoint;
        profilePoint.pointIndex = pointIndex;
        profilePoint.chainage = chainage;
        profilePoint.point = point;
        result.profilePoints.append(profilePoint);

        result.minimumElevation = std::min(result.minimumElevation, point.z);
        result.maximumElevation = std::max(result.maximumElevation, point.z);

        if (pointIndex == 0) {
            continue;
        }

        const PointRecord& previousPoint = points.at(pointIndex - 1);
        const float horizontalDistance = segmentHorizontalDistance(previousPoint, point);
        const float distance3d = segmentDistance3d(previousPoint, point);
        const float deltaZ = point.z - previousPoint.z;

        ClearanceSegment segment;
        segment.startPointIndex = pointIndex - 1;
        segment.endPointIndex = pointIndex;
        segment.chainageStart = chainage;
        segment.chainageEnd = chainage + horizontalDistance;
        segment.horizontalDistance = horizontalDistance;
        segment.distance3d = distance3d;
        segment.deltaZ = deltaZ;
        segment.belowThreshold = result.thresholdEnabled() && distance3d < result.threshold;

        result.segments.append(segment);
        result.totalHorizontalDistance += horizontalDistance;
        result.totalDistance3d += distance3d;
        if (segment.belowThreshold) {
            ++result.warningCount;
        }

        if (result.segments.size() == 1) {
            result.minimumSegmentDistance = distance3d;
            result.maximumSegmentDistance = distance3d;
        } else {
            result.minimumSegmentDistance = std::min(result.minimumSegmentDistance, distance3d);
            result.maximumSegmentDistance = std::max(result.maximumSegmentDistance, distance3d);
        }

        chainage += horizontalDistance;
        result.profilePoints.last().chainage = chainage;
    }

    if (result.profilePoints.size() >= 2) {
        result.deltaZ = result.profilePoints.constLast().point.z - result.profilePoints.constFirst().point.z;
    }

    return result;
}
