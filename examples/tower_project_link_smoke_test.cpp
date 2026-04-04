#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTemporaryDir>

#include <iostream>

#include "domain/InspectionData.h"
#include "domain/TowerFileInterop.h"

namespace
{
bool verify(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << std::endl;
        return false;
    }
    return true;
}

void normalizeTowerIndices(QList<TowerRecord>* towers)
{
    if (towers == nullptr) {
        return;
    }
    for (int index = 0; index < towers->size(); ++index) {
        (*towers)[index].index = index;
    }
}

QString resolveProjectPath(const QString& projectFilePath, const QString& storedPath)
{
    if (storedPath.isEmpty()) {
        return QString();
    }
    const QFileInfo storedInfo(storedPath);
    if (storedInfo.isAbsolute()) {
        return storedPath;
    }
    return QFileInfo(QFileInfo(projectFilePath).absoluteDir(), storedPath).absoluteFilePath();
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString sourceTowerFilePath = QFileInfo(
        QDir::current().absoluteFilePath(QStringLiteral("templates/tower.LiTower"))).absoluteFilePath();
    if (!verify(QFileInfo::exists(sourceTowerFilePath), "templates/tower.LiTower should exist")) {
        return 1;
    }

    QList<TowerRecord> importedTowers;
    QString errorMessage;
    if (!importTowerLiTowerFile(sourceTowerFilePath, &importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] importTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return 1;
    }
    if (!verify(importedTowers.size() >= 4, "Expected at least 4 towers from template")) {
        return 1;
    }
    if (!verify(importedTowers.first().index == 44, "Template first index should be 44 before editing")) {
        return 1;
    }

    // Simulate an edit flow where table order is maintained and indices are normalized to 0..N-1.
    normalizeTowerIndices(&importedTowers);
    if (!verify(importedTowers.first().index == 0, "After edit normalization, first index should be 0")) {
        return 1;
    }
    if (!verify(importedTowers.last().index == importedTowers.size() - 1, "After edit normalization, last index should be N-1")) {
        return 1;
    }

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return 1;
    }

    const QString projectFilePath = QDir(tempDir.path()).filePath(QStringLiteral("tower_project.lpproj"));
    const QString linkedTowerFilePath = QDir(tempDir.path()).filePath(QStringLiteral("tower_linked.LiTower"));
    if (!exportTowerLiTowerFile(linkedTowerFilePath, importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile initial: " << errorMessage.toStdString() << std::endl;
        return 1;
    }

    QJsonArray towersArray;
    for (const TowerRecord& towerRecord : importedTowers) {
        towersArray.append(towerRecordToJson(towerRecord));
    }

    QJsonArray pointCloudFilesArray;
    pointCloudFilesArray.append(QStringLiteral("./test_data/ezhou_powerline_sample.las"));
    QJsonObject towerFileObject {
        { QStringLiteral("format"), QStringLiteral("LiTower") },
        { QStringLiteral("relativePath"), QStringLiteral("./tower_linked.LiTower") }
    };

    QJsonObject projectObject {
        { QStringLiteral("version"), 8 },
        { QStringLiteral("pointCloudFilePaths"), pointCloudFilesArray },
        { QStringLiteral("towerFile"), towerFileObject },
        { QStringLiteral("towerMarkers"), towersArray }
    };

    QFile projectFile(projectFilePath);
    if (!verify(projectFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Project file should be writable")) {
        return 1;
    }
    projectFile.write(QJsonDocument(projectObject).toJson(QJsonDocument::Indented));
    projectFile.close();

    QFile projectFileRead(projectFilePath);
    if (!verify(projectFileRead.open(QIODevice::ReadOnly), "Project file should be readable")) {
        return 1;
    }
    const QJsonDocument loadedDocument = QJsonDocument::fromJson(projectFileRead.readAll());
    projectFileRead.close();
    if (!verify(loadedDocument.isObject(), "Loaded project JSON must be an object")) {
        return 1;
    }

    const QJsonObject loadedProject = loadedDocument.object();
    const QString loadedRelativeTowerPath = loadedProject.value(QStringLiteral("towerFile")).toObject().value(QStringLiteral("relativePath")).toString();
    const QString resolvedTowerPath = resolveProjectPath(projectFilePath, loadedRelativeTowerPath);
    if (!verify(QFileInfo::exists(resolvedTowerPath), "Resolved linked tower file should exist")) {
        return 1;
    }

    QList<TowerRecord> loadedTowerRecords;
    const QJsonArray loadedTowersArray = loadedProject.value(QStringLiteral("towerMarkers")).toArray();
    for (const QJsonValue& towerValue : loadedTowersArray) {
        loadedTowerRecords.append(towerRecordFromJson(towerValue.toObject()));
    }
    if (!verify(loadedTowerRecords.size() == importedTowers.size(), "Loaded tower record count mismatch")) {
        return 1;
    }
    if (!verify(loadedTowerRecords.first().index == 0, "Loaded project first index should be 0")) {
        return 1;
    }

    // Simulate "save project" sync to linked tower file.
    if (!exportTowerLiTowerFile(resolvedTowerPath, loadedTowerRecords, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile sync: " << errorMessage.toStdString() << std::endl;
        return 1;
    }

    QFile linkedTowerFile(resolvedTowerPath);
    if (!verify(linkedTowerFile.open(QIODevice::ReadOnly | QIODevice::Text), "Linked tower file should be readable")) {
        return 1;
    }
    QTextStream stream(&linkedTowerFile);
    stream.setCodec("UTF-8");
    const QString header = stream.readLine().trimmed();
    const QString firstRow = stream.readLine().trimmed();
    linkedTowerFile.close();

    if (!verify(header == QStringLiteral("Index,X,Y,Z,Type,Name"), "Linked tower header should match LiTower format")) {
        return 1;
    }
    if (!verify(firstRow.startsWith(QStringLiteral("0,")), "First linked tower row should start with index 0")) {
        return 1;
    }

    std::cout << "[PASS] Tower project link smoke test completed." << std::endl;
    return 0;
}
