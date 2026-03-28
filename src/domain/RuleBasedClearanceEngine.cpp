#include "domain/RuleBasedClearanceEngine.h"

#include <QObject>

#include <algorithm>

namespace
{
struct PresetParameters
{
    float advisoryMultiplier = 2.0f;
    float warningMultiplier = 1.5f;
    ClearanceRulePreset preset = ClearanceRulePreset::TransmissionCorridor;
};

PresetParameters presetParametersFor(ClearanceRulePreset preset)
{
    switch (preset) {
    case ClearanceRulePreset::DistributionCorridor:
        return { 1.7f, 1.3f, ClearanceRulePreset::DistributionCorridor };
    case ClearanceRulePreset::StructureApproach:
        return { 1.45f, 1.2f, ClearanceRulePreset::StructureApproach };
    case ClearanceRulePreset::Custom:
        return { 2.0f, 1.5f, ClearanceRulePreset::Custom };
    case ClearanceRulePreset::TransmissionCorridor:
    default:
        return { 2.1f, 1.55f, ClearanceRulePreset::TransmissionCorridor };
    }
}

QString reasonContextText(ClearanceRulePreset preset)
{
    switch (preset) {
    case ClearanceRulePreset::DistributionCorridor:
        return QObject::tr("distribution corridor");
    case ClearanceRulePreset::StructureApproach:
        return QObject::tr("tower structure approach");
    case ClearanceRulePreset::Custom:
        return QObject::tr("custom threshold rule");
    case ClearanceRulePreset::TransmissionCorridor:
    default:
        return QObject::tr("transmission corridor");
    }
}
}

QString clearanceRulePresetDisplayName(ClearanceRulePreset preset)
{
    switch (preset) {
    case ClearanceRulePreset::Custom:
        return QObject::tr("Custom");
    case ClearanceRulePreset::DistributionCorridor:
        return QObject::tr("Distribution Corridor");
    case ClearanceRulePreset::StructureApproach:
        return QObject::tr("Structure Approach");
    case ClearanceRulePreset::TransmissionCorridor:
    default:
        return QObject::tr("Transmission Corridor");
    }
}

ClearanceRuleEvaluationResult evaluateClearanceRules(
    const ClearanceAnalysisResult& analysisResult,
    const ClearanceRuleSettings& settings)
{
    ClearanceRuleEvaluationResult result;
    result.preset = settings.preset;
    result.presetLabel = clearanceRulePresetDisplayName(settings.preset);
    result.criticalThreshold = std::max(0.0f, settings.criticalThreshold);

    if (!analysisResult.isValid() || result.criticalThreshold <= 0.0f) {
        return result;
    }

    const PresetParameters parameters = presetParametersFor(settings.preset);
    result.warningThreshold = result.criticalThreshold * parameters.warningMultiplier;
    result.advisoryThreshold = result.criticalThreshold * parameters.advisoryMultiplier;
    result.segmentEvaluations.reserve(analysisResult.segments.size());
    const QString reasonContext = reasonContextText(parameters.preset);

    for (const ClearanceSegment& segment : analysisResult.segments) {
        ClearanceSegmentEvaluation evaluation;
        const float measuredDistance = segment.distance3d;
        if (measuredDistance <= result.criticalThreshold) {
            evaluation.severity = AnalysisSeverity::Critical;
            evaluation.severityLabel = analysisSeverityDisplayName(evaluation.severity);
            evaluation.reason = QObject::tr("Segment is below the critical %1 rule threshold.")
                .arg(reasonContext);
            ++result.criticalCount;
        } else if (measuredDistance <= result.warningThreshold) {
            evaluation.severity = AnalysisSeverity::Warning;
            evaluation.severityLabel = analysisSeverityDisplayName(evaluation.severity);
            evaluation.reason = QObject::tr("Segment is within the warning band for %1.")
                .arg(reasonContext);
            ++result.warningCount;
        } else if (measuredDistance <= result.advisoryThreshold) {
            evaluation.severity = AnalysisSeverity::Advisory;
            evaluation.severityLabel = analysisSeverityDisplayName(evaluation.severity);
            evaluation.reason = QObject::tr("Segment should be watched under the %1 rule.")
                .arg(reasonContext);
            ++result.advisoryCount;
        } else {
            evaluation.severity = AnalysisSeverity::None;
            evaluation.severityLabel = analysisSeverityDisplayName(evaluation.severity);
            evaluation.reason = QObject::tr("Segment is comfortably outside the %1 risk bands.")
                .arg(reasonContext);
        }

        result.segmentEvaluations.append(evaluation);
    }

    return result;
}
