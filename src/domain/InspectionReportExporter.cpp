#include "domain/InspectionReportExporter.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace
{
QString csvCell(const QString& value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString htmlEscaped(const QString& value)
{
    QString escaped = value.toHtmlEscaped();
    escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    return escaped;
}
}

bool InspectionReportExporter::exportIssuesCsv(
    const QString& filePath,
    const QList<InspectionIssue>& issues,
    QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Failed to create the issue CSV file.");
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << "Index,Title,Category,Severity,Status,Related Tower,Created At,X,Y,Z,Image Path,Description\n";

    for (int issueIndex = 0; issueIndex < issues.size(); ++issueIndex) {
        const InspectionIssue& issue = issues.at(issueIndex);
        stream
            << csvCell(QString::number(issueIndex + 1)) << ','
            << csvCell(issue.title) << ','
            << csvCell(issue.category) << ','
            << csvCell(issueSeverityDisplayName(issue.severity)) << ','
            << csvCell(issueStatusDisplayName(issue.status)) << ','
            << csvCell(issue.relatedTowerName) << ','
            << csvCell(issue.createdAt) << ','
            << csvCell(QString::number(issue.point.x, 'f', 3)) << ','
            << csvCell(QString::number(issue.point.y, 'f', 3)) << ','
            << csvCell(QString::number(issue.point.z, 'f', 3)) << ','
            << csvCell(issue.imagePath) << ','
            << csvCell(issue.description)
            << '\n';
    }

    return true;
}

bool InspectionReportExporter::exportProjectHtml(
    const QString& filePath,
    const QString& projectName,
    const QStringList& pointCloudPaths,
    const QList<TowerRecord>& towers,
    const QList<InspectionIssue>& issues,
    QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Failed to create the HTML report file.");
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    stream
        << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"/>"
        << "<title>" << htmlEscaped(projectName) << "</title>"
        << "<style>"
        << "body{font-family:'Segoe UI',sans-serif;background:#f8fafc;color:#0f172a;margin:28px;}"
        << "h1,h2{margin:0 0 12px;}section{background:#fff;border:1px solid #dbe4ee;border-radius:14px;padding:18px 20px;margin-bottom:18px;}"
        << "table{width:100%;border-collapse:collapse;margin-top:10px;}"
        << "th,td{border:1px solid #dbe4ee;padding:8px 10px;text-align:left;vertical-align:top;}"
        << "th{background:#eff6ff;color:#1d4ed8;}"
        << ".meta{color:#475569;font-size:14px;}"
        << ".paths li{margin-bottom:4px;}"
        << "</style></head><body>";

    stream << "<h1>" << htmlEscaped(projectName) << "</h1>";
    stream << "<p class=\"meta\">Datasets: " << pointCloudPaths.size()
           << " | Towers: " << towers.size()
           << " | Issues: " << issues.size() << "</p>";

    stream << "<section><h2>Datasets</h2><ul class=\"paths\">";
    for (const QString& pointCloudPath : pointCloudPaths) {
        stream << "<li>" << htmlEscaped(QFileInfo(pointCloudPath).fileName())
               << "<br/><span class=\"meta\">" << htmlEscaped(pointCloudPath) << "</span></li>";
    }
    stream << "</ul></section>";

    stream << "<section><h2>Towers</h2><table><thead><tr>"
           << "<th>#</th><th>Name</th><th>Code</th><th>Line</th><th>Voltage</th><th>Type</th><th>Status</th><th>Inspection Date</th>"
           << "</tr></thead><tbody>";
    for (int towerIndex = 0; towerIndex < towers.size(); ++towerIndex) {
        const TowerRecord& tower = towers.at(towerIndex);
        stream << "<tr>"
               << "<td>" << (towerIndex + 1) << "</td>"
               << "<td>" << htmlEscaped(tower.name) << "</td>"
               << "<td>" << htmlEscaped(tower.code) << "</td>"
               << "<td>" << htmlEscaped(tower.lineName) << "</td>"
               << "<td>" << htmlEscaped(tower.voltageLevel) << "</td>"
               << "<td>" << htmlEscaped(tower.structureType) << "</td>"
               << "<td>" << htmlEscaped(tower.status) << "</td>"
               << "<td>" << htmlEscaped(tower.inspectionDate) << "</td>"
               << "</tr>";
    }
    stream << "</tbody></table></section>";

    stream << "<section><h2>Inspection Issues</h2><table><thead><tr>"
           << "<th>#</th><th>Title</th><th>Category</th><th>Severity</th><th>Status</th><th>Related Tower</th><th>Created At</th><th>Description</th>"
           << "</tr></thead><tbody>";
    for (int issueIndex = 0; issueIndex < issues.size(); ++issueIndex) {
        const InspectionIssue& issue = issues.at(issueIndex);
        stream << "<tr>"
               << "<td>" << (issueIndex + 1) << "</td>"
               << "<td>" << htmlEscaped(issue.title) << "</td>"
               << "<td>" << htmlEscaped(issue.category) << "</td>"
               << "<td>" << htmlEscaped(issueSeverityDisplayName(issue.severity)) << "</td>"
               << "<td>" << htmlEscaped(issueStatusDisplayName(issue.status)) << "</td>"
               << "<td>" << htmlEscaped(issue.relatedTowerName) << "</td>"
               << "<td>" << htmlEscaped(issue.createdAt) << "</td>"
               << "<td>" << htmlEscaped(issue.description) << "</td>"
               << "</tr>";
    }
    stream << "</tbody></table></section>";

    stream << "</body></html>";
    return true;
}
