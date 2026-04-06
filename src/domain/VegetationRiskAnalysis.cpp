#include "domain/VegetationRiskAnalysis.h"

#include <QDateTime>
#include <QObject>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
struct SegmentProjection
{
    int segmentIndex = -1;
    float chainage = 0.0f;
    float horizontalDistance = 0.0f;
    float distance3d = 0.0f;
};

struct CandidatePoint
{
    PointRecord point;
    SegmentProjection projection;
};

bool isVegetationClassification(const PointRecord& point)
{
    if (!point.hasClassification) {
        return false;
    }

    return point.classification == 3
        || point.classification == 4
        || point.classification == 5
        || point.classification == 13;
}

SegmentProjection projectPointToPath(const PointRecord& point, const ClearanceAnalysisResult& pathAnalysis)
{
    SegmentProjection bestProjection;
    bestProjection.horizontalDistance = std::numeric_limits<float>::max();
    bestProjection.distance3d = std::numeric_limits<float>::max();

    for (int segmentIndex = 0; segmentIndex < pathAnalysis.segments.size(); ++segmentIndex) {
        const ClearanceSegment& segment = pathAnalysis.segments.at(segmentIndex);
        const PointRecord& startPoint = pathAnalysis.profilePoints.at(segment.startPointIndex).point;
        const PointRecord& endPoint = pathAnalysis.profilePoints.at(segment.endPointIndex).point;
        const double vx = static_cast<double>(endPoint.x) - static_cast<double>(startPoint.x);
        const double vy = static_cast<double>(endPoint.y) - static_cast<double>(startPoint.y);
        const double vz = static_cast<double>(endPoint.z) - static_cast<double>(startPoint.z);
        const double wx = static_cast<double>(point.x) - static_cast<double>(startPoint.x);
        const double wy = static_cast<double>(point.y) - static_cast<double>(startPoint.y);
        const double wz = static_cast<double>(point.z) - static_cast<double>(startPoint.z);
        const double lengthSquared = vx * vx + vy * vy + vz * vz;
        const double clampedT = lengthSquared <= 0.0
            ? 0.0
            : std::clamp((wx * vx + wy * vy + wz * vz) / lengthSquared, 0.0, 1.0);

        const double projectedX = static_cast<double>(startPoint.x) + vx * clampedT;
        const double projectedY = static_cast<double>(startPoint.y) + vy * clampedT;
        const double projectedZ = static_cast<double>(startPoint.z) + vz * clampedT;
        const double dx = static_cast<double>(point.x) - projectedX;
        const double dy = static_cast<double>(point.y) - projectedY;
        const double dz = static_cast<double>(point.z) - projectedZ;
        const float distance3d = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
        const float horizontalDistance = static_cast<float>(std::sqrt(dx * dx + dy * dy));

        if (distance3d >= bestProjection.distance3d) {
            continue;
        }

        bestProjection.segmentIndex = segmentIndex;
        bestProjection.chainage = segment.chainageStart + segment.horizontalDistance * static_cast<float>(clampedT);
        bestProjection.horizontalDistance = horizontalDistance;
        bestProjection.distance3d = distance3d;
    }

    return bestProjection;
}

int nearestTowerIndexForPoint(const PointRecord& point, const QList<TowerRecord>& towers)
{
    int bestIndex = -1;
    double bestDistanceSquared = std::numeric_limits<double>::max();
    for (int towerIndex = 0; towerIndex < towers.size(); ++towerIndex) {
        const PointRecord& towerPoint = towers.at(towerIndex).point;
        const double dx = static_cast<double>(point.x) - static_cast<double>(towerPoint.x);
        const double dy = static_cast<double>(point.y) - static_cast<double>(towerPoint.y);
        const double dz = static_cast<double>(point.z) - static_cast<double>(towerPoint.z);
        const double distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestIndex = towerIndex;
        }
    }
    return bestIndex;
}
}

