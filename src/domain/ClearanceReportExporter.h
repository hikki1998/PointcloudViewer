#pragma once

#include <QString>

#include "domain/ClearanceAnalysis.h"
#include "domain/RuleBasedClearanceEngine.h"

class ClearanceReportExporter
{
public:
    static bool exportSegmentsCsv(
        const QString& filePath,
        const ClearanceAnalysisResult& analysisResult,
        const ClearanceRuleEvaluationResult* ruleEvaluation = nullptr,
        QString* errorMessage = nullptr);
};
