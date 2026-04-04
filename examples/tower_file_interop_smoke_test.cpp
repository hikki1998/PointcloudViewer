#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>

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
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QList<TowerRecord> expectedTowers;
    TowerRecord tower0;
    tower0.index = 0;
    tower0.name = QStringLiteral("#001");
    tower0.point.x = 100.5f;
    tower0.point.y = 200.25f;
    tower0.point.z = 300.125f;
    tower0.towerType = TowerType::Unknown;
    expectedTowers.append(tower0);

    TowerRecord tower1;
    tower1.index = 1;
    tower1.name = QStringLiteral("#002");
    tower1.point.x = 110.5f;
    tower1.point.y = 210.25f;
    tower1.point.z = 310.125f;
    tower1.towerType = TowerType::Tangent;
    expectedTowers.append(tower1);

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return 1;
    }

    const QString towerPath = QDir(tempDir.path()).filePath(QStringLiteral("tower.LiTower"));
    QString errorMessage;
    if (!exportTowerLiTowerFile(towerPath, expectedTowers, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return 1;
    }
    if (!verify(QFile::exists(towerPath), "Exported tower file should exist")) {
        return 1;
    }

    QFile file(towerPath);
    if (!verify(file.open(QIODevice::ReadOnly | QIODevice::Text), "Exported tower file should be readable")) {
        return 1;
    }
    const QString firstLine = QString::fromUtf8(file.readLine()).trimmed();
    file.close();
    if (!verify(firstLine == QStringLiteral("Index,X,Y,Z,Type,Name"), "Tower file header should match LiTower format")) {
        return 1;
    }

    QList<TowerRecord> importedTowers;
    if (!importTowerLiTowerFile(towerPath, &importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] importTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return 1;
    }

    if (!verify(importedTowers.size() == expectedTowers.size(), "Imported tower count mismatch")) {
        return 1;
    }

    for (int index = 0; index < expectedTowers.size(); ++index) {
        const TowerRecord& expected = expectedTowers.at(index);
        const TowerRecord& actual = importedTowers.at(index);
        if (!verify(actual.index == expected.index, "Tower index mismatch")) {
            return 1;
        }
        if (!verify(actual.name == expected.name, "Tower name mismatch")) {
            return 1;
        }
        if (!verify(actual.towerType == expected.towerType, "Tower type mismatch")) {
            return 1;
        }
        if (!verify(std::fabs(actual.point.x - expected.point.x) < 1e-4f, "Tower X mismatch")) {
            return 1;
        }
        if (!verify(std::fabs(actual.point.y - expected.point.y) < 1e-4f, "Tower Y mismatch")) {
            return 1;
        }
        if (!verify(std::fabs(actual.point.z - expected.point.z) < 1e-4f, "Tower Z mismatch")) {
            return 1;
        }
    }

    std::cout << "[PASS] Tower file interop smoke test completed." << std::endl;
    return 0;
}
