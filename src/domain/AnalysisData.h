#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include "pointcloud/PointCloudData.h"

enum class AnalysisSeverity
{
    None = 0,
    Advisory,
    Warning,
    Critical
};

QString analysisSeverityDisplayName(AnalysisSeverity severity);

struct VegetationRiskRecord
{
    QString id;
    QString title;
    AnalysisSeverity severity = AnalysisSeverity::None;
    PointRecord point;
    float representativeChainage = 0.0f;
    float chainageStart = 0.0f;
    float chainageEnd = 0.0f;
    float minimumDistance = 0.0f;
    float averageDistance = 0.0f;
    int supportPointCount = 0;
    int nearestSegmentIndex = -1;
    int nearestTowerIndex = -1;
    QString nearestTowerName;
    QString sourceRule;
    QString notes;
};

QJsonObject vegetationRiskRecordToJson(const VegetationRiskRecord& record);
VegetationRiskRecord vegetationRiskRecordFromJson(const QJsonObject& object);