VegetationRiskAnalysisResult analyzeVegetationRisks(
    const PointCloudData& pointCloudData,
    const ClearanceAnalysisResult& pathAnalysis,
    const QList<TowerRecord>& towers,
    const VegetationRiskAnalysisParameters& parameters)
{
    VegetationRiskAnalysisResult result;
    if (pointCloudData.empty() || !pathAnalysis.isValid()) {
        return result;
    }

    const ClearanceRuleEvaluationResult clearanceRules = evaluateClearanceRules(
        pathAnalysis,
        { parameters.preset, parameters.criticalThreshold });
    result.advisoryThreshold = clearanceRules.advisoryThreshold;
    result.warningThreshold = clearanceRules.warningThreshold;
    result.criticalThreshold = clearanceRules.criticalThreshold;
    if (!clearanceRules.enabled()) {
        return result;
    }

    QList<CandidatePoint> candidates;
    candidates.reserve(static_cast<int>(pointCloudData.size() / 8));

    const bool canUseClassification = parameters.preferVegetationClassification && pointCloudData.hasClassification();
    result.usedVegetationClassification = canUseClassification;

    for (const PointRecord& point : pointCloudData.points()) {
        if (canUseClassification && !isVegetationClassification(point)) {
            continue;
        }

        const SegmentProjection projection = projectPointToPath(point, pathAnalysis);
        if (projection.segmentIndex < 0) {
            continue;
        }
        if (projection.horizontalDistance > parameters.searchRadius || projection.distance3d > clearanceRules.advisoryThreshold) {
            continue;
        }

        CandidatePoint candidate;
        candidate.point = point;
        candidate.projection = projection;
        candidates.append(candidate);
    }

    result.candidatePointCount = candidates.size();
    std::sort(candidates.begin(), candidates.end(), [](const CandidatePoint& left, const CandidatePoint& right) {
        if (left.projection.segmentIndex != right.projection.segmentIndex) {
            return left.projection.segmentIndex < right.projection.segmentIndex;
        }
        return left.projection.chainage < right.projection.chainage;
    });

    QList<CandidatePoint> cluster;
    const auto flushCluster = [&]() {
        if (cluster.size() < parameters.minimumClusterPoints) {
            cluster.clear();
            return;
        }

        VegetationRiskRecord record;
        record.id = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddhhmmsszzz"))
            + QStringLiteral("_%1").arg(result.records.size() + 1);
        record.title = QObject::tr("Vegetation Risk %1").arg(result.records.size() + 1);
        record.sourceRule = clearanceRulePresetDisplayName(parameters.preset);
        record.chainageStart = cluster.constFirst().projection.chainage;
        record.chainageEnd = cluster.constLast().projection.chainage;
        record.representativeChainage = (record.chainageStart + record.chainageEnd) * 0.5f;
        record.minimumDistance = cluster.constFirst().projection.distance3d;
        record.nearestSegmentIndex = cluster.constFirst().projection.segmentIndex;
        record.supportPointCount = cluster.size();

        double sumX = 0.0;
        double sumY = 0.0;
        double sumZ = 0.0;
        double sumDistance = 0.0;
        for (const CandidatePoint& candidate : cluster) {
            sumX += candidate.point.x;
            sumY += candidate.point.y;
            sumZ += candidate.point.z;
            sumDistance += candidate.projection.distance3d;
            if (candidate.projection.distance3d < record.minimumDistance) {
                record.minimumDistance = candidate.projection.distance3d;
                record.nearestSegmentIndex = candidate.projection.segmentIndex;
            }
        }

        record.point.x = sumX / cluster.size();
        record.point.y = sumY / cluster.size();
        record.point.z = sumZ / cluster.size();
        record.averageDistance = static_cast<float>(sumDistance / cluster.size());

        if (record.minimumDistance <= clearanceRules.criticalThreshold) {
            record.severity = AnalysisSeverity::Critical;
            record.notes = QObject::tr("Cluster is inside the critical vegetation clearance distance.");
        } else if (record.minimumDistance <= clearanceRules.warningThreshold) {
            record.severity = AnalysisSeverity::Warning;
            record.notes = QObject::tr("Cluster is inside the warning vegetation clearance band.");
        } else {
            record.severity = AnalysisSeverity::Advisory;
            record.notes = QObject::tr("Cluster should be monitored as it approaches the advisory clearance band.");
        }

        record.nearestTowerIndex = nearestTowerIndexForPoint(record.point, towers);
        if (record.nearestTowerIndex >= 0 && record.nearestTowerIndex < towers.size()) {
            record.nearestTowerName = towers.at(record.nearestTowerIndex).name;
        }

        result.records.append(record);
        cluster.clear();
    };

    for (const CandidatePoint& candidate : candidates) {
        if (cluster.isEmpty()) {
            cluster.append(candidate);
            continue;
        }

        const CandidatePoint& previous = cluster.constLast();
        const bool newCluster = candidate.projection.segmentIndex != previous.projection.segmentIndex
            || (candidate.projection.chainage - previous.projection.chainage) > parameters.clusterGap;
        if (newCluster) {
            flushCluster();
        }
        cluster.append(candidate);
    }
    flushCluster();

    return result;
}
