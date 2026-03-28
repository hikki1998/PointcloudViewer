#pragma once

#include <QList>

#include "domain/AnalysisData.h"
#include "domain/ClearanceAnalysis.h"
#include "domain/InspectionData.h"
#include "domain/RuleBasedClearanceEngine.h"
#include "pointcloud/PointCloudData.h"

struct VegetationRiskAnalysisParameters
{
    float searchRadius = 20.0f;
    float criticalThreshold = 10.0f;
    float clusterGap = 8.0f;
    int minimumClusterPoints = 3;
    bool preferVegetationClassification = true;
    ClearanceRulePreset preset = ClearanceRulePreset::TransmissionCorridor;
};

struct VegetationRiskAnalysisResult
{
    QList<VegetationRiskRecord> records;
    int candidatePointCount = 0;
    bool usedVegetationClassification = false;
    float advisoryThreshold = 0.0f;
    float warningThreshold = 0.0f;
    float criticalThreshold = 0.0f;
};

VegetationRiskAnalysisResult analyzeVegetationRisks(
    const PointCloudData& pointCloudData,
    const ClearanceAnalysisResult& pathAnalysis,
    const QList<TowerRecord>& towers,
    const VegetationRiskAnalysisParameters& parameters);

