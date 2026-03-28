#include "domain/ClearanceReportExporter.h"

#include <QFile>
#include <QTextStream>

namespace
{
QString csvCell(const QString& value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}
}

bool ClearanceReportExporter::exportSegmentsCsv(
    const QString& filePath,
    const ClearanceAnalysisResult& analysisResult,
    const ClearanceRuleEvaluationResult* ruleEvaluation,
    QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Failed to create the clearance CSV file.");
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << "Segment,From Point,To Point,Chainage Start (m),Chainage End (m),Horizontal Distance (m),3D Distance (m),Delta Z (m),Severity,Reason,Start X,Start Y,Start Z,End X,End Y,End Z\n";

    for (int segmentIndex = 0; segmentIndex < analysisResult.segments.size(); ++segmentIndex) {
        const ClearanceSegment& segment = analysisResult.segments.at(segmentIndex);
        const ClearanceProfilePoint& startPoint = analysisResult.profilePoints.at(segment.startPointIndex);
        const ClearanceProfilePoint& endPoint = analysisResult.profilePoints.at(segment.endPointIndex);
        const ClearanceSegmentEvaluation evaluation = ruleEvaluation != nullptr && segmentIndex < ruleEvaluation->segmentEvaluations.size()
            ? ruleEvaluation->segmentEvaluations.at(segmentIndex)
            : ClearanceSegmentEvaluation();

        stream
            << csvCell(QString::number(segmentIndex + 1)) << ','
            << csvCell(QString::number(segment.startPointIndex + 1)) << ','
            << csvCell(QString::number(segment.endPointIndex + 1)) << ','
            << csvCell(QString::number(segment.chainageStart, 'f', 3)) << ','
            << csvCell(QString::number(segment.chainageEnd, 'f', 3)) << ','
            << csvCell(QString::number(segment.horizontalDistance, 'f', 3)) << ','
            << csvCell(QString::number(segment.distance3d, 'f', 3)) << ','
            << csvCell(QString::number(segment.deltaZ, 'f', 3)) << ','
            << csvCell(evaluation.severityLabel) << ','
            << csvCell(evaluation.reason) << ','
            << csvCell(QString::number(startPoint.point.x, 'f', 3)) << ','
            << csvCell(QString::number(startPoint.point.y, 'f', 3)) << ','
            << csvCell(QString::number(startPoint.point.z, 'f', 3)) << ','
            << csvCell(QString::number(endPoint.point.x, 'f', 3)) << ','
            << csvCell(QString::number(endPoint.point.y, 'f', 3)) << ','
            << csvCell(QString::number(endPoint.point.z, 'f', 3))
            << '\n';
    }

    return true;
}
