#pragma once

#include <QString>

#include "domain/InspectionData.h"

class InspectionReportExporter
{
public:
    static bool exportIssuesCsv(
        const QString& filePath,
        const QList<InspectionIssue>& issues,
        QString* errorMessage = nullptr);

    static bool exportProjectHtml(
        const QString& filePath,
        const QString& projectName,
        const QStringList& pointCloudPaths,
        const QList<TowerRecord>& towers,
        const QList<InspectionIssue>& issues,
        QString* errorMessage = nullptr);
};
