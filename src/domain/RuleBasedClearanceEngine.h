#pragma once

#include <QList>

#include "domain/AnalysisData.h"
#include "domain/ClearanceAnalysis.h"

enum class ClearanceRulePreset
{
    Custom = 0,
    TransmissionCorridor,
    DistributionCorridor,
    StructureApproach
};

struct ClearanceRuleSettings
{
    ClearanceRulePreset preset = ClearanceRulePreset::TransmissionCorridor;
    float criticalThreshold = 10.0f;
};

struct ClearanceSegmentEvaluation
{
    AnalysisSeverity severity = AnalysisSeverity::None;
    QString severityLabel;
    QString reason;
};

struct ClearanceRuleEvaluationResult
{
    ClearanceRulePreset preset = ClearanceRulePreset::TransmissionCorridor;
    QString presetLabel;
    QList<ClearanceSegmentEvaluation> segmentEvaluations;
    float advisoryThreshold = 0.0f;
    float warningThreshold = 0.0f;
    float criticalThreshold = 0.0f;
    int advisoryCount = 0;
    int warningCount = 0;
    int criticalCount = 0;

    [[nodiscard]] bool enabled() const
    {
        return criticalThreshold > 0.0f;
    }
};

QString clearanceRulePresetDisplayName(ClearanceRulePreset preset);
ClearanceRuleEvaluationResult evaluateClearanceRules(
    const ClearanceAnalysisResult& analysisResult,
    const ClearanceRuleSettings& settings);

