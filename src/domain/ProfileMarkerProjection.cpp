#include "domain/ProfileMarkerProjection.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
struct ProjectionCandidate
{
    float chainage = 0.0f;
    float lateralDistance = std::numeric_limits<float>::max();
};

ProjectionCandidate projectToProfilePath(
    const ClearanceAnalysisResult& analysisResult,
    const PointRecord& point)
{
    ProjectionCandidate bestCandidate;
    if (!analysisResult.isValid()) {
        return bestCandidate;
    }

    for (const ClearanceSegment& segment : analysisResult.segments) {
        const PointRecord& startPoint = analysisResult.profilePoints.at(segment.startPointIndex).point;
        const PointRecord& endPoint = analysisResult.profilePoints.at(segment.endPointIndex).point;
        const double dx = static_cast<double>(endPoint.x) - static_cast<double>(startPoint.x);
        const double dy = static_cast<double>(endPoint.y) - static_cast<double>(startPoint.y);
        const double segmentLengthSquared = dx * dx + dy * dy;

        double interpolation = 0.0;
        if (segmentLengthSquared > 1e-8) {
            const double px = static_cast<double>(point.x) - static_cast<double>(startPoint.x);
            const double py = static_cast<double>(point.y) - static_cast<double>(startPoint.y);
            interpolation = std::clamp((px * dx + py * dy) / segmentLengthSquared, 0.0, 1.0);
        }

        const double projectedX = static_cast<double>(startPoint.x) + dx * interpolation;
        const double projectedY = static_cast<double>(startPoint.y) + dy * interpolation;
        const double offsetX = static_cast<double>(point.x) - projectedX;
        const double offsetY = static_cast<double>(point.y) - projectedY;
        const float lateralDistance = static_cast<float>(std::sqrt(offsetX * offsetX + offsetY * offsetY));

        if (lateralDistance >= bestCandidate.lateralDistance) {
            continue;
        }

        bestCandidate.lateralDistance = lateralDistance;
        bestCandidate.chainage = segment.chainageStart + static_cast<float>(interpolation) * segment.horizontalDistance;
    }

    return bestCandidate;
}

float defaultProjectionTolerance(const ClearanceAnalysisResult& analysisResult)
{
    if (!analysisResult.isValid()) {
        return 0.0f;
    }

    return std::clamp(analysisResult.totalHorizontalDistance * 0.08f, 8.0f, 60.0f);
}
}

QList<ProjectedProfileMarker> projectProfileMarkers(
    const ClearanceAnalysisResult& analysisResult,
    const QList<TowerRecord>& towers,
    int selectedTowerIndex,
    const QList<InspectionIssue>& issues,
    int selectedIssueIndex,
    float maxLateralDistance)
{
    QList<ProjectedProfileMarker> markers;
    if (!analysisResult.isValid()) {
        return markers;
    }

    const float tolerance = maxLateralDistance > 0.0f
        ? maxLateralDistance
        : defaultProjectionTolerance(analysisResult);

    markers.reserve(towers.size() + issues.size());

    for (int towerIndex = 0; towerIndex < towers.size(); ++towerIndex) {
        const TowerRecord& tower = towers.at(towerIndex);
        const ProjectionCandidate projection = projectToProfilePath(analysisResult, tower.point);
        const bool isSelected = towerIndex == selectedTowerIndex;
        if (projection.lateralDistance > tolerance && !isSelected) {
            continue;
        }

        ProjectedProfileMarker marker;
        marker.kind = ProfileMarkerKind::Tower;
        marker.sourceIndex = towerIndex;
        marker.chainage = projection.chainage;
        marker.elevation = tower.point.z;
        marker.lateralDistance = projection.lateralDistance;
        marker.title = tower.name;
        marker.selected = isSelected;
        markers.append(marker);
    }

    for (int issueIndex = 0; issueIndex < issues.size(); ++issueIndex) {
        const InspectionIssue& issue = issues.at(issueIndex);
        const ProjectionCandidate projection = projectToProfilePath(analysisResult, issue.point);
        const bool isSelected = issueIndex == selectedIssueIndex;
        if (projection.lateralDistance > tolerance && !isSelected) {
            continue;
        }

        ProjectedProfileMarker marker;
        marker.kind = ProfileMarkerKind::Issue;
        marker.sourceIndex = issueIndex;
        marker.chainage = projection.chainage;
        marker.elevation = issue.point.z;
        marker.lateralDistance = projection.lateralDistance;
        marker.title = issue.title;
        marker.selected = isSelected;
        markers.append(marker);
    }

    std::sort(markers.begin(), markers.end(), [](const ProjectedProfileMarker& left, const ProjectedProfileMarker& right) {
        if (std::fabs(left.chainage - right.chainage) > 0.001f) {
            return left.chainage < right.chainage;
        }
        if (left.kind != right.kind) {
            return static_cast<int>(left.kind) < static_cast<int>(right.kind);
        }
        return left.sourceIndex < right.sourceIndex;
    });

    return markers;
}
