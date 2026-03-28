#pragma once

#include <QString>

#include "domain/ClearanceAnalysis.h"

class ClearanceReportExporter
{
public:
    static bool exportSegmentsCsv(
        const QString& filePath,
        const ClearanceAnalysisResult& analysisResult,
        QString* errorMessage = nullptr);
};
